#pragma once
#include "ats_catalog.h"
#include <cmath>

namespace ats {
inline std::vector<live::Route> automaticDrops(const Catalog& catalog,const live::Route& final,const std::string& body,unsigned count) {
    if(count<3||count>live::maximumStops||!catalog.valid(final,body))throw std::runtime_error("Choose pickup, cargo, final city and company before generating stops.");
    auto cities=catalog.firstStops(final.cargo,final.sourceCity,final.targetCity);
    cities.erase(std::remove_if(cities.begin(),cities.end(),[&](const Choice& city){return catalog.companies(city.id,body,final.cargo,false).empty();}),cities.end());
    if(cities.size()<count-1)throw std::runtime_error("Not enough compatible cities along this route. Choose fewer stops, another destination, or Manual selection.");
    const auto& start=catalog.positions.at(final.sourceCity);const auto& end=catalog.positions.at(final.targetCity);
    const double dx=end[0]-start[0],dz=end[1]-start[1],length=dx*dx+dz*dz;
    auto progress=[&](const Choice& city){const auto& p=catalog.positions.at(city.id);return ((p[0]-start[0])*dx+(p[1]-start[1])*dz)/length;};
    std::sort(cities.begin(),cities.end(),[&](const Choice& a,const Choice& b){const auto pa=progress(a),pb=progress(b);return pa==pb?a.id<b.id:pa<pb;});
    std::vector<live::Route> result;size_t begin=0;
    for(unsigned stop=1;stop<count;++stop) {
        const size_t last=cities.size()-(count-stop);size_t best=begin;
        for(size_t i=begin;i<=last;++i)if(std::fabs(progress(cities[i])-double(stop)/count)<std::fabs(progress(cities[best])-double(stop)/count))best=i;
        auto route=final;route.targetCity=cities[best].id;route.targetCompany=catalog.companies(route.targetCity,body,route.cargo,false).front().id;
        result.push_back(route);begin=best+1;
    }
    result.push_back(final);return result;
}
inline bool validDrops(const Catalog& catalog,const std::vector<live::Route>& drops,const std::string& body) {
    if(drops.size()<3||drops.size()>live::maximumStops)return false;
    std::set<std::string> cities;
    for(size_t i=0;i<drops.size();++i){const auto& route=drops[i];if(!catalog.valid(route,body)||route.cargo!=drops.front().cargo||route.source()!=drops.front().source()||(route.targetCity==route.sourceCity&&i+1!=drops.size())||!cities.insert(route.targetCity).second)return false;}
    return true;
}
// Scaled city-centre distances are planning estimates, never GPS road distances.
inline double cityScale(const Catalog& catalog,const std::string& city) {
    if(!dispatchGame::isETS2())return 20.0;
    const auto country=catalog.cityCountries.find(city);
    return country!=catalog.cityCountries.end()&&(country->second=="uk"||country->second=="united_kingdom")?15.0:19.0;
}
inline double cityKm(const Catalog& catalog,const std::string& a,const std::string& b) {
    const auto pa=catalog.positions.find(a),pb=catalog.positions.find(b);
    if(pa==catalog.positions.end()||pb==catalog.positions.end())return -1;
    return std::hypot(pa->second[0]-pb->second[0],pa->second[1]-pb->second[1])*(cityScale(catalog,a)+cityScale(catalog,b))/2000.0;
}
inline double tripKm(const Catalog& catalog,const std::vector<live::Route>& routes) {
    if(routes.empty())return -1;
    double total=0;auto previous=routes.front().sourceCity;
    for(const auto& route:routes){const auto leg=cityKm(catalog,previous,route.targetCity);if(leg<0)return -1;total+=leg;previous=route.targetCity;}return total;
}
struct TripChoices {
    std::array<std::vector<live::Route>,live::maximumStops+1> routes;
    std::set<std::string> cities;
    unsigned maximum=0;
    double ceilingKm=0;
    std::string message;
};
inline TripChoices availableTrips(const Catalog& catalog,const live::Route& final,const std::string& body,unsigned preferredKm) {
    TripChoices result;
    if(preferredKm>10000){result.message="Preferred distance must be between 0 and 10000 km.";return result;}
    if(!catalog.valid(final,body)){result.message="Choose pickup, cargo and final company. A return delivery needs a different company from the pickup.";return result;}
    const bool round=final.sourceCity==final.targetCity;
    const auto direct=cityKm(catalog,final.sourceCity,final.targetCity);
    if(direct<0){result.message="City coordinates are unavailable for this trip.";return result;}
    if(round&&!preferredKm){result.message="Enter a preferred distance for your round trip, such as 500 km.";return result;}
    result.ceilingKm=preferredKm?preferredKm*1.20:direct*1.25+120.0;
    if(direct>result.ceilingKm){result.message="The final city is beyond the preferred distance. Increase it or use 0 for a direct trip.";return result;}
    const auto candidates=preferredKm?catalog.destinationCities(final.cargo):catalog.firstStops(final.cargo,final.sourceCity,final.targetCity);
    std::vector<live::Route> stops;
    for(const auto& city:candidates) {
        if(city.id==final.sourceCity||city.id==final.targetCity)continue;
        const auto from=cityKm(catalog,final.sourceCity,city.id),to=cityKm(catalog,city.id,final.targetCity);
        if(from<0||to<0||from+to>result.ceilingKm)continue;
        const auto companies=catalog.companies(city.id,body,final.cargo,false);if(companies.empty())continue;
        auto route=final;route.targetCity=city.id;route.targetCompany=companies.front().id;stops.push_back(route);result.cities.insert(city.id);
    }
    if(stops.size()<2){result.message="Fewer than three compatible deliveries fit this route. Increase the distance, change cargo, or use Single/Dual.";return result;}
    // Bounded insertion search: retain both short routes (room for more stops)
    // and routes nearest the requested distance. Each advertised count has an
    // actual compatible itinerary, not just a count of points inside a radius.
    struct Path {std::vector<size_t> stops;double km=0;};
    std::vector<Path> beam={{{},direct}};
    auto distance=[&](int a,int b){return cityKm(catalog,a<0?final.sourceCity:stops[static_cast<size_t>(a)].targetCity,b<0?final.targetCity:stops[static_cast<size_t>(b)].targetCity);};
    auto score=[&](const Path& p){return preferredKm?std::fabs(p.km-preferredKm):p.km;};
    for(unsigned depth=1;depth<live::maximumStops&&!beam.empty();++depth) {
        std::vector<Path> next;std::set<std::vector<size_t>> seen;
        for(const auto& path:beam)for(size_t city=0;city<stops.size();++city) {
            if(std::find(path.stops.begin(),path.stops.end(),city)!=path.stops.end())continue;
            for(size_t at=0;at<=path.stops.size();++at) {
                const int before=at?static_cast<int>(path.stops[at-1]):-1,after=at<path.stops.size()?static_cast<int>(path.stops[at]):-1;
                const double km=path.km+distance(before,static_cast<int>(city))+distance(static_cast<int>(city),after)-distance(before,after);
                if(km>result.ceilingKm+0.001)continue;
                Path added=path;added.km=km;added.stops.insert(added.stops.begin()+static_cast<ptrdiff_t>(at),city);
                if(seen.insert(added.stops).second)next.push_back(std::move(added));
            }
        }
        if(next.empty())break;
        auto best=std::min_element(next.begin(),next.end(),[&](const Path& a,const Path& b){return score(a)==score(b)?a.stops<b.stops:score(a)<score(b);});
        if(depth>=2){auto& routes=result.routes[depth+1];for(auto city:best->stops)routes.push_back(stops[city]);routes.push_back(final);result.maximum=depth+1;}
        std::sort(next.begin(),next.end(),[](const Path& a,const Path& b){return a.km==b.km?a.stops<b.stops:a.km<b.km;});
        beam.assign(next.begin(),next.begin()+(std::min)(size_t(12),next.size()));
        std::sort(next.begin(),next.end(),[&](const Path& a,const Path& b){return score(a)==score(b)?a.stops<b.stops:score(a)<score(b);});
        for(size_t i=0;i<(std::min)(size_t(20),next.size());++i)beam.push_back(std::move(next[i]));
    }
    if(!result.maximum)result.message="Three deliveries cannot fit the estimated distance. Increase the distance or use Single/Dual.";
    else result.message="Up to "+std::to_string(result.maximum)+" compatible deliveries fit this route.";
    return result;
}
inline bool tripFits(const Catalog& catalog,const std::vector<live::Route>& routes,const std::string& body,const TripChoices& choices) {
    if(!validDrops(catalog,routes,body)||routes.size()>choices.maximum)return false;
    for(size_t i=0;i+1<routes.size();++i)if(!choices.cities.count(routes[i].targetCity))return false;
    const auto km=tripKm(catalog,routes);return km>=0&&km<=choices.ceilingKm+0.001;
}
}
