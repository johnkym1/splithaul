#pragma once
#include "../dispatch_shared/protocol.h"
#include <map>
#include <set>
#include <vector>

namespace ats {
struct Choice {std::string id,label;};
struct Cargo {std::string label;std::set<std::string> bodies;double unitKg=0,unitM3=0;};
class Catalog {
public:
    explicit Catalog(const std::filesystem::path& project);
    live::Json snapshot() const;
    std::map<std::string,Cargo> cargo;
    std::map<std::string,std::string> cityNames,companyNames;
    std::map<std::string,std::set<std::string>> locations,outgoing,incoming;
    std::vector<Choice> sourceCities(const std::string& body) const;
    std::vector<Choice> companies(const std::string& city,const std::string& body,const std::string& selectedCargo,bool source) const;
    std::vector<Choice> cargoes(const std::string& company,const std::string& body) const;
    std::vector<Choice> destinationCities(const std::string& selectedCargo) const;
    bool compatible(const std::string& id,const std::string& body) const;
    bool valid(const live::Route& route,const std::string& body) const;
private:
    void loadSnapshot(const live::Json& data);
};
}
