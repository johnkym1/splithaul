#include "ats_catalog.h"
#include "dispatch_core.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <cmath>

namespace ats {
namespace fs=std::filesystem;
static std::string read(const fs::path& path) {std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read game definitions: "+path.u8string());return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
static std::string match(const std::string& value,const std::regex& pattern) {std::smatch m;return std::regex_search(value,m,pattern)?m[1].str():std::string();}
static void sort(std::vector<Choice>& choices) {std::sort(choices.begin(),choices.end(),[](const Choice& a,const Choice& b){return a.label==b.label?a.id<b.id:a.label<b.label;});}
static std::string name(const std::map<std::string,std::string>& names,const std::string& id) {const auto n=names.find(id);return n==names.end()?dispatch::displayToken(id):n->second;}
Catalog::Catalog(const fs::path& project) {
    const auto packed=project/"catalogs"/(std::string(dispatchGame::id)+".json");
    if(fs::exists(packed)){loadSnapshot(dispatch::readJson(packed));return;}
    std::vector<fs::path> roots;auto base=project/(dispatchGame::isETS2()?"game_data/ets2_def_current/def":"game_data/ats_def_current/def");if(dispatchGame::isETS2()&&!fs::is_directory(base))base=project/"game_data/base/def";if(!dispatchGame::isETS2()&&!fs::is_directory(base))base=project/"game_data/ats_def_route/def";if(fs::is_directory(base))roots.push_back(base);
    const auto dlcs=project/(dispatchGame::isETS2()?"game_data/ets2_dlc":"game_data/ats_dlc");
    if(fs::is_directory(dlcs)){std::vector<fs::path> more;for(const auto& d:fs::directory_iterator(dlcs))if(d.is_directory()&&fs::is_directory(d.path()/"def"))more.push_back(d.path()/"def");std::sort(more.begin(),more.end());roots.insert(roots.end(),more.begin(),more.end());}
    const std::regex cityName(R"rx(\bcity_name\s*:\s*"([^"]+)")rx"),country(R"rx(\bcountry\s*:\s*([a-z0-9_]+))rx"),location(R"rx(\bcity\s*:\s*([a-z0-9_]+))rx"),companyName(R"rx((?:^|\n)\s*name\s*:\s*"([^"]+)")rx");
    const std::regex cargoId(R"rx(\bcargo_data\s*:\s*cargo\.([a-z0-9_]+))rx"),bodyType(R"rx(\bbody_types\[\]\s*:\s*([a-z0-9_]+))rx"),cargoRef(R"rx(\bcargo\s*:\s*"?cargo\.([a-z0-9_]+))rx");
    for(const auto& root:roots) {
        if(fs::is_directory(root/"city"))for(const auto& f:fs::directory_iterator(root/"city"))if(f.is_regular_file()&&f.path().extension()==".sui") {
            const auto text=read(f.path()),id=f.path().stem().u8string();auto label=match(text,cityName);const auto state=match(text,country);
            if(label.empty()||label.rfind("@@",0)==0)label=dispatch::displayToken(id);
            if(!state.empty())label+=" ("+dispatch::displayToken(state)+")";cityNames[id]=label;
        }
        if(fs::is_directory(root/"cargo"))for(const auto& f:fs::directory_iterator(root/"cargo"))if(f.is_regular_file()&&(f.path().extension()==".sui"||f.path().extension()==".sii")) {
            const auto text=read(f.path()),id=match(text,cargoId);if(!live::identifier(id))continue;
            Cargo c;c.label=id=="empty_palet"?"Empty Pallets":dispatch::displayToken(id);
            const auto mass=match(text,std::regex(R"rx(\bmass\s*:\s*([0-9]+(?:\.[0-9]+)?))rx")),volume=match(text,std::regex(R"rx(\bvolume\s*:\s*([0-9]+(?:\.[0-9]+)?))rx"));
            if(!mass.empty())c.unitKg=std::stod(mass);if(!volume.empty())c.unitM3=std::stod(volume);
            for(auto m=std::sregex_iterator(text.begin(),text.end(),bodyType);m!=std::sregex_iterator();++m)c.bodies.insert((*m)[1].str());cargo[id]=std::move(c);
        }
        if(!fs::is_directory(root/"company"))continue;
        for(const auto& company:fs::directory_iterator(root/"company")) {
            if(company.is_regular_file()&&company.path().extension()==".sui") {
                auto label=match(read(company.path()),companyName);if(!label.empty()&&label.rfind("@@",0)!=0)companyNames[company.path().stem().u8string()]=label;continue;
            }
            if(!company.is_directory())continue;const auto id=company.path().filename().u8string();if(!live::identifier(id))continue;
            const auto editor=company.path()/"editor";
            if(fs::is_directory(editor))for(const auto& f:fs::directory_iterator(editor))if(f.is_regular_file()&&f.path().extension()==".sii"){const auto city=match(read(f.path()),location);if(live::identifier(city))locations[city].insert(id);}
            for(const auto& direction:{std::string("out"),std::string("in")}) {
                const auto dir=company.path()/direction;if(!fs::is_directory(dir))continue;
                for(const auto& f:fs::directory_iterator(dir))if(f.is_regular_file()&&f.path().extension()==".sii") {
                    const auto text=read(f.path());for(auto m=std::sregex_iterator(text.begin(),text.end(),cargoRef);m!=std::sregex_iterator();++m)(direction=="out"?outgoing:incoming)[id].insert((*m)[1].str());
                }
            }
        }
    }
    if(locations.empty()||cargo.empty())throw std::runtime_error("Game catalog data is missing. Rebuild the extracted game-data catalog.");
}
live::Json Catalog::snapshot() const {
    live::Json definitions=live::Json::object();
    for(const auto& item:cargo)definitions[item.first]={{"label",item.second.label},{"bodies",item.second.bodies},{"unit_kg",item.second.unitKg},{"unit_m3",item.second.unitM3}};
    return {{"format",1},{"game",dispatchGame::id},{"cargo",definitions},{"city_names",cityNames},{"company_names",companyNames},{"locations",locations},{"outgoing",outgoing},{"incoming",incoming}};
}
void Catalog::loadSnapshot(const live::Json& data) {
    if(!data.is_object()||data.at("format")!=1||data.at("game")!=dispatchGame::id)
        throw std::runtime_error("Packaged game catalog has the wrong format or game.");
    cityNames=data.at("city_names").get<decltype(cityNames)>();companyNames=data.at("company_names").get<decltype(companyNames)>();
    locations=data.at("locations").get<decltype(locations)>();outgoing=data.at("outgoing").get<decltype(outgoing)>();incoming=data.at("incoming").get<decltype(incoming)>();
    for(const auto& item:data.at("cargo").items()) {
        const auto& value=item.value();Cargo entry;
        entry.label=value.at("label").get<std::string>();entry.bodies=value.at("bodies").get<std::set<std::string>>();
        entry.unitKg=value.at("unit_kg").get<double>();entry.unitM3=value.at("unit_m3").get<double>();
        if(!live::identifier(item.key())||entry.label.empty()||!std::isfinite(entry.unitKg)||!std::isfinite(entry.unitM3)||entry.unitKg<0||entry.unitM3<0)
            throw std::runtime_error("Invalid cargo in packaged game catalog.");
        for(const auto& body:entry.bodies)if(!live::identifier(body))throw std::runtime_error("Invalid trailer body in packaged game catalog.");
        cargo.emplace(item.key(),std::move(entry));
    }
    for(const auto* table:{&locations,&outgoing,&incoming})for(const auto& entry:*table) {
        if(!live::identifier(entry.first))throw std::runtime_error("Invalid location or company in packaged game catalog.");
        for(const auto& id:entry.second)if(!live::identifier(id))throw std::runtime_error("Invalid reference in packaged game catalog.");
    }
    if(locations.empty()||cargo.empty())throw std::runtime_error("Packaged game catalog is empty.");
}
bool Catalog::compatible(const std::string& id,const std::string& body) const {const auto c=cargo.find(id);return c!=cargo.end()&&!body.empty()&&c->second.bodies.count(body)!=0;}
std::vector<Choice> Catalog::cargoes(const std::string& company,const std::string& body) const {
    std::vector<Choice> result;const auto found=outgoing.find(company);if(found!=outgoing.end())for(const auto& id:found->second)if(compatible(id,body))result.push_back({id,cargo.at(id).label});sort(result);return result;
}
std::vector<Choice> Catalog::companies(const std::string& city,const std::string& body,const std::string& selectedCargo,bool source) const {
    std::vector<Choice> result;const auto found=locations.find(city);if(found!=locations.end())for(const auto& id:found->second) {
        const auto accepted=incoming.find(id);if(source?!cargoes(id,body).empty():accepted!=incoming.end()&&accepted->second.count(selectedCargo))result.push_back({id,name(companyNames,id)});
    }
    std::map<std::string,size_t> counts;for(const auto& c:result)++counts[c.label];for(auto& c:result)if(counts[c.label]>1)c.label+=" ["+c.id+"]";sort(result);return result;
}
std::vector<Choice> Catalog::sourceCities(const std::string& body) const {
    std::vector<Choice> result;for(const auto& city:locations)if(!companies(city.first,body,"",true).empty())result.push_back({city.first,name(cityNames,city.first)});sort(result);return result;
}
std::vector<Choice> Catalog::destinationCities(const std::string& selectedCargo) const {
    std::vector<Choice> result;for(const auto& city:locations)if(!companies(city.first,"",selectedCargo,false).empty())result.push_back({city.first,name(cityNames,city.first)});sort(result);return result;
}
bool Catalog::valid(const live::Route& r,const std::string& body) const {
    const auto source=locations.find(r.sourceCity),target=locations.find(r.targetCity),out=outgoing.find(r.sourceCompany),in=incoming.find(r.targetCompany);
    return r.valid()&&compatible(r.cargo,body)&&source!=locations.end()&&source->second.count(r.sourceCompany)&&target!=locations.end()&&target->second.count(r.targetCompany)&&out!=outgoing.end()&&out->second.count(r.cargo)&&in!=incoming.end()&&in->second.count(r.cargo);
}
}
