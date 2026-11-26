#include "ats_catalog.h"
#include "dispatch_core.h"
#include "installed_storage.h"
#include <iostream>

static unsigned checks=0;
static void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>static bool rejected(F action){try{action();return false;}catch(const std::exception&){return true;}}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::runtime_error("Usage: CatalogPack.exe source-project package-directory");
    const dispatch::fs::path source=argv[1],package=argv[2];
    dispatch::fs::create_directories(package/"catalogs");
    for(bool ets:{false,true}){
        dispatchGame::select(ets);ats::Catalog original(source);
        const auto packed=original.snapshot();const auto file=package/"catalogs"/(std::string(dispatchGame::id)+".json");
        dispatch::writeJsonAtomic(file,packed);ats::Catalog restored(package);
        check(restored.snapshot()==packed,"Packaged catalog changed cargo, labels or compatibility tables");
        check(!restored.depotPackages.empty(),"Packaged catalog lost DLC ownership metadata");
        restored.filterDlcs=true;
        size_t blocked=0,base=0;
        for(const auto& depot:restored.depotPackages){const auto dot=depot.first.find('.');const auto city=depot.first.substr(0,dot),company=depot.first.substr(dot+1);const bool included=depot.second.count("base")!=0;check(restored.depotAvailable(city,company)==included,"Base-only filter leaked a DLC depot");if(included)++base;else ++blocked;}
        check(base>0&&blocked>0,"Catalog did not distinguish base and DLC depots");
        for(const auto& depot:restored.depotPackages)for(const auto& origin:depot.second)if(origin!="base")restored.loadedDlcs.insert(origin);
        for(const auto& depot:restored.depotPackages){const auto dot=depot.first.find('.');check(restored.depotAvailable(depot.first.substr(0,dot),depot.first.substr(dot+1)),"Loaded DLC depot missing");}
        if(ets){check(restored.locations.count("reykjavik")&&restored.positions.count("reykjavik")&&restored.positions.count("akureyri"),"Iceland cities or coordinates missing");restored.loadedDlcs.clear();for(const auto& company:restored.locations.at("reykjavik"))check(!restored.depotAvailable("reykjavik",company),"Iceland offered without its DLC");restored.loadedDlcs.insert("dlc_iceland");check(!restored.companies("reykjavik","curtainside","",true).empty(),"Iceland DLC did not unlock depots");}
        for(const auto& cargo:original.cargo)for(const auto& body:cargo.second.bodies)
            check(restored.compatible(cargo.first,body),"Packaged catalog lost trailer compatibility");
        auto invalid=packed;invalid["game"]=ets?"ats":"ets2";dispatch::writeJsonAtomic(file,invalid);
        check(rejected([&]{ats::Catalog wrong(package);}),"Cross-game catalog was accepted");
        invalid=packed;invalid["cargo"]=dispatch::Json::object();dispatch::writeJsonAtomic(file,invalid);
        check(rejected([&]{ats::Catalog empty(package);}),"Empty packaged cargo was accepted");
        invalid=packed;invalid["cargo"].begin().value()["unit_kg"]=-1;dispatch::writeJsonAtomic(file,invalid);
        check(rejected([&]{ats::Catalog negative(package);}),"Negative packaged cargo mass was accepted");
        invalid=packed;invalid["locations"]["../escape"]={"depot"};dispatch::writeJsonAtomic(file,invalid);
        check(rejected([&]{ats::Catalog invalidLocation(package);}),"Invalid packaged location was accepted");
        dispatch::writeJsonAtomic(file,packed);
        std::cout<<dispatchGame::id<<": "<<original.locations.size()<<" cities, "<<original.cargo.size()<<" cargo definitions\n";
    }
    dispatch::writeJsonAtomic(package/"installed-layout.json",{{"format",1}});
    // Exercise initial setup and upgrades in a disposable directory outside the payload.
    const auto test=package.parent_path()/("storage-test-"+live::uniqueId());
    const auto local=test/"local-app-data";
    check(installedStorage::userDataRoot(local)==local/"Grumpyman"/"SplitHaul","Fresh install did not use SplitHaul data folder");
    const auto legacy=local/"Grumpyman"/"DispatchCompanion";
    dispatch::fs::create_directories(legacy/"profiles"/"driver");
    dispatch::writeJsonAtomic(legacy/"profiles"/"driver"/"job_history.json",{{"preserve","existing history"}});
    check(installedStorage::userDataRoot(local)==legacy,"Rename hid existing user data");
    installedStorage::prepare(package,installedStorage::userDataRoot(local));
    check(dispatch::readJson(legacy/"profiles"/"driver"/"job_history.json").at("preserve")=="existing history","Rename modified driver history");
    installedStorage::prepare(package,test);
    check(dispatch::readJson(test/"company_catalog.json")==dispatch::Json::object(),"New install did not create neutral planner defaults");
    dispatch::writeJsonAtomic(test/"app_options.json",{{"mode",3},{"dark_theme",true}});
    dispatch::fs::create_directories(test/"profiles_ets2"/"TEST");
    dispatch::writeJsonAtomic(test/"profiles_ets2"/"TEST"/"job_history.json",{{"keep","history"}});
    dispatch::writeJsonAtomic(test/"company_catalog.json",{{"keep",dispatch::Json::array({"existing"})}});
    const auto beforeOptions=dispatch::readJson(test/"app_options.json"),beforeHistory=dispatch::readJson(test/"profiles_ets2"/"TEST"/"job_history.json"),beforePlanner=dispatch::readJson(test/"company_catalog.json");
    auto updated=dispatch::readJson(package/"catalogs"/"ats.json");updated["city_names"]["test_city"]="Updated catalog";
    const auto original=dispatch::readJson(package/"catalogs"/"ats.json");dispatch::writeJsonAtomic(package/"catalogs"/"ats.json",updated);
    installedStorage::prepare(package,test);
    check(dispatch::readJson(test/"catalogs"/"ats.json")==updated,"An update did not refresh the installed catalog");
    check(dispatch::readJson(test/"app_options.json")==beforeOptions,"An update reset user options");
    check(dispatch::readJson(test/"profiles_ets2"/"TEST"/"job_history.json")==beforeHistory,"An update reset delivery history");
    check(dispatch::readJson(test/"company_catalog.json")==beforePlanner,"An update reset existing planner data");
    dispatch::writeJsonAtomic(package/"catalogs"/"ats.json",original);
    std::cout<<"PASS: "<<checks<<" packaged catalog/storage checks.\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
