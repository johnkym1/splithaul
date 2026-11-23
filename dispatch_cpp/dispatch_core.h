#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <json.hpp>

namespace dispatch {
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Catalog=std::map<std::string,std::vector<std::string>>;
using Coordinates=std::map<std::string,std::array<double,2>>;
enum Field : size_t {StartCity,PickupCompany,Cargo,FinalCity,FinalCompany,MiddleCity,MiddleCompany,FieldCount};
inline const std::array<const char*,FieldCount> keys={"start_city","pickup_company","cargo","end_city","end_company","d2_city","d2_company"};
struct State {
    std::array<std::string,FieldCount> values{};
    std::string stage="setup";
};
Json readJson(const fs::path& path);
void writeJsonAtomic(const fs::path& path,const Json& data);
State decodeState(const Json& data);
Json encodeState(const State& state);
State loadState(const fs::path& root);
Json jobRequest(const State& state);
bool advance(State& state);
std::string displayToken(std::string token);
double distance(const std::array<double,2>& a,const std::array<double,2>& b);
std::vector<std::string> suitableStops(const std::string& start,const std::string& end,const Catalog& catalog,const Coordinates& coords);

class Model {
public:
    explicit Model(fs::path root);
    fs::path root;
    Catalog catalog;
    Coordinates coords;
    void refresh();
    const std::set<std::string>& cargoes(const std::string& company,const std::string& direction="out");
    std::vector<std::string> choices(Field field,const State& state);
    std::string validate(const State& state);
    void save(const State& state) const;
    void start(State& state);
    void startSingle(State& state);
private:
    std::map<std::pair<std::string,std::string>,std::set<std::string>> cargoCache;
    fs::path companyRoot() const;
};
}
