#include "ats_catalog.h"
#include "../native_job_bridge/live_state.h"
#include "../native_job_bridge/menu_snapshot.h"
#include "../native_job_bridge/host_builds.h"
#include "../native_job_bridge/pickup_navigation.h"
#include "../native_job_bridge/two_stop_state.h"
#include "../dispatch_shared/app_launcher.h"
#include "../dispatch_shared/startup_diagnostic.h"
#include "../dispatch_shared/cancel_protocol.h"
#include "cancel_hint.h"
#include <limits>
#include <iostream>

static unsigned count=0;
static void check(bool ok,const char* message){++count;if(!ok)throw std::runtime_error(message);}
template<class F>static bool rejects(F action){try{action();return false;}catch(const std::exception&){return true;}}
static scs_named_value_t stringValue(const char* name,const char* value){scs_named_value_t a={};a.name=name;a.value.type=SCS_VALUE_TYPE_string;a.value.value_string.value=value;return a;}
int wmain(int argc,wchar_t** argv) {
    try {
        live::MenuSnapshot menu;menu.pause(10000,9900,0.0f,9900,false);
        check(menu.stopped()&&menu.ignitionKnown&&!menu.ignitionOn,"Stopped menu snapshot was not captured");
        menu.pause(10000,9900,8.0f,9900,true);check(!menu.stopped()&&menu.ignitionKnown&&menu.ignitionOn,"Moving truck or enabled ignition was misreported");
        menu.pause(10000,7000,0.0f,7000,false);check(!menu.stopped()&&!menu.ignitionKnown,"Stale samples were treated as current");
        menu.pause(10000,9900,std::numeric_limits<float>::quiet_NaN(),9900,false);check(!menu.stopped(),"Invalid speed was treated as stopped");
        menu.beginDriving();check(!menu.captured&&!menu.speedKnown&&!menu.ignitionKnown,"Menu snapshot survived returning to Drive");
        live::CancelRequest stop;stop.id=live::uniqueId();stop.session=live::uniqueId();stop.context="world:player:trailer";stop.profileId="4A6F686E";stop.target="job:revision";stop.created=live::utcMs();stop.expires=stop.created+20000;
        const auto parsedStop=live::parseCancelRequest(live::cancelRequestJson(stop));
        check(live::currentCancelRequest(parsedStop,stop.session,stop.created),"Current cancellation request was rejected");
        check(!live::currentCancelRequest(parsedStop,live::uniqueId(),stop.created),"Cancellation request was accepted for another game session");
        check(!live::currentCancelRequest(parsedStop,stop.session,stop.expires),"Expired cancellation request was accepted");
        auto malformedStop=live::cancelRequestJson(stop);malformedStop["game"]="ets2";check(rejects([&]{live::parseCancelRequest(malformedStop);}),"ATS cancellation accepted a different game");
        malformedStop=live::cancelRequestJson(stop);malformedStop["request_id"]="../invalid";check(rejects([&]{live::parseCancelRequest(malformedStop);}),"Invalid cancellation identifier was accepted");
        malformedStop=live::cancelRequestJson(stop);malformedStop["profile_id"]="../bad";check(rejects([&]{live::parseCancelRequest(malformedStop);}),"Invalid profile identifier was accepted");
        malformedStop=live::cancelRequestJson(stop);malformedStop["context_id"]="";check(rejects([&]{live::parseCancelRequest(malformedStop);}),"Missing game context was accepted");
        malformedStop=live::cancelRequestJson(stop);malformedStop["target_token"]="";check(rejects([&]{live::parseCancelRequest(malformedStop);}),"Missing cancellation target was accepted");
        const auto dispatchId=live::uniqueId(),dispatchSession=live::uniqueId();
        const live::Json savedDispatch={{"last_request_id",dispatchId},{"session_id",dispatchSession},{"route",{{"cargo_id","wood"}}}};
        const live::Json pendingStop={{"request_id",stop.id},{"session_id",stop.session},{"dispatch_id",dispatchId},{"dispatch_session_id",dispatchSession}};
        live::Json stopReply={{"protocol",live::protocolVersion},{"game","ats"},{"request_id",stop.id},{"session_id",stop.session},{"outcome","game_requested"}};
        const auto cleaned=live::applyCancelCleanup(savedDispatch,pendingStop,stopReply,stop.created);
        check(cleaned.at("form_cleared_for")==dispatchId&&cleaned.at("cancel_cleanup").at("outcome")=="game_requested"&&!cleaned.contains("completion_acknowledgement")&&cleaned.at("route")==savedDispatch.at("route"),"Unconfirmed cancellation fabricated delivery or lost original data");
        auto timeoutReply=stopReply;timeoutReply["outcome"]="contact_lost";const auto timedOut=live::applyCancelCleanup(savedDispatch,pendingStop,timeoutReply,stop.created+35001);
        check(timedOut.at("cancel_cleanup").at("outcome")=="contact_lost"&&!timedOut.contains("completion_acknowledgement"),"Lost contact was misreported as a confirmed game cancellation");
        auto badStopReply=stopReply;badStopReply["outcome"]="rejected_state";check(rejects([&]{live::applyCancelCleanup(savedDispatch,pendingStop,badStopReply,stop.created);}),"Rejected cancellation cleared the dispatch");
        badStopReply=stopReply;badStopReply["session_id"]=live::uniqueId();check(rejects([&]{live::applyCancelCleanup(savedDispatch,pendingStop,badStopReply,stop.created);}),"Other game session cleared the dispatch");
        auto changedDispatch=savedDispatch;changedDispatch["last_request_id"]=live::uniqueId();check(rejects([&]{live::applyCancelCleanup(changedDispatch,pendingStop,stopReply,stop.created);}),"A changed dispatch was cleared by a stale response");
        live::Json hintStatus={{"experimental_cancel_available",true},{"driving",false},{"context_id","context"},{"cancel_target_token","target"},{"menu_snapshot",{{"captured",true},{"speed_mps",0.0},{"stopped",true},{"ignition_on",false}}},{"job",{{"active",true}}}};
        check(ats::cancelUnavailableReason(true,true,false,hintStatus,false).empty(),"Ready cancellation was incorrectly disabled");
        auto hintChanged=hintStatus;hintChanged["driving"]=true;check(ats::cancelUnavailableReason(true,true,false,hintChanged,false).find("menu")!=std::string::npos,"Drive-mode hint did not explain opening the menu");
        hintChanged=hintStatus;hintChanged["menu_snapshot"]["stopped"]=false;check(ats::cancelUnavailableReason(true,true,false,hintChanged,false).find("Stop")!=std::string::npos,"Moving-truck hint did not explain stopping");
        hintChanged=hintStatus;hintChanged["menu_snapshot"]["ignition_on"]=nullptr;check(ats::cancelUnavailableReason(true,true,false,hintChanged,false).find("Ignition")!=std::string::npos,"Unknown ignition did not get an explanation");
        check(ats::cancelUnavailableReason(false,true,false,hintStatus,false).find("Connect")!=std::string::npos,"Offline cancellation did not get an explanation");
        if(argc!=3)throw std::runtime_error("Expected project and isolated test directory.");
        check(dispatchLauncher::shortcutModifiers==(MOD_SHIFT|MOD_NOREPEAT)&&dispatchLauncher::shortcutKey==VK_F9&&std::string(dispatchLauncher::shortcutLabel)=="Shift+F9","Shortcut chord and displayed label disagree");
        for(unsigned modifiers=0;modifiers<16;++modifiers){
            check(dispatchLauncher::shortcutChord(true,modifiers)==(modifiers==MOD_SHIFT),"Shortcut accepted wrong modifiers or rejected Shift alone");
            check(!dispatchLauncher::shortcutChord(false,modifiers),"Shortcut activated without F9");
        }
        check(selectHost(hostBuilds[0].hash)&&handlerRva==0x5bebc0&&routeResetRva==0x5fed20&&finishLoadingRva==0x5ffeb0&&capacityRva==0x83ef50&&companyCityRva==0x7bc880,"Previously verified build selection failed");
        check(selectHost(hostBuilds[1].hash)&&handlerRva==0x5c00d0&&acceptRva==0x78a700&&registryRva==0x371bdf8&&companyVtableRva==0x2301560&&routeResetRva==0x600230&&finishLoadingRva==0x6013c0&&capacityRva==0x8403d0&&companyCityRva==0x7bdd00,"ATS 1.62.2 build selection failed");
        check(selectHost(hostBuilds[2].hash)&&registryRva==0x371be58&&economyRva==0x30b1680&&mainGameRva==0x371bd28&&handlerRva==0x5c0110&&lookupRva==0x12e7b0&&acceptRva==0x78a730&&companyVtableRva==0x2301560&&cargoOfferVtableRva==0x23532f8&&routeResetRva==0x600260&&finishLoadingRva==0x6013f0&&capacityRva==0x840400&&companyCityRva==0x7bdd30,"ATS 1.62.2.2 build selection failed");
        check(!selectHost("unsupported")&&handlerRva==0&&routeResetRva==0&&finishLoadingRva==0&&capacityRva==0&&companyCityRva==0&&!activeHostHash,"Unknown executable retained native addresses");
        live::Json startup={{"protocol",live::protocolVersion},{"game","ats"},{"online",false},{"state","startup_rejected"},{"pid",1234},{"updated_utc_ms",20000},{"message","Game update requires a compatible DLL."}};
        check(live::startupDiagnostic(startup,1234,10000,30000)=="Game update requires a compatible DLL.","Current ATS startup failure was hidden");
        check(live::startupDiagnostic(startup,1234,25000,30000).empty(),"An earlier process's startup error was reused after PID recycling");
        check(live::startupDiagnostic(startup,4321,10000,30000).empty(),"Another process's startup error was shown");
        check(live::startupDiagnostic(startup,1234,10000,10000).empty(),"Future startup diagnostics were accepted");
        auto badStartup=startup;badStartup["online"]=true;check(live::startupDiagnostic(badStartup,1234,10000,30000).empty(),"Online status was mistaken for a startup failure");
        badStartup=startup;badStartup["message"]=nullptr;check(live::startupDiagnostic(badStartup,1234,10000,30000).empty(),"Null startup message was accepted");
        badStartup=startup;badStartup["message"]=std::string(1025,'x');check(live::startupDiagnostic(badStartup,1234,10000,30000).empty(),"Unbounded startup diagnostics were accepted");
        check(live::startupDiagnostic(startup,0,0,30000).empty(),"A missing process identity accepted a startup error");
        live::TrailerCapacity cap;cap.grossKg=32700;cap.chassisKg=6409.2998f;cap.bodyKg=998;cap.volumeM3=112.56f;cap.axles=3;
        check(cap.valid()&&std::fabs(cap.payloadKg()-25292.7002)<0.01,"Trailer payload ceiling included trailer tare");
        check(cap.maximumUnits(522.1,2.6)==43,"Volume-limited Batteries maximum was incorrect");
        check(cap.maximumUnits(1000,1)==25,"Mass-limited cargo exceeded the payload ceiling");
        check(cap.maximumUnits(1,200)==0,"A cargo unit larger than the trailer was accepted");
        check(cap.maximumUnits(0,2)==0&&cap.maximumUnits(500,0)==0,"Missing cargo mass/volume was accepted");
        auto invalidCap=cap;invalidCap.bodyKg=40000;check(!invalidCap.valid()&&invalidCap.maximumUnits(1,1)==0,"Negative available payload was accepted");
        invalidCap=cap;invalidCap.volumeM3=std::numeric_limits<float>::quiet_NaN();check(!invalidCap.valid(),"Non-finite capacity was accepted");
        check(live::remainingUnits(43,50)==21,"Half load did not round down to whole units");
        check(live::remainingUnits(43,25)==10&&live::remainingUnits(43,75)==32,"Selectable remaining percentage was wrong");
        check(rejects([]{live::remainingUnits(1,50);}),"An unsplittable unit became an empty final job");
        check(rejects([]{live::remainingUnits(43,0);})&&rejects([]{live::remainingUnits(43,100);}),"Invalid remaining percentage was accepted");
        check(live::matchingPayload(21,10964.1,21,522.1),"Expected reduced load was not confirmed");
        check(!live::matchingPayload(43,22450.3,21,522.1)&&!live::matchingPayload(21,22450.3,21,522.1),"Old full cargo mass/count confirmed a half load");
        check(dispatchLauncher::projectKey(L"C:\\Dispatch\\Main")==dispatchLauncher::projectKey(L"c:\\dispatch\\main"),"Shortcut project identity was case-sensitive");
        check(dispatchLauncher::projectKey(L"C:\\Dispatch\\Main")!=dispatchLauncher::projectKey(L"C:\\Dispatch\\Test"),"Shortcut could focus the wrong data folder");
        live::PickupNavigation nav;nav.arm(1000);
        check(!nav.ready(true,true,false,true,true,1001)&&nav.pending,"Pickup navigation ran outside Drive");
        check(!nav.ready(true,true,true,false,true,1001)&&nav.pending,"Pickup navigation ran while moving/stale speed");
        check(!nav.ready(true,true,true,true,false,1001)&&nav.pending,"Pickup navigation ran without fresh attached trailer");
        check(nav.ready(true,true,true,true,true,1001),"Parked matching unloaded job could not request navigation");
        nav.claim();check(!nav.ready(true,true,true,true,true,1002)&&nav.attempted,"Route reset could execute twice");
        nav.finish(true);check(nav.state=="requested"&&!nav.pending,"Successful reset was not recorded");
        nav.arm(2000);check(!nav.ready(false,true,true,true,true,2001)&&!nav.pending,"Loaded/changed job could reset to pickup");
        nav.arm(3000);check(!nav.ready(true,false,true,true,true,3001)&&!nav.pending,"Changed profile/vehicle could reset navigation");
        nav.arm(4000);check(!nav.ready(true,true,true,true,true,34000)&&!nav.pending,"Expired navigation request could execute");
        nav.arm(35000);check(nav.ready(true,true,true,true,true,35001),"A new job could not arm its own route reset");
        nav.claim();nav.finish(false);check(nav.state=="unavailable"&&!nav.ready(true,true,true,true,true,35002),"Failed reset was retried");
        const live::fs::path project=live::fs::absolute(argv[1]),root=live::fs::absolute(argv[2])/live::uniqueId();
        for(const auto& d:{root,root/"claims",root/"duplicates",root/"results"})live::fs::create_directories(d);
        const dispatchLauncher::Paths launcher={L"C:\\Users\\Grümp\\Dispatch Folder\\DispatchPlanner.exe",L"C:\\Users\\Grümp\\Dispatch Folder"};
        auto registration=dispatchLauncher::registrationJson(launcher);
        check(dispatchLauncher::parseRegistration(registration).exe==launcher.exe,"Launcher lost spaces or Unicode in the executable path");
        auto invalidRegistration=registration;invalidRegistration["executable"]="DispatchPlanner.exe";check(rejects([&]{dispatchLauncher::parseRegistration(invalidRegistration);}),"Launcher accepted a relative executable path");
        invalidRegistration=registration;invalidRegistration["project"]="relative";check(rejects([&]{dispatchLauncher::parseRegistration(invalidRegistration);}),"Launcher accepted a relative project path");
        invalidRegistration=registration;invalidRegistration["executable"]="C:\\Other.exe";check(rejects([&]{dispatchLauncher::parseRegistration(invalidRegistration);}),"Launcher accepted the wrong executable name");
        invalidRegistration=registration;invalidRegistration["version"]=99;check(rejects([&]{dispatchLauncher::parseRegistration(invalidRegistration);}),"Unknown launcher registration version was accepted");
        invalidRegistration=registration;invalidRegistration["project"]=std::string("C:\\folder\0hidden",16);check(rejects([&]{dispatchLauncher::parseRegistration(invalidRegistration);}),"Launcher accepted an embedded null path");
        dispatchLauncher::Paths foundLauncher=launcher;check(!dispatchLauncher::registered(foundLauncher,root)&&foundLauncher.exe.empty(),"Missing launcher registration reused old paths");
        live::writeJson(root/"launcher.json",registration);check(dispatchLauncher::registered(foundLauncher,root)&&foundLauncher.project==launcher.project,"Shared physical launcher registration round-trip failed");
        live::writeJson(root/"launcher.json",invalidRegistration);check(!dispatchLauncher::registered(foundLauncher,root)&&foundLauncher.exe.empty(),"Bad launcher registration retained old paths");
        live::Request r;r.id=live::uniqueId();r.session=live::uniqueId();r.context="world:player:vehicle:trailer";r.trailer="stock|curtainside|single";r.created=live::utcMs();r.expires=r.created+120000;r.route={"dry_milk","yakima","bn_farm","wenatchee","sg_whs"};
        const auto encoded=live::requestJson(r);const auto parsed=live::parseRequest(encoded);
        check(live::routeJson(parsed.route)==live::routeJson(r.route),"Configurable route round-trip failed");
        check(live::currentRequest(r,r.session,r.context,r.trailer,r.created),"Current request rejected");
        check(!live::currentRequest(r,live::uniqueId(),r.context,r.trailer,r.created),"Old game session accepted");
        check(!live::currentRequest(r,r.session,"other-profile",r.trailer,r.created),"Changed game context accepted");
        check(!live::currentRequest(r,r.session,r.context,"different trailer",r.created),"Changed trailer accepted");
        check(!live::currentRequest(r,r.session,r.context,r.trailer,r.expires),"Expired request accepted");
        auto bad=encoded;bad["route"]["cargo_id"]="cargo with spaces";check(rejects([&]{live::parseRequest(bad);}),"Command-like cargo accepted");
        bad=encoded;bad["request_id"]="../escape";check(rejects([&]{live::parseRequest(bad);}),"Unsafe request path accepted");
        bad=encoded;bad["route"]["source_city_id"]="yakima.other";check(rejects([&]{live::parseRequest(bad);}),"Extra depot separator accepted");
        bad=encoded;bad["expires_utc_ms"]=r.created+120001;check(rejects([&]{live::parseRequest(bad);}),"Unbounded request lifetime accepted");
        bad=encoded;bad["game"]="ets2";check(rejects([&]{live::parseRequest(bad);}),"Unsupported game accepted");
        bad=encoded;bad["protocol"]=2;check(rejects([&]{live::parseRequest(bad);}),"Old dispatcher could silently ignore a two-stop request");
        bad=encoded;bad["protocol"]=3;check(rejects([&]{live::parseRequest(bad);}),"Old dispatcher could ignore reduced cargo quantities");
        bad=encoded;bad["load_policy"]="overload";check(rejects([&]{live::parseRequest(bad);}),"Unknown load policy was accepted");
        auto planned=r;planned.twoStop=true;planned.secondId=live::uniqueId();planned.finalRoute=r.route;planned.finalRoute.targetCity="everett";
        const auto plannedJson=live::requestJson(planned);const auto plannedParsed=live::parseRequest(plannedJson);
        check(plannedParsed.twoStop&&plannedParsed.secondId==planned.secondId&&plannedParsed.finalRoute.targetCity=="everett"&&plannedParsed.remainingPercent==50,"Two-stop plan round-trip lost the final leg or remaining percentage");
        bad=plannedJson;bad["continuation"]["remaining_percent"]=100;check(rejects([&]{live::parseRequest(bad);}),"Full second load bypassed percentage validation");
        bad=plannedJson;bad["continuation"]["remaining_percent"]=-1;check(rejects([&]{live::parseRequest(bad);}),"Negative remaining load was accepted");
        bad=plannedJson;bad["continuation"]["remaining_percent"]=50.5;check(rejects([&]{live::parseRequest(bad);}),"Fractional percentage bypassed integer validation");
        bad=plannedJson;bad["continuation"]["route"]["cargo_id"]="food";check(rejects([&]{live::parseRequest(bad);}),"Different carried cargo accepted");
        bad=plannedJson;bad["continuation"]["route"]["source_city_id"]="seattle";check(rejects([&]{live::parseRequest(bad);}),"Different original pickup accepted");
        bad=plannedJson;bad["continuation"]["route"]=live::routeJson(r.route);check(rejects([&]{live::parseRequest(bad);}),"Duplicate delivery depot accepted");
        bad=plannedJson;bad["continuation"]["request_id"]=r.id;check(rejects([&]{live::parseRequest(bad);}),"Both legs could use the same durable identifier");
        live::TwoStopRun run;run.start(planned);
        check(run.active()&&!run.canHandoff(true,true,true),"Final leg started before confirmed first delivery");
        check(!run.delivered(planned.secondId)&&run.state=="first_active","Wrong delivery ID advanced the plan");
        check(run.delivered(planned.id)&&run.state=="waiting_handoff","First delivery did not enable handover");
        check(!run.delivered(planned.id)&&run.state=="waiting_handoff","Duplicate delivery event changed the plan");
        check(!run.canHandoff(false,true,true),"Final leg started before the first native/SDK job cleared");
        check(!run.canHandoff(true,true,false),"Final leg started outside fresh stationary driving");
        check(run.canHandoff(true,true,true),"Ready handover was blocked");
        run.claimHandoff();check(!run.canHandoff(true,true,true)&&run.currentId==planned.secondId,"Handover could execute twice");
        run.state="second_active";check(run.delivered(planned.secondId)&&run.state=="complete"&&!run.active(),"Final delivery did not complete the plan");
        run.start(planned);run.delivered(planned.id);check(!run.canHandoff(true,false,true)&&!run.active(),"Changed profile/trailer was allowed to continue");
        run.start(planned);run.stop("cancelled");check(!run.canHandoff(true,true,true),"Cancelled plan could continue");
        const auto stopRoot=root/"cancel-work";for(const auto& d:{stopRoot,stopRoot/"claims",stopRoot/"duplicates"})live::fs::create_directories(d);
        live::Request queued=r;queued.id=live::uniqueId();const auto queuedJson=live::requestJson(queued);
        live::writeJson(stopRoot/"request.json",queuedJson,false);live::writeJson(stopRoot/"resume.json",live::Json{{"request_id",queued.id}},false);
        live::stopQueuedWork(stopRoot);
        check(!live::fs::exists(stopRoot/"request.json")&&!live::fs::exists(stopRoot/"resume.json"),"Cancellation left queued work available");
        check(live::readJson(stopRoot/"claims"/(queued.id+".json"))==queuedJson,"Cancelled queued dispatch lost its durable claim");
        live::writeJson(stopRoot/"request.json",queuedJson,false);check(!live::claimOnce(stopRoot/"request.json",stopRoot,queued.id),"Cancelled queued dispatch was replayed");
        live::stopQueuedWork(stopRoot);check(!run.canHandoff(true,true,true),"Stopping work allowed a cancelled handoff");
        live::TwoStopRun restarted;check(!restarted.active()&&!restarted.canHandoff(true,true,true),"Restart automatically replayed an interrupted plan");
        const auto incoming=root/"request.json";
        live::writeJson(incoming,encoded,false);
        check(rejects([&]{live::writeJson(incoming,bad,false);}),"Second client overwrote pending request");
        check(live::readJson(incoming)==encoded,"Pending request changed during collision");
        unsigned mutations=0;if(live::claimOnce(incoming,root,r.id))++mutations;
        check(mutations==1&&!live::fs::exists(incoming),"Initial claim failed");
        // Simulate a crash after the claim and before writing a result. Replayed
        // requests still cannot reach another native mutation.
        live::writeJson(incoming,encoded,false);if(live::claimOnce(incoming,root,r.id))++mutations;
        check(mutations==1,"Duplicate request executed after simulated crash");
        check(live::readJson(root/"claims"/(r.id+".json"))==encoded,"Permanent claim was overwritten");
        live::Request second=r;second.id=live::uniqueId();second.route.cargo="food";live::writeJson(incoming,live::requestJson(second),false);if(live::claimOnce(incoming,root,second.id))++mutations;
        check(mutations==2,"A different request required a new session/reload");
        const auto carryClaim=root/"claims"/(planned.secondId+".json");live::writeJson(carryClaim,plannedJson,false);
        check(rejects([&]{live::writeJson(carryClaim,plannedJson,false);}),"Internal handover claim could be overwritten after a crash");
        live::Json status={{"protocol",live::protocolVersion},{"game","ats"},{"session_id",r.session},{"online",true},{"updated_utc_ms",r.created}};
        check(live::freshStatus(status,r.created+1000),"Fresh status rejected");check(!live::freshStatus(status,r.created+6001),"Stale game status accepted");status["online"]=false;check(!live::freshStatus(status,r.created),"Unloaded plugin accepted");
        scs_named_value_t attributes[8]={};attributes[0]=stringValue("cargo.id","dry_milk");attributes[1]=stringValue("source.city.id","yakima");attributes[2]=stringValue("source.company.id","bn_farm");attributes[3]=stringValue("destination.city.id","wenatchee");attributes[4]=stringValue("destination.company.id","sg_whs");attributes[5]=stringValue("job.market","cargo_market");attributes[6].name="cargo.loaded";attributes[6].value.type=SCS_VALUE_TYPE_bool;
        live::Job job;job.update(attributes);check(job.assigned(r.route,0),"Dynamic matching job was not confirmed");check(!job.assigned(r.route,job.revision),"Stale telemetry confirmed assignment");
        attributes[0].value.value_string.value="empty_palet";job.update(attributes);check(!job.matches(r.route),"Old hard-coded cargo matched a new request");attributes[0].value.value_string.value="dry_milk";attributes[6].value.value_bool.value=1;job.update(attributes);check(job.matches(r.route)&&!job.assigned(r.route,0),"Loaded job incorrectly confirmed as newly unloaded");
        check(job.confirmedLoaded(r.route,job.revision-1)&&!job.confirmedLoaded(r.route,job.revision),"Fresh loaded transition was not required for the carried leg");
        attributes[6].value.value_bool.value=0;job.update(attributes);check(!job.confirmedLoaded(r.route,0),"Unloaded job confirmed the carried leg");
        scs_named_value_t weighted[11]={};for(size_t i=0;i<7;++i)weighted[i]=attributes[i];
        weighted[7].name="cargo.mass";weighted[7].value.type=SCS_VALUE_TYPE_float;weighted[7].value.value_float.value=10964.1f;
        weighted[8].name="cargo.unit.mass";weighted[8].value.type=SCS_VALUE_TYPE_float;weighted[8].value.value_float.value=522.1f;
        weighted[9].name="cargo.unit.count";weighted[9].value.type=SCS_VALUE_TYPE_u32;weighted[9].value.value_u32.value=21;
        job.update(weighted);check(job.payloadKnown&&job.payloadMatches(21,522.1),"SDK cargo quantity/mass did not confirm the reduced payload");
        weighted[7].value.value_float.value=22450.3f;job.update(weighted);check(!job.payloadMatches(21,522.1),"SDK full cargo mass confirmed a reduced payload");
        job.update(attributes);check(!job.payloadKnown&&!job.payloadMatches(21,522.1),"Missing SDK payload retained previous cargo quantities");
        job.update(nullptr);check(!job.active&&!job.matches(r.route)&&!job.payloadKnown,"Ended job remained active");
        ats::Catalog catalog(project);check(catalog.locations.size()>100,"ATS map catalog incomplete");check(catalog.locations.count("yakima")&&catalog.locations.at("yakima").count("bn_farm"),"Washington pickup depot missing");
        check(catalog.positions.count("yakima")!=0,"Pickup city map coordinates missing");
        const auto yakima=catalog.positions.at("yakima");
        check(catalog.nearbyCity(yakima[0],yakima[1])=="yakima","Truck position did not match its current city");
        check(catalog.nearbyCity(1e9,1e9).empty(),"Remote truck position invented a current city");
        auto geometry=catalog;geometry.positions={{"a",{0,0}},{"b",{10000,0}},{"c",{20000,0}},{"behind",{-10000,0}},{"beyond",{30000,0}},{"detour",{10000,20000}}};
        check(geometry.alongRoute("a","b","c"),"Intermediate stop on route was rejected");
        check(!geometry.alongRoute("a","behind","c")&&!geometry.alongRoute("a","beyond","c")&&!geometry.alongRoute("a","detour","c"),"Backtracking or excessive detour accepted");
        check(!geometry.alongRoute("a","a","c")&&!geometry.alongRoute("a","c","c")&&!geometry.alongRoute("a","missing","c"),"Duplicate or unmapped delivery city accepted");
        geometry.locations={{"a",{"depot"}},{"b",{"depot"}},{"c",{"depot"}},{"detour",{"depot"}}};geometry.incoming={{"depot",{"wood"}}};
        const auto stops=geometry.firstStops("wood","a","c");check(stops.size()==1&&stops[0].id=="b","First-stop choices did not follow selected final city");
        check(geometry.firstStops("incompatible","a","c").empty(),"First-stop choices ignored cargo compatibility");
        check(catalog.compatible("dry_milk","curtainside"),"Curtainsider-compatible cargo rejected");check(!catalog.compatible("dry_milk","tanker"),"Incompatible tanker cargo offered");
        check(std::fabs(catalog.cargo.at("battery").unitKg-522.1)<0.01&&std::fabs(catalog.cargo.at("battery").unitM3-2.6)<0.01,"Cargo unit mass/volume catalog parsing failed");
        check(catalog.valid(r.route,"curtainside"),"Chosen test route is not supplied/accepted by these companies");
        check(!catalog.valid(r.route,"tanker"),"Catalog accepted incompatible trailer body");
        check(!catalog.valid(live::Route{"dry_milk","yakima","not_a_company","wenatchee","sg_whs"},"curtainside"),"Unavailable pickup company accepted");
        std::cout<<"PASS: "<<count<<" live checks: protocol, stale session/profile/trailer, request expiry, input validation, durable duplicate/crash protection, sequential IDs, telemetry and ATS catalog/trailer filtering.\n";
        std::cout<<"ATS catalog: "<<catalog.locations.size()<<" cities, "<<catalog.cargo.size()<<" cargo definitions.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
