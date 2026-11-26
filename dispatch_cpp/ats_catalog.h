#pragma once
#include "../dispatch_shared/protocol.h"
#include <map>
#include <array>
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
    std::map<std::string,std::string> cityCountries;
    std::map<std::string,std::set<std::string>> depotPackages;
    bool filterDlcs=false;
    std::set<std::string> loadedDlcs;
    bool depotAvailable(const std::string& city,const std::string& company) const;
    std::map<std::string,std::set<std::string>> locations,outgoing,incoming;
    std::map<std::string,std::array<double,2>> positions;
    std::vector<Choice> sourceCities(const std::string& body) const;
    std::vector<Choice> companies(const std::string& city,const std::string& body,const std::string& selectedCargo,bool source) const;
    std::vector<Choice> cargoes(const std::string& company,const std::string& body) const;
    std::vector<Choice> destinationCities(const std::string& selectedCargo) const;
    std::vector<Choice> firstStops(const std::string& cargo,const std::string& source,const std::string& final) const;
    bool alongRoute(const std::string& source,const std::string& first,const std::string& final) const;
    std::string nearbyCity(double x,double z,const std::string& pickupBody="") const;
    bool compatible(const std::string& id,const std::string& body) const;
    bool valid(const live::Route& route,const std::string& body) const;
private:
    void loadSnapshot(const live::Json& data);
};
}
