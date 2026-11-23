#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "dispatch_core.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace dispatch {
static std::string lower(std::string value) {
    for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
static std::string upper(std::string value) {
    for(auto& c:value)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
static std::string readText(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot read "+path.u8string());
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
Json readJson(const fs::path& path) {return Json::parse(readText(path));}
void writeJsonAtomic(const fs::path& path,const Json& data) {
    const auto temp=fs::path(path.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId()));
    try {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Cannot write "+path.u8string());
        out<<data.dump(2)<<'\n';out.close();
        if(!out)throw std::runtime_error("Could not finish writing "+path.u8string());
        if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace "+path.u8string()+" (Windows error "+std::to_string(GetLastError())+")");
    }catch(...){std::error_code ignored;fs::remove(temp,ignored);throw;}
}
State decodeState(const Json& data) {
    if(!data.is_object())throw std::runtime_error("The saved route is not a JSON object.");
    State state;
    for(size_t i=0;i<FieldCount;++i)state.values[i]=data.value(keys[i],std::string());
    if(!data.contains("start_city"))state.values[StartCity]=data.value("j1",std::string());
    if(!data.contains("d2_city"))state.values[MiddleCity]=data.value("d2",std::string());
    if(!data.contains("end_city"))state.values[FinalCity]=data.value("d1",std::string());
    state.stage=data.value("stage",std::string("setup"));
    if(state.stage!="setup"&&state.stage!="to_d2"&&state.stage!="to_d1"&&state.stage!="done")throw std::runtime_error("The saved route has an unknown stage: "+state.stage);
    return state;
}
Json encodeState(const State& state) {
    Json data=Json::object();for(size_t i=0;i<FieldCount;++i)data[keys[i]]=state.values[i];data["stage"]=state.stage;return data;
}
State loadState(const fs::path& root) {return fs::exists(root/"dispatch.json")?decodeState(readJson(root/"dispatch.json")):State{};}
Json jobRequest(const State& s) {
    return {{"game","ETS2"},{"origin_city",s.values[StartCity]},{"origin_company",s.values[PickupCompany]},{"cargo",s.values[Cargo]},{"destination_city",s.values[MiddleCity]},{"destination_company",s.values[MiddleCompany]},{"status","pending_game_adapter"}};
}
bool advance(State& state) {
    if(state.stage=="to_d2"){state.stage="to_d1";return true;}
    if(state.stage=="to_d1"){state.stage="done";return true;}
    return false;
}
std::string displayToken(std::string token) {
    bool capital=true;
    for(auto& c:token){if(c=='_')c=' ';const auto u=static_cast<unsigned char>(c);c=static_cast<char>(capital?std::toupper(u):std::tolower(u));capital=!std::isalpha(u);}
    return token;
}
double distance(const std::array<double,2>& a,const std::array<double,2>& b) {
    constexpr double rad=3.14159265358979323846/180.0;
    const double lat1=a[0]*rad,lat2=b[0]*rad,dlat=lat2-lat1,dlon=(b[1]-a[1])*rad;
    const double h=std::sin(dlat/2)*std::sin(dlat/2)+std::cos(lat1)*std::cos(lat2)*std::sin(dlon/2)*std::sin(dlon/2);
    return 6371*2*std::asin(std::min(1.0,std::sqrt(h)));
}
std::vector<std::string> suitableStops(const std::string& start,const std::string& end,const Catalog& catalog,const Coordinates& coords) {
    std::vector<std::string> result;
    if(!coords.count(start)||!coords.count(end))return result;
    const double direct=distance(coords.at(start),coords.at(end));if(direct<50)return result;
    std::vector<std::pair<double,std::string>> ranked;
    for(const auto& item:catalog) {
        const auto& city=item.first;if(city==start||city==end||!coords.count(city))continue;
        const double first=distance(coords.at(start),coords.at(city)),second=distance(coords.at(city),coords.at(end));
        const double extra=first+second-direct;
        if(first>30&&second>30&&extra<=std::max(120.0,direct*0.25))ranked.emplace_back(extra,city);
    }
    std::sort(ranked.begin(),ranked.end());for(const auto& city:ranked)result.push_back(city.second);return result;
}
Model::Model(fs::path path):root(std::move(path)) {
    if(fs::exists(root/"company_catalog.json"))catalog=readJson(root/"company_catalog.json").get<Catalog>();
    else refresh();
    if(fs::exists(root/"city_coordinates.json"))coords=readJson(root/"city_coordinates.json").get<Coordinates>();
}
fs::path Model::companyRoot() const {return root/"game_data"/"base"/"def"/"company";}
void Model::refresh() {
    if(!fs::is_directory(companyRoot()))throw std::runtime_error("Extracted ETS2 company definitions are missing. The existing catalog has been kept.");
    std::map<std::string,std::set<std::string>> found;
    const std::regex cityPattern(R"(^\s*city\s*:\s*([\w.]+))",std::regex_constants::icase|std::regex_constants::multiline);
    for(const auto& company:fs::directory_iterator(companyRoot())) {
        if(!company.is_directory())continue;const auto editor=company.path()/"editor";if(!fs::is_directory(editor))continue;
        for(const auto& file:fs::directory_iterator(editor)) {
            if(!file.is_regular_file()||lower(file.path().extension().u8string())!=".sii")continue;
            std::smatch match;const auto content=readText(file.path());
            if(std::regex_search(content,match,cityPattern))found[displayToken(match[1].str())].insert(upper(company.path().filename().u8string()));
        }
    }
    if(found.empty())throw std::runtime_error("No company locations were found. The existing catalog has been kept.");
    Catalog next;for(const auto& city:found)next[city.first]={city.second.begin(),city.second.end()};
    writeJsonAtomic(root/"company_catalog.json",Json(next));catalog=std::move(next);cargoCache.clear();
}
const std::set<std::string>& Model::cargoes(const std::string& company,const std::string& direction) {
    const auto key=std::make_pair(company,direction);const auto old=cargoCache.find(key);if(old!=cargoCache.end())return old->second;
    std::set<std::string> found;const auto folder=companyRoot()/lower(company)/direction;
    const std::regex pattern(R"(\bcargo\s*:\s*"?cargo\.([\w]+))",std::regex_constants::icase);
    if(fs::is_directory(folder))for(const auto& file:fs::directory_iterator(folder)) {
        if(!file.is_regular_file()||lower(file.path().extension().u8string())!=".sii")continue;
        const auto content=readText(file.path());
        for(auto i=std::sregex_iterator(content.begin(),content.end(),pattern);i!=std::sregex_iterator();++i)found.insert(displayToken((*i)[1].str()));
    }
    return cargoCache.emplace(key,std::move(found)).first->second;
}
std::vector<std::string> Model::choices(Field field,const State& s) {
    std::vector<std::string> result;
    if(field==StartCity||field==FinalCity){for(const auto& city:catalog)result.push_back(city.first);return result;}
    if(field==Cargo){const auto& out=cargoes(s.values[PickupCompany]);return {out.begin(),out.end()};}
    if(field==MiddleCity) {
        for(const auto& city:suitableStops(s.values[StartCity],s.values[FinalCity],catalog,coords)) {
            for(const auto& company:catalog.at(city))if(cargoes(company,"in").count(s.values[Cargo])){result.push_back(city);break;}
        }
        return result;
    }
    const auto cityField=field==PickupCompany?StartCity:field==FinalCompany?FinalCity:MiddleCity;
    const auto city=catalog.find(s.values[cityField]);if(city==catalog.end())return result;
    for(const auto& company:city->second)if(field==PickupCompany||s.values[Cargo].empty()||cargoes(company,"in").count(s.values[Cargo]))result.push_back(company);
    return result;
}
std::string Model::validate(const State& state) {
    if(state.stage!="setup")return "Start a new route before preparing another job.";
    for(size_t i=0;i<FieldCount;++i)if(state.values[i].empty())return "Choose every city, company and cargo before saving the route.";
    for(size_t i=0;i<FieldCount;++i){const auto allowed=choices(static_cast<Field>(i),state);if(std::find(allowed.begin(),allowed.end(),state.values[i])==allowed.end())return "This selection is unavailable or incompatible: "+state.values[i]+". Choose it again from the current list.";}
    return {};
}
void Model::save(const State& state) const {writeJsonAtomic(root/"dispatch.json",encodeState(state));}
void Model::start(State& state) {
    const auto error=validate(state);if(!error.empty())throw std::runtime_error(error);
    // Same pending-adapter format as the Python prototype. This file is NOT a
    // request to the live ATS DLL and never claims a game job was assigned.
    writeJsonAtomic(root/"game_job_request.json",jobRequest(state));
    auto next=state;next.stage="to_d2";save(next);state=std::move(next);
}
void Model::startSingle(State& state) {
    if(state.stage!="setup")throw std::runtime_error("Start a new route before preparing another job.");
    for(size_t i=0;i<=FinalCompany;++i){const auto allowed=choices(static_cast<Field>(i),state);if(state.values[i].empty()||std::find(allowed.begin(),allowed.end(),state.values[i])==allowed.end())throw std::runtime_error("Choose valid pickup, cargo and delivery details before saving.");}
    auto request=jobRequest(state);request["destination_city"]=state.values[FinalCity];request["destination_company"]=state.values[FinalCompany];writeJsonAtomic(root/"game_job_request.json",request);
    auto next=state;next.stage="to_d1";writeJsonAtomic(root/"dispatch_single.json",encodeState(next));state=std::move(next);
}
}
