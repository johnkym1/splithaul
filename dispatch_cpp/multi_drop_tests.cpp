#include "multi_drop.h"
#include "job_history.h"
#include "app_options.h"
#include "dlc_filter.h"
#include "../native_job_bridge/two_stop_state.h"
#include <iostream>
static unsigned checks=0;
static void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class F>static bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected catalog and isolated test directory.");
        ats::Catalog catalog(argv[1]);catalog.positions.clear();catalog.locations.clear();
        for(unsigned i=0;i<10;++i){const auto city="city_"+std::to_string(i);catalog.positions[city]={double(i)*10000,0};catalog.locations[city]={"depot"};}
        catalog.outgoing={{"depot",{"wood"}}};catalog.incoming=catalog.outgoing;catalog.cargo["wood"]={"Wood",{"curtainside"},1000,1};
        live::Route final={"wood","city_0","depot","city_9","depot"};
        const auto root=live::fs::absolute(argv[2])/live::uniqueId(),bridge=root/"bridge";
        for(const auto& path:{root/"owned_requests",bridge/"plans",bridge/"results"})live::fs::create_directories(path);
        for(bool ets:{false,true}) {dispatchGame::select(ets);
            const auto direct=ats::availableTrips(catalog,final,"curtainside",0);
            check(direct.maximum==8,"Direct route lost feasible stops");
            check(ats::availableTrips(catalog,final,"curtainside",500).maximum==0,"Too-short distance accepted far destination");
            for(unsigned n=3;n<=direct.maximum;++n)check(ats::tripFits(catalog,direct.routes[n],"curtainside",direct),"Advertised count has no feasible itinerary");
            catalog.locations["city_0"].insert("return_depot");catalog.incoming["return_depot"]={"wood"};
            auto round=final;round.targetCity="city_0";round.targetCompany="return_depot";
            check(ats::availableTrips(catalog,round,"curtainside",0).maximum==0,"Unbounded round trip accepted");
            const auto shortRound=ats::availableTrips(catalog,round,"curtainside",500),longRound=ats::availableTrips(catalog,round,"curtainside",1500);
            check(shortRound.maximum==0&&longRound.maximum>=4,"Round-trip distance did not limit deliveries");
            check(ats::tripFits(catalog,longRound.routes[4],"curtainside",longRound),"Round trip not valid");
            live::Request loop;loop.id=live::uniqueId();loop.session=live::uniqueId();loop.context="context";loop.trailer="trailer";loop.created=live::utcMs();loop.expires=loop.created+120000;loop.dropRoutes=longRound.routes[4];loop.route=loop.dropRoutes.front();
            for(size_t i=0;i<loop.dropRoutes.size();++i)loop.dropIds.push_back(i?live::uniqueId():loop.id);
            check(live::parseRequest(live::requestJson(loop)).finalRoute.targetCity=="city_0","Protocol rejected return city");
            auto badLoop=live::requestJson(loop);badLoop["drops"][1]["route"]=live::routeJson(round);check(rejects([&]{live::parseRequest(badLoop);}),"Early return city accepted");
            badLoop=live::requestJson(loop);badLoop["drops"].back()["route"]["destination_company_id"]="depot";check(rejects([&]{live::parseRequest(badLoop);}),"Return to pickup depot accepted");
            auto noPosition=catalog;noPosition.positions.erase("city_0");check(ats::availableTrips(noPosition,round,"curtainside",1500).maximum==0,"Missing coordinates accepted");
            catalog.depotPackages.clear();for(const auto& city:catalog.locations)for(const auto& depot:city.second)catalog.depotPackages[city.first+"."+depot]={city.first=="city_0"?"base":"dlc_test"};
            catalog.filterDlcs=true;catalog.loadedDlcs.clear();check(catalog.sourceCities("curtainside").size()==1&&catalog.destinationCities("wood").size()==1&&!catalog.valid(final,"curtainside"),"DLC filter offered unavailable cities");
            const auto mounted=ats::mountedDlcs("[hashfs] dlc_unowned.scs: Created\n[fs] device C:/game/dlc_test.scs mounted to scs pool.\n[fs] device C:\\game\\dlc_other.scs mounted to scs pool.\n");
            check(mounted==std::set<std::string>({"dlc_test","dlc_other"}),"DLC parser used unmounted archives or missed Windows paths");catalog.loadedDlcs=mounted;
            check(catalog.valid(final,"curtainside")&&ats::availableTrips(catalog,final,"curtainside",0).maximum==8,"Loaded DLC did not restore routes");
            catalog.loadedDlcs.clear();check(ats::availableTrips(catalog,final,"curtainside",0).maximum==0,"Removed DLC left automatic stops");catalog.filterDlcs=false;
            for(unsigned count=3;count<=8;++count) {
                const auto routes=ats::automaticDrops(catalog,final,"curtainside",count);
                check(routes.size()==count&&ats::validDrops(catalog,routes,"curtainside"),"Automatic stops invalid");
                for(size_t i=1;i<routes.size();++i)check(catalog.positions.at(routes[i].targetCity)[0]>catalog.positions.at(routes[i-1].targetCity)[0],"Automatic route backtracks");
                auto invalid=routes;invalid[1]=invalid[0];check(!ats::validDrops(catalog,invalid,"curtainside"),"Duplicate delivery accepted");
                for(uint32_t units:{count,count+1,90u,101u}) {
                    uint32_t total=0,low=1000000,high=0;
                    for(unsigned i=0;i<count;++i){const auto amount=live::multiDropRemaining(units,count,i)-live::multiDropRemaining(units,count,i+1);total+=amount;low=(std::min)(low,amount);high=(std::max)(high,amount);}
                    check(total==units&&low>0&&high-low<=1,"Even split lost units or produced an empty delivery");
                }
                check(rejects([&]{live::multiDropRemaining(count-1,count,0);}),"Insufficient whole units accepted");
                live::Request request;request.id=live::uniqueId();request.session=live::uniqueId();request.context="context";request.trailer="triple";request.created=live::utcMs();request.expires=request.created+120000;request.route=routes.front();request.dropRoutes=routes;
                for(unsigned i=0;i<count;++i)request.dropIds.push_back(i?live::uniqueId():request.id);
                request=live::parseRequest(live::requestJson(request));check(request.twoStop&&request.secondId==request.dropIds.back(),"Multi-drop protocol roundtrip failed");
                auto bad=live::requestJson(request);bad["drops"][1]["request_id"]=request.id;check(rejects([&]{live::parseRequest(bad);}),"Repeated request identifier accepted");
                bad=live::requestJson(request);bad["protocol"]=4;check(rejects([&]{live::parseRequest(bad);}),"Old protocol accepted multi-drop");
                bad=live::requestJson(request);bad["drops"][1]["route"]["source_city_id"]="elsewhere";check(rejects([&]{live::parseRequest(bad);}),"Changed pickup accepted");
                live::TwoStopRun chain;chain.start(request);chain.firstUnits=101;chain.unitMassKg=1000;chain.finalUnits=live::multiDropRemaining(101,count,count-1);
                for(unsigned i=0;i<count;++i) {
                    check(chain.currentId==request.dropIds[i]&&chain.carriedUnits()==live::multiDropRemaining(101,count,i),"Wrong active cargo or delivery");
                    check(!chain.delivered(live::uniqueId()),"Unrelated delivery advanced the plan");
                    auto receipt=live::Json{{"protocol",live::protocolVersion},{"request_id",chain.currentId},{"session_id",request.session},{"state","delivered"},{"updated_utc_ms",live::utcMs()},{"route",live::routeJson(routes[i])},{"job",live::Json::object()}};
                    check(chain.delivered(chain.currentId),"Expected delivery did not advance");
                    check(!chain.delivered(chain.currentId),"Duplicate delivery advanced the plan");
                    live::writeJson(bridge/"results"/(chain.currentId+".json"),receipt);live::writeJson(root/"owned_requests"/(chain.currentId+".json"),{{"request_id",chain.currentId},{"session_id",request.session}});
                    if(i+1<count){check(chain.active()&&!ats::completedForm(true,request.id,request.session,chain.json(request.session),receipt),"Intermediate delivery completed the plan");check(!chain.canHandoff(false,true,true),"Active job permitted handoff");check(chain.canHandoff(true,true,true),"Ready handoff refused");chain.claimHandoff();check(!chain.canHandoff(true,true,true),"Handoff could be claimed twice");chain.state="second_active";}
                    else check(!chain.active()&&ats::verifiedDeliveryReady(true,request.id,request.session,"",chain.json(request.session),receipt),"Final receipt did not complete the plan");
                }
                live::writeJson(bridge/"plans"/(request.id+".json"),chain.json(request.session));
                ats::exportJobHistory(root,bridge,&catalog,true);const auto archive=live::readJson(root/"job_history.json");unsigned found=0;
                for(const auto& job:archive.at("jobs"))if(job.at("plan_id")==request.id){++found;const auto stop=job.at("stop").get<unsigned>();check(stop>=1&&stop<=count&&job.at("receipt").at("request_id")==request.dropIds[stop-1],"Incorrect history stop numbering");}
                check(found==count,"History lost a multi-drop delivery");
                chain.start(request);check(chain.delivered(request.id),"Cancellation fixture failed");check(!chain.canHandoff(true,false,true)&&!chain.active(),"Trailer change did not stop handoff");
            }
        }
        check(rejects([&]{ats::automaticDrops(catalog,final,"curtainside",9);}),"Excessive stop count accepted");
        catalog.positions.erase("city_1");catalog.positions.erase("city_2");check(rejects([&]{ats::automaticDrops(catalog,final,"curtainside",8);}),"Insufficient automatic cities accepted");
        std::cout<<"PASS: "<<checks<<" multi-drop protocol, routing, payload, handoff, completion and history checks.\n";return 0;
    }catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
