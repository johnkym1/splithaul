#include "job_history.h"
#include "dispatch_core.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace ats {
bool completedForm(bool twoStop,const std::string& id,const std::string& session,const live::Json& plan,const live::Json& result) {
    try {
        if(!live::requestId(id)||!live::requestId(session))return false;
        if(twoStop){
            if(plan.at("plan_id")!=id||plan.at("session_id")!=session)return false;
            if(plan.at("state")=="complete")return true;
            return plan.at("state")=="stopped"&&result.at("state")=="cancelled"&&result.at("session_id")==session&&result.at("request_id")==plan.at("active_request_id");
        }
        return result.at("request_id")==id&&result.at("session_id")==session&&(result.at("state")=="delivered"||result.at("state")=="cancelled");
    }catch(...){return false;}
}
bool awaitingAcknowledgement(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result) {
    return !id.empty()&&id!=cleared&&completedForm(twoStop,id,session,plan,result);
}
bool verifiedDeliveryReady(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result) {
    try {
        if(!awaitingAcknowledgement(twoStop,id,session,cleared,plan,result)||result.at("state")!="delivered"||result.at("session_id")!=session)return false;
        return result.at("request_id")== (twoStop?plan.at("second_request_id"):live::Json(id));
    }catch(...){return false;}
}
live::Json acknowledgeCompletedForm(const live::Json& saved,const live::Json& plan,const live::Json& result) {
    const auto id=saved.at("last_request_id").get<std::string>(),session=saved.at("session_id").get<std::string>();
    if(!completedForm(saved.value("two_stop",false),id,session,plan,result))throw std::runtime_error("This dispatch has not finished yet.");
    auto updated=saved;updated["form_cleared_for"]=id;updated["acknowledged_request_id"]=id;return updated;
}
bool canConfirmUnrecordedCompletion(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now) {
    try {
        if(!live::requestId(id)||!live::requestId(session)||id==cleared||!live::freshStatus(status,now)||status.at("session_id")==session||!status.at("can_queue").get<bool>()||status.at("job").at("active").get<bool>())return false;
        const auto phase=result.at("state").get<std::string>();
        if(result.at("session_id")!=session)return false;
        if(phase!="loaded"&&phase!="ended_unconfirmed"){
            // A single job may be saved/restarted before its loading event.
            // Only a confirmed, matching active-job snapshot establishes that
            // an assigned request actually became a game job.
            if(twoStop||phase!="assigned"||!result.at("job").at("active").get<bool>()||!live::parseRoute(result.at("route")).valid()||result.at("job").at("route")!=result.at("route"))return false;
        }
        if(!twoStop)return result.at("request_id")==id;
        return plan.at("plan_id")==id&&plan.at("session_id")==session&&plan.at("state")=="second_active"&&plan.at("active_request_id")==plan.at("second_request_id")&&result.at("request_id")==plan.at("second_request_id");
    }catch(...){return false;}
}
live::Json acknowledgeUnrecordedCompletion(const live::Json& saved,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now) {
    const auto id=saved.at("last_request_id").get<std::string>(),session=saved.at("session_id").get<std::string>();
    if(!canConfirmUnrecordedCompletion(saved.value("two_stop",false),id,session,saved.value("form_cleared_for",std::string()),plan,result,status,now))throw std::runtime_error("Completion review is unavailable. ATS must report no active job in the new game session.");
    auto updated=saved;updated["form_cleared_for"]=id;updated["acknowledged_request_id"]=id;
    updated["completion_acknowledgement"]={{"source","user_confirmed"},{"confirmed_utc_ms",now},{"observed_session_id",status.at("session_id")},{"automatic_final_delivery_receipt_available",false},{"action","Confirm delivery in history and reset form; original game receipt retained."}};
    return updated;
}
bool restartReviewDue(const live::Json& saved,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now) {
    try {
        if(!canConfirmUnrecordedCompletion(saved.value("two_stop",false),saved.at("last_request_id"),saved.at("session_id"),saved.value("form_cleared_for",std::string()),plan,result,status,now)||saved.value("completion_review_prompted_session",std::string())==status.at("session_id"))return false;
        if(!status.at("driving").get<bool>())return true;
        const auto speed=status.at("speed_mps").get<double>();return std::isfinite(speed)&&std::fabs(speed)<=0.1;
    }catch(...){return false;}
}
std::string csvText(const std::string& value) {
    std::string text=value;
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first!=std::string::npos&&std::string("=+-@").find(text[first])!=std::string::npos)text.insert(text.begin(),'\'');
    if(text.find_first_of(",\"\r\n")==std::string::npos)return text;
    std::string quoted="\"";for(const auto c:text){if(c=='\"')quoted+='\"';quoted+=c;}return quoted+'\"';
}
namespace {
std::string timestamp(uint64_t ms) {
    if(!ms||ms>253402300799999ULL)throw std::runtime_error("Invalid job completion timestamp.");
    ULARGE_INTEGER value;value.QuadPart=(ms+11644473600000ULL)*10000;FILETIME file={value.LowPart,value.HighPart};SYSTEMTIME time={};
    if(!FileTimeToSystemTime(&file,&time))throw std::runtime_error("Cannot format job completion timestamp.");
    std::ostringstream out;out<<std::setfill('0')<<std::setw(4)<<time.wYear<<'-'<<std::setw(2)<<time.wMonth<<'-'<<std::setw(2)<<time.wDay<<'T'<<std::setw(2)<<time.wHour<<':'<<std::setw(2)<<time.wMinute<<':'<<std::setw(2)<<time.wSecond<<'.'<<std::setw(3)<<time.wMilliseconds<<'Z';return out.str();
}
std::string numeric(const live::Json& data,const char* key) {
    const auto it=data.find(key);if(it==data.end()||!it->is_number())return {};
    return std::isfinite(it->get<double>())?it->dump():std::string();
}
std::string label(const std::map<std::string,std::string>* names,const std::string& id) {
    if(names){const auto found=names->find(id);if(found!=names->end())return found->second;}return dispatch::displayToken(id);
}
std::string readCsv(const live::fs::path& path) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE){if(GetLastError()==ERROR_FILE_NOT_FOUND)return {};throw std::runtime_error("Cannot read job_history.csv. Close it in other programs and retry.");}
    try{live::physicalPath(f,path);LARGE_INTEGER size;if(!GetFileSizeEx(f,&size)||size.QuadPart<0||size.QuadPart>64*1024*1024)throw std::runtime_error("Invalid job history size.");std::string data(static_cast<size_t>(size.QuadPart),'\0');DWORD read=0;if(!data.empty()&&(!ReadFile(f,data.data(),static_cast<DWORD>(data.size()),&read,nullptr)||read!=data.size()))throw std::runtime_error("Cannot read job history.");CloseHandle(f);return data;}catch(...){CloseHandle(f);throw;}
}
void writeCsv(const live::fs::path& path,const std::string& data) {
    const auto id=live::uniqueId();const auto temp=live::fs::path(path.wstring()+L"."+std::wstring(id.begin(),id.end())+L".tmp");
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create job history.");
    try{live::physicalPath(f,temp);DWORD written=0;if(!WriteFile(f,data.data(),static_cast<DWORD>(data.size()),&written,nullptr)||written!=data.size()||!FlushFileBuffers(f))throw std::runtime_error("Cannot finish job history.");CloseHandle(f);f=INVALID_HANDLE_VALUE;if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){const auto error=GetLastError();throw std::runtime_error("Cannot update "+path.filename().u8string()+" (Windows error "+std::to_string(error)+"). Close the CSV in other programs and check any protection alert.");}}
    catch(...){if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);DeleteFileW(temp.c_str());throw;}
}
struct Leg {std::string plan,session;unsigned stop=0;};
struct Row {uint64_t stamp=0;std::string id,session,line;};
struct Candidate {bool archived=false;live::Json receipt,confirmation=live::Json::object();};
}
void retainUserConfirmedDelivery(const live::fs::path& project,const live::fs::path& bridge,const live::Json& saved) {
    if(!saved.contains("completion_acknowledgement"))return;
    const auto confirmation=saved.at("completion_acknowledgement");
    if(confirmation.value("source",std::string())!="user_confirmed")return;
    const auto first=saved.at("last_request_id").get<std::string>(),session=saved.at("session_id").get<std::string>();const bool multiple=saved.value("two_stop",false);
    const auto id=multiple?saved.at("second_request_id").get<std::string>():first;
    if(!live::requestId(first)||!live::requestId(id)||!live::requestId(session)||saved.at("acknowledged_request_id")!=first||saved.at("form_cleared_for")!=first)throw std::runtime_error("Invalid driver-confirmed delivery identity.");
    timestamp(confirmation.at("confirmed_utc_ms").get<uint64_t>());
    const auto directory=project/"confirmed_deliveries",path=directory/(session+"-"+id+".json");
    if(live::fs::exists(path))return; // The durable original snapshot survives bridge cleanup.
    const auto receipt=live::readJson(bridge/"results"/(id+".json"));
    if(receipt.at("request_id")!=id||receipt.at("session_id")!=session||receipt.at("route")!=saved.at(multiple?"final_route":"route"))throw std::runtime_error("The confirmed delivery does not match its saved game receipt.");
    const auto phase=receipt.at("state").get<std::string>();if(phase!="loaded"&&phase!="assigned"&&phase!="ended_unconfirmed"&&phase!="delivered")throw std::runtime_error("This game receipt cannot be archived as a confirmed delivery.");
    live::fs::create_directories(directory);live::writeJson(path,live::Json{{"receipt",receipt},{"confirmation",confirmation},{"plan_id",multiple?first:std::string()},{"stop",multiple?2:0}},false);
}
HistoryExport exportJobHistory(const live::fs::path& project,const live::fs::path& bridge,const Catalog* catalog,bool profileScoped) {
    if(live::fs::exists(project/dispatchGame::savedDispatch))retainUserConfirmedDelivery(project,bridge,live::readJson(project/dispatchGame::savedDispatch));
    std::map<std::string,Leg> legs;
    if(live::fs::exists(bridge/"plans"))for(const auto& file:live::fs::directory_iterator(bridge/"plans"))try {
        if(!file.is_regular_file()||file.path().extension()!=L".json")continue;
        const auto p=live::readJson(file.path());const auto id=p.at("plan_id").get<std::string>(),session=p.at("session_id").get<std::string>(),first=p.at("first_request_id").get<std::string>(),second=p.at("second_request_id").get<std::string>();
        if(!live::requestId(id)||!live::requestId(session)||!live::requestId(first)||!live::requestId(second)||id!=first||first==second)continue;
        legs[first]={id,session,1};legs[second]={id,session,2};
    }catch(const std::exception&){}
    const auto archivePath=project/"job_history.json";live::Json previous={{"version",1},{"jobs",live::Json::array()}};
    std::vector<Candidate> candidates;
    if(live::fs::exists(archivePath)){
        previous=live::readJson(archivePath,64*1024*1024);if(previous.at("version")!=1||!previous.at("jobs").is_array())throw std::runtime_error("Saved job history is invalid; it has not been overwritten.");
        for(const auto& saved:previous.at("jobs")){
            const auto& r=saved.at("receipt");const auto id=r.at("request_id").get<std::string>(),session=r.at("session_id").get<std::string>(),plan=saved.value("plan_id",std::string());const auto stop=saved.value("stop",0u);
            if(!plan.empty()&&live::requestId(plan)&&(stop==1||stop==2))legs.emplace(id,Leg{plan,session,stop});
            candidates.push_back({true,r,saved.value("confirmation",live::Json::object())});
        }
    }
    if(live::fs::exists(bridge/"results"))for(const auto& file:live::fs::directory_iterator(bridge/"results"))try {
        if(!file.is_regular_file()||file.path().extension()!=L".json")continue;const auto r=live::readJson(file.path());
        const auto id=r.at("request_id").get<std::string>();if(!live::requestId(id)||file.path().stem()!=live::fs::path(id))continue;
        if(profileScoped){const auto ownership=live::readJson(project/"owned_requests"/(id+".json"));if(ownership.at("request_id")!=id||ownership.at("session_id")!=r.at("session_id"))continue;}
        candidates.push_back({false,r,live::Json::object()});
    }catch(const std::exception&){}
    if(live::fs::exists(project/"confirmed_deliveries"))for(const auto& file:live::fs::directory_iterator(project/"confirmed_deliveries")){
        if(!file.is_regular_file()||file.path().extension()!=L".json")continue;
        const auto saved=live::readJson(file.path());const auto& receipt=saved.at("receipt");const auto id=receipt.at("request_id").get<std::string>(),session=receipt.at("session_id").get<std::string>(),plan=saved.value("plan_id",std::string());const auto stop=saved.value("stop",0u);
        if(file.path().stem()!=live::fs::path(session+"-"+id))throw std::runtime_error("Driver-confirmed delivery filename does not match its identity.");
        if(!plan.empty()&&live::requestId(plan)&&stop==2)legs[id]={plan,session,stop};
        candidates.push_back({true,receipt,saved.at("confirmation")});
    }
    std::map<std::pair<std::string,std::string>,Row> rows;
    std::map<std::pair<std::string,std::string>,live::Json> records;
    for(const auto& candidate:candidates)try {
        const auto& r=candidate.receipt;const auto state=r.at("state").get<std::string>();const bool driverConfirmed=!candidate.confirmation.empty();
        if(driverConfirmed&&(candidate.confirmation.at("source")!="user_confirmed"||(state!="assigned"&&state!="loaded"&&state!="ended_unconfirmed"&&state!="delivered")))throw std::runtime_error("Invalid driver confirmation.");
        if(state!="delivered"&&state!="cancelled"&&!driverConfirmed){if(candidate.archived)throw std::runtime_error("Unfinished receipt in archive.");continue;}
        const auto id=r.at("request_id").get<std::string>(),session=r.at("session_id").get<std::string>();const auto version=r.at("protocol").get<int>();
        if(version<2||version>live::protocolVersion||!live::requestId(id)||!live::requestId(session))throw std::runtime_error("Invalid receipt identity.");
        const auto route=live::parseRoute(r.at("route"));if(!route.valid())throw std::runtime_error("Invalid receipt route.");
        const auto stamp=driverConfirmed?candidate.confirmation.at("confirmed_utc_ms").get<uint64_t>():r.at("updated_utc_ms").get<uint64_t>();const auto when=timestamp(stamp);
        auto cargoName=dispatch::displayToken(route.cargo);if(catalog&&catalog->cargo.count(route.cargo))cargoName=catalog->cargo.at(route.cargo).label;
        const auto cities=catalog?&catalog->cityNames:nullptr,companies=catalog?&catalog->companyNames:nullptr;
        auto job=r.value("job",live::Json::object()),details=r.value("details",live::Json::object());if(!job.is_object())job=live::Json::object();if(!details.is_object())details=live::Json::object();
        std::string mass,units;if(job.value("payload_known",false)&&job.value("cargo_mass_kg",0.0)>0&&job.value("cargo_units",0u)>0){mass=numeric(job,"cargo_mass_kg");units=numeric(job,"cargo_units");}
        std::string plan,stop;const auto leg=legs.find(id);if(leg!=legs.end()&&leg->second.session==session){plan=leg->second.plan;stop=std::to_string(leg->second.stop);}
        if(driverConfirmed)details=live::Json::object(); // Assignment/navigation details are not delivery measurements.
        const std::vector<std::string> cells={when,driverConfirmed?"delivered_user_confirmed":state,csvText(cargoName),csvText(label(cities,route.sourceCity)),csvText(label(companies,route.sourceCompany)),csvText(label(cities,route.targetCity)),csvText(label(companies,route.targetCompany)),mass,units,numeric(details,"revenue"),numeric(details,"distance.km"),numeric(details,"cargo.damage"),plan,stop,id,driverConfirmed?"Driver confirmation":"Game telemetry"};
        std::string line;for(size_t i=0;i<cells.size();++i){if(i)line+=',';line+=cells[i];}line+="\r\n";
        const auto key=std::make_pair(session,id);const auto old=rows.find(key);if(old!=rows.end()){
            const bool oldManual=records.at(key).contains("confirmation");if(driverConfirmed&&!oldManual)continue;
            if(driverConfirmed==oldManual&&old->second.stamp>stamp)continue;
        }
        rows[key]={stamp,id,session,line};records[key]={{"receipt",r},{"plan_id",plan},{"stop",stop.empty()?0u:static_cast<unsigned>(std::stoul(stop))}};if(driverConfirmed)records[key]["confirmation"]=candidate.confirmation;
    }catch(const std::exception&){if(candidate.archived)throw std::runtime_error("A saved job history entry is invalid; the archive has not been overwritten.");}
    std::vector<Row> ordered;for(auto& item:rows)ordered.push_back(std::move(item.second));
    std::sort(ordered.begin(),ordered.end(),[](const Row& a,const Row& b){return std::tie(a.stamp,a.session,a.id)<std::tie(b.stamp,b.session,b.id);});
    std::string data="\xEF\xBB\xBF" "Completed / confirmed (UTC),Status,Cargo,Pickup city,Pickup company,Delivery city,Delivery company,Cargo weight (kg),Cargo units,Revenue,Distance (km),Cargo damage,Plan ID,Stop,Request ID,Completion source\r\n";
    for(const auto& row:ordered)data+=row.line;
    live::Json archive={{"version",1},{"jobs",live::Json::array()}};for(const auto& record:records)archive["jobs"].push_back(record.second);
    if(archive!=previous)live::writeJson(archivePath,archive);
    const auto path=project/(profileScoped?"user.data":"job_history.csv");const bool changed=readCsv(path)!=data;if(changed)writeCsv(path,data);return {ordered.size(),changed};
}
std::vector<HistoryEntry> readHistoryEntries(const live::fs::path& project,const Catalog* catalog) {
    std::vector<HistoryEntry> entries;if(!live::fs::exists(project/"job_history.json"))return entries;
    const auto archive=live::readJson(project/"job_history.json",64*1024*1024);if(archive.at("version")!=1||!archive.at("jobs").is_array())throw std::runtime_error("Saved job history is invalid.");
    const auto optionalNumber=[](const live::Json& data,const char* key)->std::optional<double>{if(!data.is_object())return {};const auto value=data.find(key);if(value==data.end()||!value->is_number())return {};const auto number=value->get<double>();return std::isfinite(number)?std::optional<double>(number):std::nullopt;};
    for(const auto& saved:archive.at("jobs")){
        const auto& receipt=saved.at("receipt");HistoryEntry entry;entry.id=receipt.at("request_id");entry.session=receipt.at("session_id");
        if(!live::requestId(entry.id)||!live::requestId(entry.session))throw std::runtime_error("Invalid history identity.");
        const auto route=live::parseRoute(receipt.at("route"));if(!route.valid())throw std::runtime_error("Invalid history route.");
        entry.driverConfirmed=saved.contains("confirmation");const auto state=receipt.at("state").get<std::string>();
        if(entry.driverConfirmed){const auto& confirmation=saved.at("confirmation");if(confirmation.at("source")!="user_confirmed")throw std::runtime_error("Unknown delivery confirmation source.");entry.completedMs=confirmation.at("confirmed_utc_ms");}
        else {if(state!="delivered"&&state!="cancelled")throw std::runtime_error("Unfinished job in history.");entry.completedMs=receipt.at("updated_utc_ms");}
        timestamp(entry.completedMs);entry.outcome=state=="cancelled"?"Cancelled":"Delivered";entry.source=entry.driverConfirmed?"Driver confirmed":"Game telemetry";
        entry.cargo=catalog&&catalog->cargo.count(route.cargo)?catalog->cargo.at(route.cargo).label:dispatch::displayToken(route.cargo);
        const auto cities=catalog?&catalog->cityNames:nullptr,companies=catalog?&catalog->companyNames:nullptr;
        entry.pickupCity=label(cities,route.sourceCity);entry.pickupCompany=label(companies,route.sourceCompany);entry.deliveryCity=label(cities,route.targetCity);entry.deliveryCompany=label(companies,route.targetCompany);
        const auto job=receipt.value("job",live::Json::object());
        if(job.is_object()&&job.value("payload_known",false)&&job.value("cargo_mass_kg",0.0)>0&&job.value("cargo_units",0u)>0){entry.massKg=optionalNumber(job,"cargo_mass_kg");entry.units=job.at("cargo_units").get<uint32_t>();}
        if(!entry.driverConfirmed){const auto details=receipt.value("details",live::Json::object());entry.revenue=optionalNumber(details,"revenue");entry.distanceKm=optionalNumber(details,"distance.km");entry.damage=optionalNumber(details,"cargo.damage");}
        entry.plan=saved.value("plan_id",std::string());entry.stop=saved.value("stop",0u);entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const HistoryEntry& a,const HistoryEntry& b){return std::tie(a.completedMs,a.session,a.id)>std::tie(b.completedMs,b.session,b.id);});return entries;
}
void saveHistoryCsvCopy(const live::fs::path& project,const live::fs::path& destination,bool profileScoped) {
    const auto data=readCsv(project/(profileScoped?"user.data":"job_history.csv"));if(data.empty())throw std::runtime_error("There is no generated history CSV to export. Refresh History and try again.");
    writeCsv(destination,data);
}
}
