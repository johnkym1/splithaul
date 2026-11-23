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
