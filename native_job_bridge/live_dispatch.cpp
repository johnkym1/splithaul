// Configurable owned-trailer combination dispatch. Native calls remain exact-build gated.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <shlobj.h>
#include "scssdk_telemetry.h"
#include "native_arguments.h"
#include "live_state.h"
#include "pickup_navigation.h"
#include "two_stop_state.h"
#include "menu_snapshot.h"
#include "../dispatch_cpp/restart_match.h"
#include "../dispatch_cpp/ats_profile_welcome.h"
#include "../dispatch_cpp/profile_files.h"
#include "../dispatch_shared/cancel_protocol.h"
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"ole32.lib")

static scs_log_t gameLog=nullptr;
static uintptr_t imageBase=0;
static bool disabled=false,nativeFault=false;
#include "host_builds.h"
#include "native_calls.h"
#include "native_trailers.h"
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
#include "cancellation_build.h"
#endif
#include "native_capacity.h"
#include "app_hotkey.h"

namespace fs=std::filesystem;
using live::Json;
static fs::path folder;
static DWORD callbackThread=0;
static ULONGLONG lastPoll=0,lastSpeed=0,lastTrailer=0,deadline=0,endedAt=0;
static ULONGLONG lastIgnition=0;
static bool ignitionOn=false;
static live::MenuSnapshot menuSnapshot;
static bool driving=false,jobSeen=false,carSeen=false,busSeen=false,carActive=false,busActive=false;
static bool awaiting=false,monitoring=false;
static bool resumedTracking=false;
static float speed=0;
static live::Job job;
static live::Trailer trailer;
static live::Request working;
static live::PickupNavigation pickupNavigation;
static live::TwoStopRun chain;
static bool carryMode=false,carryPending=false,carryConfirming=false;
static ULONGLONG carryDeadline=0;
static unsigned beforeLoadRevision=0;
static float navigationDistance=0;
static ULONGLONG lastNavigationDistance=0;
static double truckX=0,truckZ=0;
static ULONGLONG lastPosition=0;
static unsigned beforeRevision=0;
static uint32_t expectedUnits=0,routeMaximumUnits=0;
static float expectedUnitKg=0;
static std::string session,state="starting",lastId,lastProgress;
static std::string autoOpenedRequest;
static ULONGLONG lastAutoOpenCheck=0;
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
static std::string pendingCancelId;
static bool observedCancelEvent=false;
static ULONGLONG pendingCancelDeadline=0;
#ifdef GT_CANCELLATION_TEST
static unsigned testCancelCalls=0;
static bool testCancelSucceeds=true;
static bool testCancelEmit=true;
#endif
#endif

struct Objects {uintptr_t economy=0,player=0,vehicles=0,ownedTrailer=0,definition=0,source=0,target=0,cargo=0;};
static bool basicObjects(Objects& o) {
    const bool found=word(imageBase+economyRva,o.economy)&&liveUnit(o.economy)&&word(o.economy+0x18,o.player)&&liveUnit(o.player)&&word(o.player+0xa0,o.vehicles)&&liveUnit(o.vehicles)&&word(o.vehicles+0x48,o.ownedTrailer)&&liveUnit(o.ownedTrailer)&&word(o.ownedTrailer+0x1a0,o.definition)&&liveUnit(o.definition);
    trailer.combination=found?readTrailerCombination(o.ownedTrailer):live::TrailerCombination{};
    return found;
}
static bool trailerCapacity(const Objects& o,live::TrailerCapacity& cap) {
    return readMemory(o.definition+0x30,&cap.grossKg,4)&&readMemory(o.definition+0x34,&cap.chassisKg,4)&&readMemory(o.definition+0x38,&cap.bodyKg,4)&&readMemory(o.definition+0x3c,&cap.axles,4)&&readMemory(o.definition+0x40,&cap.volumeM3,4)&&cap.valid();
}
static Json capacityJson(const Objects& o) {
    live::TrailerCapacity cap;if(!trailerCapacity(o,cap))return {{"available",false}};
    return {{"available",true},{"gross_limit_kg",cap.grossKg},{"chassis_mass_kg",cap.chassisKg},{"body_mass_kg",cap.bodyKg},{"payload_ceiling_kg",cap.payloadKg()},{"volume_m3",cap.volumeM3},{"axles",cap.axles}};
}
static uint32_t maximumForRoute(const Objects& o,float& unitKg) {
    live::TrailerCapacity cap;float unitVolume=0;
    if(!capacityCodeMatches()||!trailerCapacity(o,cap)||!readMemory(o.cargo+0x9c,&unitKg,4)||!readMemory(o.cargo+0x98,&unitVolume,4))return 0;
    const auto geometricMaximum=cap.maximumUnits(unitKg,unitVolume);if(!geometricMaximum)return 0;
    const auto sourceCity=nativeCompanyCity(o.source),targetCity=nativeCompanyCity(o.target);
    NativeCapacityRequest request;request.economy=o.economy;request.definition=o.definition;request.cargo=o.cargo;
    if(!liveUnit(sourceCity)||!liveUnit(targetCity)||!word(sourceCity+0x160,request.sourceCountry)||!liveUnit(request.sourceCountry)||!word(targetCity+0x160,request.targetCountry)||!liveUnit(request.targetCountry))return 0;
    // Preserve ATS's normal job-generator tractor allowance, but force 100%
    // utilization instead of its random fill. Country/axle limits still apply.
    const auto count=nativeMaximumUnits(&request);
    return count&&count<=geometricMaximum&&count==request.units?count:0;
}
static std::string context(const Objects& o) {std::ostringstream s;s<<std::hex<<o.economy<<':'<<o.player<<':'<<o.vehicles<<':'<<o.ownedTrailer;return s.str()+trailer.combination.identity;}
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
static std::string cancellationTarget(uintptr_t player) {
    uintptr_t ordinary=0,car=0,bus=0;if(!word(player+0x28,ordinary)||!word(player+0x30,car)||!word(player+0x38,bus))return {};
    std::ostringstream out;out<<std::hex<<ordinary<<':'<<car<<':'<<bus<<':'<<job.revision;return out.str();
}
#endif
static Json navigationStatus() {
    uintptr_t root=0,count=0;uint32_t mode=0;
    const bool native=word(imageBase+mainGameRva,root)&&root&&readMemory(root+0x4300,&mode,4)&&word(root+0x4340,count);
    const bool fresh=lastNavigationDistance&&GetTickCount64()-lastNavigationDistance<2000&&std::isfinite(navigationDistance);
    return {{"pickup_state",pickupNavigation.state},{"attempted",pickupNavigation.attempted},{"distance_m",fresh?Json(navigationDistance):Json(nullptr)},{"native_route_mode",native?Json(mode):Json(nullptr)},{"native_waypoints",native&&count<=10000?Json(count):Json(nullptr)}};
}
static bool inEconomy(const Objects& o,uintptr_t company) {
    uintptr_t data=0,count=0;if(!word(o.economy+0x28,data)||!word(o.economy+0x30,count)||!data||count>10000)return false;
    std::vector<uintptr_t> companies(count);return readMemory(data,companies.data(),companies.size()*8)&&std::find(companies.begin(),companies.end(),company)!=companies.end();
}
static bool bodyCompatible(const Objects& o) {
    uintptr_t types=0,count=0,body=0;if(!word(o.cargo+0xa8,types)||!word(o.cargo+0xb0,count)||!word(o.definition+0x48,body)||!types||!count||count>128||!body)return false;
    std::vector<uintptr_t> allowed(count);return readMemory(types,allowed.data(),allowed.size()*8)&&std::find(allowed.begin(),allowed.end(),body)!=allowed.end();
}
static const char* findObjects(Objects& o,const live::Route& r) {
    if(!basicObjects(o))return "The owned trailer or player is unavailable.";
    if(!trailer.supported())return "The attached combination is unverified or exceeds this game's trailer limit.";
    for(uintptr_t off=0x28;off<=0x38;off+=8){uintptr_t active=0;if(!word(o.player+off,active)||active)return "An active native job already exists.";}
    uint32_t mode=0;if(!readMemory(o.player+0x98,&mode,4)||mode!=0)return "Truck mode is required.";
    uintptr_t root=0,actor=0;if(!word(imageBase+mainGameRva,root)||!root||!word(root+0x31b0,actor)||!liveUnit(actor))return "Return to the driving view.";
    o.source=lookupUnit(("company.volatile."+r.source()).c_str());o.target=lookupUnit(("company.volatile."+r.target()).c_str());o.cargo=lookupUnit(("cargo."+r.cargo).c_str());
    if(!liveUnit(o.source)||!liveUnit(o.target)||!liveUnit(o.cargo))return "The chosen cargo or depot is not available in this game.";
    for(auto company:{o.source,o.target}){uintptr_t vtable=0,prefab=0;if(!word(company,vtable)||vtable!=imageBase+companyVtableRva||!word(company+0x10,prefab)||!prefab||!inEconomy(o,company))return "A chosen depot is not present in this profile's economy.";}
    if(!bodyCompatible(o))return "The live trailer definition does not support this cargo body type.";
    return nullptr;
}
static uintptr_t matchingOffer(const Objects& o,const live::Route& r) {
    uintptr_t data=0,count=0,chosen=0;uint32_t clock=0;
    if(!word(o.source+0xa8,data)||!word(o.source+0xb0,count)||!data||count>128||!readMemory(o.economy+0x19c,&clock,4))return 0;
    const auto target=r.target();
    for(uintptr_t i=0;i<count;++i){uintptr_t candidate=0,vtable=0,cargo=0;uint32_t expiry=0;
        if(!word(data+i*8,candidate)||!liveUnit(candidate)||!word(candidate,vtable)||vtable!=imageBase+cargoOfferVtableRva||!word(candidate+0x58,cargo)||!readMemory(candidate+0x48,&expiry,4))return 0;
        if(cargo!=o.cargo||expiry<=clock||!stringEquals(candidate+0x10,target.c_str()))continue;
        if(chosen)return 0;chosen=candidate;
    }return chosen;
}
static bool createOfferCall(uintptr_t registry,NativeParsedInput* args) {
    using Handler=bool(__fastcall*)(void*,void*);
    __try{return reinterpret_cast<Handler>(imageBase+handlerRva)(reinterpret_cast<void*>(registry),args);}
    __except(EXCEPTION_EXECUTE_HANDLER){nativeFault=true;return false;}
}
static bool createOffer(uintptr_t registry,const live::Route& r) {
    const auto source=r.source(),target=r.target();
    NativeStringInput tokens[]={borrowedInput("cheat"),borrowedInput("job"),borrowedInput(r.cargo.c_str()),borrowedInput(source.c_str()),borrowedInput(target.c_str())};
    NativeParsedInput args={};args.data=tokens;args.count=5;return createOfferCall(registry,&args);
}
static void setState(const std::string& next) {if(state==next)return;state=next;log(("[GT_TELEMETRY] "+state).c_str());}
static void saveChain() {if(chain.enabled())live::writeJson(folder/"plans"/(chain.first.id+".json"),chain.json(session));}
static void result(const live::Request& r,const std::string& phase,const std::string& message,const Json& extra=Json::object()) {
    Json value={{"protocol",live::protocolVersion},{"request_id",r.id},{"session_id",resumedTracking?r.session:session},{"state",phase},{"message",message},{"updated_utc_ms",live::utcMs()},{"route",live::routeJson(r.route)},{"job",job.json()}};
    if(resumedTracking)value["observed_session_id"]=session;
    if(r.id==working.id)value["payload"]={{"expected_units",expectedUnits},{"unit_mass_kg",expectedUnitKg},{"expected_mass_kg",double(expectedUnits)*expectedUnitKg},{"route_maximum_units",routeMaximumUnits}};
    value["details"]=extra;live::writeJson(folder/"results"/(r.id+".json"),value);lastId=r.id;lastProgress=phase;
    if(chain.active()&&r.id==chain.currentId&&(phase=="failed"||phase=="rejected"||phase=="uncertain"||phase=="ended_unconfirmed")){chain.stop(message,phase=="uncertain"||phase=="ended_unconfirmed");saveChain();}
}
static void guideToPickup() {
    if(!pickupNavigation.pending)return;
    const auto now=GetTickCount64();Objects o;const bool world=basicObjects(o);
    const bool sameContext=world&&context(o)==working.context&&trailer.signature()==working.trailer;
    const bool stationary=lastSpeed&&now-lastSpeed<2000&&std::isfinite(speed)&&std::fabs(speed)<=0.1f;
    const bool attached=lastTrailer&&now-lastTrailer<2000&&trailer.connected&&trailer.supported();
    const bool unloaded=job.matches(working.route)&&job.loadedKnown&&!job.loaded;
    if(!pickupNavigation.ready(unloaded,sameContext,driving,stationary,attached,now)) {
        if(!pickupNavigation.pending&&unloaded)result(working,"assigned","Job assigned. Pickup navigation was unavailable; set a waypoint to the pickup depot manually.",navigationStatus());
        return;
    }
    // Claim this route attempt before any native call; failure never triggers
    // another acceptance or routing call. All checks run on the SDK frame thread.
    pickupNavigation.claim();
    uintptr_t nativeJob=0,source=0,target=0,root=0,actor=0,otherJob=0;uint32_t mode=0;unsigned char loaded=1;
    bool safe=!disabled&&!nativeFault&&callbackThread==GetCurrentThreadId()&&routingCodeMatches()
        &&word(o.player+0x28,nativeJob)&&liveUnit(nativeJob)
        &&readMemory(nativeJob+0x13c,&loaded,1)&&!loaded
        &&word(nativeJob+0x28,source)&&liveUnit(source)&&word(nativeJob+0x30,target)&&liveUnit(target)
        &&readMemory(o.player+0x98,&mode,4)&&mode==0
        &&word(o.player+0x30,otherJob)&&!otherJob&&word(o.player+0x38,otherJob)&&!otherJob
        &&word(imageBase+mainGameRva,root)&&root&&word(root+0x31b0,actor)&&liveUnit(actor);
    if(safe) {
        const auto expectedSource=lookupUnit(("company.volatile."+working.route.source()).c_str());
        const auto expectedTarget=lookupUnit(("company.volatile."+working.route.target()).c_str());
        safe=!disabled&&source==expectedSource&&target==expectedTarget;
        for(auto company:{source,target}){uintptr_t table=0,prefab=0;safe=safe&&word(company,table)&&table==imageBase+companyVtableRva&&word(company+0x10,prefab)&&prefab&&inEconomy(o,company);}
    }
    const bool requested=safe&&resetPickupRoute(o.player);pickupNavigation.finish(requested);
    const auto details=navigationStatus();
    log(("[GT_TELEMETRY] PICKUP_NAVIGATION "+working.id+" "+pickupNavigation.state+" "+details.dump()).c_str());
    result(working,"assigned",requested?"Job assigned. Pickup route requested; check the GPS and load normally.":"Job assigned. Pickup navigation was unavailable; set a waypoint to the pickup depot manually.",details);
}
static void publish() {
    Objects o;const bool world=basicObjects(o);const auto token=world?context(o):std::string();
    bool cancellationInFlight=false;
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
    cancellationInFlight=!pendingCancelId.empty();
#endif
    const bool canQueue=!cancellationInFlight&&!disabled&&!chain.active()&&!awaiting&&!monitoring&&!job.active&&!carActive&&!busActive&&jobSeen&&carSeen&&busSeen&&trailer.connected&&trailer.supported()&&!trailer.signature().empty()&&world&&!fs::exists(folder/"request.json");
    Json value={{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"online",true},{"pid",GetCurrentProcessId()},{"session_id",session},{"updated_utc_ms",live::utcMs()},{"state",state},{"context_id",token},{"can_queue",canQueue},{"disabled",disabled},{"driving",driving},{"trailer",trailer.json()},{"job",job.json()},{"last_request_id",lastId},{"last_progress",lastProgress},{"awaiting_confirmation",awaiting},{"host_sha256",activeHostHash?activeHostHash:""},{"plugin","GT_Telemetry.dll"}};
    value["build_id"]=dispatchGame::build;value["resumed_monitoring"]=resumedTracking&&monitoring;value["app_launcher"]=appHotkey::status();value["navigation"]=navigationStatus();value["trailer_capacity"]=world?capacityJson(o):Json{{"available",false}};if(chain.enabled())value["plan"]=chain.json(session);
    value["menu_snapshot"]={{"captured",menuSnapshot.captured},{"speed_mps",menuSnapshot.speedKnown?Json(menuSnapshot.speedMps):Json(nullptr)},{"stopped",menuSnapshot.stopped()},{"ignition_on",menuSnapshot.ignitionKnown?Json(menuSnapshot.ignitionOn):Json(nullptr)}};
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
    value["experimental_cancel_available"]=!disabled&&!nativeFault&&cancellationCodeMatches();
    value["cancel_target_token"]=world?cancellationTarget(o.player):std::string();
    value["car_job_active"]=carSeen&&carActive;value["bus_job_active"]=busSeen&&busActive;
#endif
    value["truck_position"]=lastPosition&&GetTickCount64()-lastPosition<2000?Json{{"x",truckX},{"z",truckZ}}:Json(nullptr);
    value["speed_mps"]=lastSpeed&&GetTickCount64()-lastSpeed<2000&&std::isfinite(speed)?Json(speed):Json(nullptr);live::writeJson(folder/"status.json",value);
}
static bool claim(const fs::path& incoming,const std::string& id) {
    const bool claimed=live::claimOnce(incoming,folder,id);if(!claimed)setState("duplicate_request_ignored");return claimed;
}
static void executeClaimed(uintptr_t registry,const live::Request& r,bool carried=false) {
    const auto now=GetTickCount64();resumedTracking=false;working=r;pickupNavigation={};carryMode=carried;carryPending=carryConfirming=false;expectedUnits=routeMaximumUnits=0;expectedUnitKg=0;
    if(!carried){chain.start(r);saveChain();}
    result(r,"executing","Request claimed. Checking the live economy and trailer.");
    Objects o;const char* reason=findObjects(o,r.route);
    if(reason){result(r,"rejected",reason);return;}
    if(r.twoStop){Objects finalObjects;reason=findObjects(finalObjects,r.finalRoute);if(reason){result(r,"rejected",std::string("Final delivery: ")+reason);return;}}
    if(context(o)!=r.context||trailer.signature()!=r.trailer){result(r,"rejected","The selected vehicle, trailer combination or profile changed.");return;}
    routeMaximumUnits=maximumForRoute(o,expectedUnitKg);
    if(!routeMaximumUnits){result(r,"rejected","The maximum cargo load could not be verified for this trailer and route.");return;}
    expectedUnits=carried?chain.finalUnits:routeMaximumUnits;
    if(!expectedUnits||expectedUnits>routeMaximumUnits||(carried&&std::fabs(chain.unitMassKg-expectedUnitKg)>0.01)){result(r,"rejected","The planned remaining cargo no longer fits this route or cargo definition.");return;}
    if(r.twoStop) {
        Objects finalObjects;reason=findObjects(finalObjects,r.finalRoute);float finalUnitKg=0;
        const auto finalMaximum=reason?0:maximumForRoute(finalObjects,finalUnitKg);
        try {chain.firstUnits=expectedUnits;chain.finalUnits=live::remainingUnits(expectedUnits,r.remainingPercent);chain.unitMassKg=expectedUnitKg;}
        catch(const std::exception& e){result(r,"rejected",e.what());return;}
        if(!finalMaximum||chain.finalUnits>finalMaximum||std::fabs(finalUnitKg-expectedUnitKg)>0.01){result(r,"rejected","The remaining cargo portion does not fit the final route. Choose a smaller percentage or another depot.");return;}
        saveChain();
    }
    log(("[GT_TELEMETRY] REQUEST "+r.id+" "+r.route.cargo+": "+r.route.source()+" -> "+r.route.target()+(carried?" [carried final leg]":"")).c_str());
    if(!createOffer(registry,r.route)){result(r,"failed","Offer creation failed; this request will not be retried.");if(nativeFault)disabled=true;return;}
    const auto selected=matchingOffer(o,r.route);if(!selected){result(r,"failed","The game did not produce exactly one matching compatible Cargo Market offer. No assignment was attempted.");return;}
    Objects checked;reason=findObjects(checked,r.route);
    if(reason||context(checked)!=r.context||trailer.signature()!=r.trailer||checked.definition!=o.definition||checked.cargo!=o.cargo||checked.source!=o.source||checked.target!=o.target||!bodyCompatible(checked)){result(r,"failed","Game state changed after offer creation. No assignment was attempted.");return;}
    uintptr_t table=0;uint32_t originalUnits=0;
    if(!word(selected,table)||table!=imageBase+cargoOfferVtableRva||!readMemory(selected+0xcc,&originalUnits,4)||!originalUnits||originalUnits>1000000||!setOfferUnits(selected,expectedUnits,float(expectedUnits)/routeMaximumUnits)){result(r,"uncertain","Cargo quantity could not be set safely. No assignment retry.");disabled=true;return;}
    uint32_t checkedUnits=0;if(!readMemory(selected+0xcc,&checkedUnits,4)||checkedUnits!=expectedUnits){result(r,"uncertain","Cargo quantity verification failed. No assignment attempted.");disabled=true;return;}
    log(("[GT_TELEMETRY] PAYLOAD "+r.id+" units="+std::to_string(expectedUnits)+" mass_kg="+std::to_string(double(expectedUnits)*expectedUnitKg)+" route_max_units="+std::to_string(routeMaximumUnits)).c_str());
    beforeRevision=job.revision;
    if(!acceptOffer(o.economy,o.source,selected)){result(r,nativeFault?"uncertain":"failed","The native acceptance call failed. No automatic retry.");if(nativeFault)disabled=true;return;}
    awaiting=true;deadline=now+15000;setState("waiting_fresh_assignment_telemetry");
}
static void tryHandoff(uintptr_t registry) {
    if(chain.state!="waiting_handoff")return;
    Objects o;if(!basicObjects(o))return;
    const bool same=context(o)==chain.first.context&&trailer.signature()==chain.first.trailer;
    const auto now=GetTickCount64();
    const bool ready=driving&&lastSpeed&&now-lastSpeed<2000&&std::isfinite(speed)&&std::fabs(speed)<=0.1f&&lastTrailer&&now-lastTrailer<2000&&trailer.connected&&trailer.supported();
    // Delivery screens briefly reset trailer configuration. Judge identity only
    // after fresh Drive samples, not during those transient menu resets.
    if(!ready||!trailer.configured||trailer.signature().empty())return;
    if((job.active&&!job.matches(chain.first.route))||carActive||busActive){chain.stop("Another game job appeared before handover.");saveChain();return;}
    bool empty=!job.active&&!awaiting&&!monitoring;
    for(uintptr_t off=0x28;off<=0x38;off+=8){uintptr_t active=0;empty=empty&&word(o.player+off,active)&&!active;}
    if(!chain.canHandoff(empty,same,ready)){if(!chain.active())saveChain();return;}
    live::Request next;next.id=chain.first.secondId;next.route=chain.first.finalRoute;next.session=session;next.context=chain.first.context;next.trailer=chain.first.trailer;next.created=live::utcMs();next.expires=next.created+120000;
    // No incoming file exists for an internal handover. Reserve the second ID
    // durably without replacement before either native mutation.
    auto claimed=live::requestJson(next);claimed["handoff_from"]=chain.first.id;
    if(fs::exists(folder/"claims"/(next.id+".json"))){chain.stop("The final-leg identifier was already consumed. No retry.",true);saveChain();return;}
    live::writeJson(folder/"claims"/(next.id+".json"),claimed,false);
    chain.claimHandoff();saveChain();executeClaimed(registry,next,true);
}
static void pollRequest(uintptr_t registry) {
    const auto incoming=folder/"request.json";if(!fs::exists(incoming))return;
    live::Request r;
    try{r=live::parseRequest(live::readJson(incoming,8192));}catch(const std::exception&){const auto id=live::uniqueId();if(claim(incoming,id)){setState("invalid_request_rejected");}return;}
    Objects basic;const bool world=basicObjects(basic);
    if(!live::currentRequest(r,session,world?context(basic):"",trailer.signature(),live::utcMs())){if(claim(incoming,r.id))result(r,"rejected","Request expired, or the game/profile/trailer changed. Select and dispatch again.");return;}
    if(disabled||chain.active()||awaiting||monitoring||job.active||carActive||busActive){if(claim(incoming,r.id))result(r,"rejected","A job or two-stop plan is already active, or this session is disabled.");return;}
    if(!jobSeen||!carSeen||!busSeen){setState("waiting_job_configurations");return;}
    const auto now=GetTickCount64();
    if(!driving||!lastSpeed||now-lastSpeed>2000||!std::isfinite(speed)||std::fabs(speed)>0.1f){setState("request_waiting_stationary_driving");return;}
    if(!lastTrailer||now-lastTrailer>2000||!trailer.connected||!trailer.supported()){setState("request_waiting_supported_attached_combination");return;}
    if(!claim(incoming,r.id))return;
    executeClaimed(registry,r);
}
static void pollResume() {
    const auto incoming=folder/"resume.json";if(!fs::exists(incoming))return;
    const auto consumed=folder/"duplicates"/(live::uniqueId()+".resume.json");
    if(!MoveFileExW(incoming.c_str(),consumed.c_str(),MOVEFILE_WRITE_THROUGH))return;
    try {
        const auto request=live::readJson(consumed,8192);const auto now=live::utcMs();
        const auto id=request.at("request_id").get<std::string>(),original=request.at("original_session_id").get<std::string>();
        const auto created=request.at("created_utc_ms").get<uint64_t>();
        if(request.at("protocol")!=live::protocolVersion||request.at("target_session_id")!=session||!live::requestId(id)||!live::requestId(original)||created>now+5000||now>created+10000||disabled||awaiting||monitoring||chain.active()||carActive||busActive||!jobSeen)return;
        Objects o;if(!basicObjects(o)||request.at("context_id")!=context(o)||request.at("trailer_signature")!=trailer.signature()||!trailer.connected||!trailer.supported())return;
        const auto previous=live::readJson(folder/"results"/(id+".json"));
        const auto claim=live::parseRequest(live::readJson(folder/"claims"/(id+".json")));
        if(claim.twoStop||claim.id!=id||claim.session!=original||claim.trailer!=trailer.signature()||live::routeJson(claim.route)!=previous.at("route"))return;
        const Json current={{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"online",true},{"session_id",session},{"updated_utc_ms",now},{"job",job.json()}};
        if(!ats::restartJobCandidate(previous,current,id,original,now))return;
        const auto payload=previous.at("payload");const auto units=payload.at("expected_units").get<uint32_t>();const auto unitKg=payload.at("unit_mass_kg").get<float>();
        if(!units||!std::isfinite(unitKg)||unitKg<=0||!job.payloadMatches(units,unitKg))return;
        working={};working.id=id;working.session=original;working.route=live::parseRoute(previous.at("route"));working.context=context(o);working.trailer=trailer.signature();
        expectedUnits=units;expectedUnitKg=unitKg;routeMaximumUnits=payload.value("route_maximum_units",units);pickupNavigation={};carryMode=carryPending=carryConfirming=false;chain={};endedAt=0;resumedTracking=true;monitoring=true;
        result(working,job.loadedKnown&&job.loaded?"loaded":"assigned","Tracking resumed for the matching job after game restart.");setState("resumed_job_monitoring");
        log(("[GT_TELEMETRY] RESUMED_MONITORING "+id).c_str());
    }catch(const std::exception& ex){log((std::string("[GT_TELEMETRY] Resume rejected: ")+ex.what()).c_str());}
}
static void openForRecoveredJob() {
    const auto now=GetTickCount64();
    if(!driving||!jobSeen||!job.active||monitoring||awaiting||disabled||!lastSpeed||now-lastSpeed>2000||!std::isfinite(speed)||std::fabs(speed)>0.1f||!lastTrailer||now-lastTrailer>2000||!trailer.connected||!trailer.supported()||trailer.signature().empty()||now-lastAutoOpenCheck<5000)return;
    lastAutoOpenCheck=now;
    try {
        dispatchLauncher::Paths paths;if(!dispatchLauncher::registered(paths))return;
        PWSTR documents=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents)))return;
        const auto logPath=fs::path(documents)/dispatchGame::documents/L"game.log.txt";CoTaskMemFree(documents);
        std::ifstream stream(logPath,std::ios::binary|std::ios::ate);if(!stream)return;const auto size=stream.tellg();if(size<=0||size>std::streamoff(32*1024*1024))return;
        std::string content(static_cast<size_t>(size),'\0');stream.seekg(0);if(!stream.read(content.data(),size))return;
        const auto profile=ats::selectedProfileFromLog(content);if(profile.empty())return;
        const auto profileDir=paths.project/dispatchGame::profiles/ats::profileFolderId(profile);
        const auto saved=live::readJson(profileDir/dispatchGame::savedDispatch);
        const auto id=saved.at("last_request_id").get<std::string>(),original=saved.at("session_id").get<std::string>();
        if(!live::requestId(id)||!live::requestId(original)||saved.value("two_stop",false)||saved.value("form_cleared_for",std::string())==id||autoOpenedRequest==id)return;
        const auto owner=live::readJson(profileDir/"owned_requests"/(id+".json"));if(owner.at("request_id")!=id||owner.at("session_id")!=original)return;
        const auto claim=live::parseRequest(live::readJson(folder/"claims"/(id+".json")));if(claim.twoStop||claim.id!=id||claim.session!=original||claim.trailer!=trailer.signature())return;
        const auto receipt=live::readJson(folder/"results"/(id+".json"));
        const Json current={{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"online",true},{"session_id",session},{"updated_utc_ms",live::utcMs()},{"job",job.json()}};
        if(live::routeJson(claim.route)!=receipt.at("route")||!ats::restartJobCandidate(receipt,current,id,original,live::utcMs()))return;
        if(appHotkey::requestRecovered()){autoOpenedRequest=id;log(("[GT_TELEMETRY] Recovered job detected; opening Dispatch Companion: "+id).c_str());}
    }catch(const std::exception&){/* A missing or ambiguous saved dispatch is not a recovered job. */}
}
static void carryFailure(const std::string& why) {
    carryPending=carryConfirming=false;disabled=true;result(working,"uncertain",why);setState("disabled_uncertain_handoff");
}
static void prepareCarriedLoad() {
    if(!carryPending&&!carryConfirming)return;
    const auto now=GetTickCount64();
    if(carryConfirming) {
        if(job.confirmedLoaded(working.route,beforeLoadRevision)&&job.payloadMatches(expectedUnits,expectedUnitKg)) {
            carryConfirming=false;chain.state="second_active";chain.message="Final leg loaded. Follow the GPS to the final delivery.";saveChain();
            result(working,"loaded","Final leg loaded and ready for delivery. No return to pickup is needed.",navigationStatus());setState("carried_final_leg_loaded");
        }else if(now>carryDeadline||!job.matches(working.route)||(job.loadedKnown&&job.loaded&&job.payloadKnown&&!job.payloadMatches(expectedUnits,expectedUnitKg)))carryFailure("Final-leg loading/weight was not confirmed by fresh matching telemetry. Check the game; no retry.");
        return;
    }
    if(now>carryDeadline){carryFailure("The final leg could not be prepared while stopped. Check the assigned job; no retry.");return;}
    if(!driving||!lastSpeed||now-lastSpeed>=2000||!std::isfinite(speed)||std::fabs(speed)>0.1f||!lastTrailer||now-lastTrailer>=2000||!trailer.connected||!trailer.configured||trailer.signature().empty())return;
    Objects o;if(!basicObjects(o))return;
    if(context(o)!=working.context||trailer.signature()!=working.trailer||!job.matches(working.route)){carryFailure("The game/profile/trailer or final job changed during handover.");return;}
    uintptr_t nativeJob=0,source=0,target=0,external=0,root=0,actor=0,other=0;unsigned char loaded=1;uint32_t mode=1;
    bool safe=chain.second()&&callbackThread==GetCurrentThreadId()&&loadingCodeMatches()&&routingCodeMatches()
        &&word(o.player+0x28,nativeJob)&&liveUnit(nativeJob)&&readMemory(nativeJob+0x13c,&loaded,1)&&!loaded
        &&word(nativeJob+0x1a0,external)&&external==static_cast<uintptr_t>(-1)
        &&word(nativeJob+0x48,external)&&!external
        &&word(nativeJob+0x28,source)&&liveUnit(source)&&word(nativeJob+0x30,target)&&liveUnit(target)
        &&readMemory(o.player+0x98,&mode,4)&&mode==0
        &&word(o.player+0x30,other)&&!other&&word(o.player+0x38,other)&&!other
        &&word(imageBase+mainGameRva,root)&&root&&word(root+0x31b0,actor)&&liveUnit(actor);
    if(safe) {
        safe=source==lookupUnit(("company.volatile."+working.route.source()).c_str())&&target==lookupUnit(("company.volatile."+working.route.target()).c_str());
        o.cargo=lookupUnit(("cargo."+working.route.cargo).c_str());safe=safe&&!disabled&&liveUnit(o.cargo)&&bodyCompatible(o);
        for(auto company:{source,target}){uintptr_t table=0,prefab=0;safe=safe&&word(company,table)&&table==imageBase+companyVtableRva&&word(company+0x10,prefab)&&prefab&&inEconomy(o,company);}
    }
    if(!safe){carryFailure("Final-leg native loading checks failed. The job is assigned but was not automatically loaded.");return;}
    // Persist the attempt before calling the normal completion routine. A crash
    // or reload cannot replay it; fresh loaded telemetry is required afterward.
    carryPending=false;carryConfirming=true;beforeLoadRevision=job.revision;carryDeadline=now+15000;
    result(working,"loading","Preparing the carried final leg; waiting for game confirmation.");
    log(("[GT_TELEMETRY] CARRIED_LOAD "+working.id+" attempting once").c_str());
    if(!finishCarriedLoading(o.player,o.economy)){carryFailure("Native final-leg loading failed. Check the game; no retry.");return;}
}
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
SCSAPI_VOID liveEvent(const scs_event_t event,const void* info,const scs_context_t context);
static bool invokeCancelWrapper() {
    const auto* build=activeCancellationBuild();
    if(!build||!cancellationCodeMatches())return false;
#ifdef GT_CANCELLATION_TEST
    ++testCancelCalls;if(testCancelSucceeds&&testCancelEmit){scs_telemetry_gameplay_event_t event={};event.id="job.cancelled";liveEvent(SCS_TELEMETRY_EVENT_gameplay,&event,nullptr);}return testCancelSucceeds;
#else
    using Cancel=void(__fastcall*)(uintptr_t,unsigned char);
    __try {reinterpret_cast<Cancel>(imageBase+build->wrapper)(0,0);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
#endif
}
static std::string cancellationProfile() {
#ifdef GT_CANCELLATION_TEST
    return "Cancellation Test";
#else
    PWSTR documents=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&documents)))return {};
    const auto path=fs::path(documents)/dispatchGame::documents/L"game.log.txt";CoTaskMemFree(documents);
    std::ifstream stream(path,std::ios::binary|std::ios::ate);if(!stream)return {};
    const auto size=stream.tellg();if(size<=0||size>std::streamoff(32*1024*1024))return {};
    std::string data(static_cast<size_t>(size),'\0');stream.seekg(0);if(!stream.read(data.data(),size))return {};
    return ats::selectedProfileFromLog(data);
#endif
}
static bool cancellationProfileMatches(const std::string& id) {
    try{const auto name=cancellationProfile();return !name.empty()&&ats::profileFolderId(name)==id;}catch(...){return false;}
}
static void cancelReply(const std::string& id,const std::string& outcome) {
    live::writeJson(folder/"cancel_results"/(id+".json"),Json{{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"request_id",id},{"session_id",session},{"outcome",outcome},{"updated_utc_ms",live::utcMs()}});
}
static void pollCancel() {
    const auto file=folder/"cancel_request.json";if(!fs::exists(file))return;
    live::CancelRequest req;
    try{req=live::parseCancelRequest(live::readJson(file,8192));}
    catch(...){const auto invalid=folder/"duplicates"/(live::uniqueId()+".invalid-cancel.json");MoveFileExW(file.c_str(),invalid.c_str(),MOVEFILE_WRITE_THROUGH);return;}
    const auto claimed=folder/"cancel_claims"/(req.id+".json");
    if(!MoveFileExW(file.c_str(),claimed.c_str(),MOVEFILE_WRITE_THROUGH)){if(fs::exists(claimed)){const auto duplicate=folder/"duplicates"/(live::uniqueId()+".duplicate-cancel.json");MoveFileExW(file.c_str(),duplicate.c_str(),MOVEFILE_WRITE_THROUGH);}return;}
    Objects confirmed;
    if(!live::currentCancelRequest(req,session,live::utcMs())||driving||!menuSnapshot.stopped()||!menuSnapshot.ignitionKnown||callbackThread!=GetCurrentThreadId()||!cancellationProfileMatches(req.profileId)||!basicObjects(confirmed)||context(confirmed)!=req.context||cancellationTarget(confirmed.player)!=req.target) {cancelReply(req.id,"rejected_state");return;}
    try {
    // Stop the queue on the SDK thread before considering a native call. A
    // claimed request can never be retried by the normal dispatch path.
    live::stopQueuedWork(folder);
    if(chain.active()){chain.stop("Plan stopped at the driver's request.");saveChain();}
    awaiting=false;carryPending=carryConfirming=false;pickupNavigation={};
    uintptr_t economy=0,player=0,root=0,actor=0,ui=0,uiVtable=0,uiMethod=0,otherRoot=0,ordinary=0,car=0,bus=0;
    const auto* build=activeCancellationBuild();
    const bool supported=build&&cancellationCodeMatches();
    const bool objects=!disabled&&!nativeFault&&supported&&word(imageBase+economyRva,economy)&&liveUnit(economy)&&word(economy+0x18,player)&&liveUnit(player)&&word(player+0x28,ordinary)&&word(player+0x30,car)&&word(player+0x38,bus)&&word(imageBase+mainGameRva,root)&&root&&word(root+0x31b0,actor)&&liveUnit(actor)&&word(root+0x2b30,ui)&&ui&&word(ui,uiVtable)&&word(uiVtable+0x98,uiMethod)&&uiMethod&&word(imageBase+build->otherRoot,otherRoot)&&otherRoot;
    const bool pointersValid=objects&&(!ordinary||liveUnit(ordinary))&&(!car||liveUnit(car))&&(!bus||liveUnit(bus));
    const unsigned jobCount=(ordinary?1u:0u)+(car?1u:0u)+(bus?1u:0u);
    const bool any=pointersValid&&jobCount>0;
    if(!pointersValid){monitoring=false;resumedTracking=false;cancelReply(req.id,"game_unavailable");return;}
    if(!any){monitoring=false;resumedTracking=false;cancelReply(req.id,"no_game_job");return;}
    // Only the ordinary-job cancellation path has been mapped. Car, bus,
    // mixed or ambiguous jobs must not enter an unverified native branch.
    if(jobCount!=1||!ordinary||car||bus){monitoring=false;resumedTracking=false;cancelReply(req.id,"game_unavailable");return;}
    uintptr_t external=0,externalData=0;
    if(!word(ordinary+0x1a0,external)||external!=static_cast<uintptr_t>(-1)||!word(ordinary+0x48,externalData)||externalData){monitoring=false;resumedTracking=false;cancelReply(req.id,"game_unavailable");return;}
    pendingCancelId=req.id;observedCancelEvent=false;pendingCancelDeadline=GetTickCount64()+15000;
    const bool returned=invokeCancelWrapper();
    if(!returned){nativeFault=disabled=true;monitoring=false;resumedTracking=false;pendingCancelId.clear();cancelReply(req.id,"game_unconfirmed");return;}
    cancelReply(req.id,observedCancelEvent?"game_confirmed":"game_requested");
    if(observedCancelEvent)pendingCancelId.clear();
    }catch(const std::exception& ex){disabled=true;awaiting=monitoring=resumedTracking=false;log((std::string("[GT_TELEMETRY] Cancellation stop failed: ")+ex.what()).c_str());try{cancelReply(req.id,"stop_failed");}catch(...){}}
}
#endif
static void tick() {
    const auto now=GetTickCount64();
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
    pollCancel();if(!pendingCancelId.empty()&&now>pendingCancelDeadline){cancelReply(pendingCancelId,"game_unconfirmed");pendingCancelId.clear();if(monitoring){result(working,"ended_unconfirmed","Cancellation was requested but ATS did not confirm it.");monitoring=false;resumedTracking=false;}}
#endif
    bool cancelActive=false;
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
    cancelActive=!pendingCancelId.empty();
#endif
    if(!cancelActive&&!monitoring&&!awaiting&&!disabled)pollResume();
    if(!cancelActive&&!monitoring&&!awaiting&&!disabled)openForRecoveredJob();
    if(awaiting){
        if(job.assigned(working.route,beforeRevision)&&job.payloadMatches(expectedUnits,expectedUnitKg)){
            awaiting=false;monitoring=true;endedAt=0;
            if(carryMode){carryPending=true;carryDeadline=now+30000;chain.state="second_loading";chain.message="Preparing the carried final leg.";saveChain();result(working,"assigned","Final leg assigned. Keep the truck stopped while its carried load is prepared.");}
            else {pickupNavigation.arm(now);result(working,"assigned","Job assigned. Keep the truck stopped while pickup navigation is prepared.");}
            setState("job_assigned");}
        else if((job.revision>beforeRevision&&job.active)||now>deadline){awaiting=false;disabled=true;result(working,"uncertain","The native call returned, but fresh matching unloaded-job telemetry was not confirmed. Check the game; no automatic retry.");setState("disabled_uncertain_assignment");}
    }
    if(!cancelActive&&monitoring&&resumedTracking&&job.matches(working.route)) {Objects o;if(basicObjects(o)&&context(o)!=working.context){result(working,"ended_unconfirmed","Game context changed while monitoring the resumed job.");monitoring=false;resumedTracking=false;}}
    if(!cancelActive&&monitoring&&job.matches(working.route)&&job.payloadKnown&&!job.payloadMatches(expectedUnits,expectedUnitKg)){
        result(working,"uncertain","The game cargo quantity/weight changed unexpectedly. Check the job before continuing.");disabled=true;monitoring=false;setState("disabled_payload_mismatch");
    }
    if(!cancelActive&&monitoring&&!disabled){
        if(!disabled){if(carryMode)prepareCarriedLoad();else guideToPickup();}
        if(!carryMode&&job.matches(working.route)&&job.loadedKnown&&job.loaded&&lastProgress!="loaded"){result(working,"loaded","Cargo loaded. Drive and deliver normally.");setState("cargo_loaded");}
        else if(!job.active){if(!endedAt)endedAt=now;else if(now-endedAt>3000){result(working,"ended_unconfirmed","Job ended without a confirmed delivery or cancellation event.");monitoring=false;endedAt=0;}}
        else if(!job.matches(working.route)){result(working,"uncertain","The active game job no longer matches this dispatch.");monitoring=false;disabled=true;setState("disabled_job_mismatch");}
    }
    if(!monitoring)resumedTracking=false;
    bool cancellationInFlight=false;
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
    cancellationInFlight=!pendingCancelId.empty();
#endif
    if(!cancellationInFlight&&!disabled&&!awaiting){uintptr_t registry=0;if(!assignmentCodeMatches()){disabled=true;setState("disabled_native_code_changed");}
        else if(registryReady(registry)){tryHandoff(registry);if(!monitoring&&!awaiting&&!fs::exists(folder/"request.json"))setState(chain.state=="waiting_handoff"?"waiting_two_stop_handoff":job.active||carActive||busActive?"game_job_active":"ready");pollRequest(registry);}else setState("waiting_game_economy");}
    if(nativeFault){disabled=true;setState("disabled_native_fault");}
    // Heartbeat publication is read-only diagnostics. A transient replacement
    // failure must not permanently disable dispatch or replay a game mutation.
    try{publish();}catch(const std::exception& ex){static ULONGLONG lastReport=0;const auto stamp=GetTickCount64();if(!lastReport||stamp-lastReport>30000){lastReport=stamp;log((std::string("[GT_TELEMETRY] Heartbeat write deferred: ")+ex.what()).c_str());}}
}
SCSAPI_VOID liveSpeed(const scs_string_t,const scs_u32_t,const scs_value_t* v,const scs_context_t) {if(v&&v->type==SCS_VALUE_TYPE_float){speed=v->value_float.value;lastSpeed=GetTickCount64();}else lastSpeed=0;}
SCSAPI_VOID liveIgnition(const scs_string_t,const scs_u32_t,const scs_value_t* v,const scs_context_t) {if(v&&v->type==SCS_VALUE_TYPE_bool){ignitionOn=v->value_bool.value!=0;lastIgnition=GetTickCount64();}else lastIgnition=0;}
SCSAPI_VOID liveTrailer(const scs_string_t,const scs_u32_t,const scs_value_t* v,const scs_context_t) {if(v&&v->type==SCS_VALUE_TYPE_bool){trailer.connected=v->value_bool.value!=0;lastTrailer=GetTickCount64();}else lastTrailer=0;}
SCSAPI_VOID livePosition(const scs_string_t,const scs_u32_t,const scs_value_t* v,const scs_context_t) {
    lastPosition=0;if(!v||v->type!=SCS_VALUE_TYPE_dplacement)return;
    const auto& p=v->value_dplacement.position;if(!std::isfinite(p.x)||!std::isfinite(p.z))return;
    truckX=p.x;truckZ=p.z;lastPosition=GetTickCount64();
}
SCSAPI_VOID liveNavigationDistance(const scs_string_t,const scs_u32_t,const scs_value_t* v,const scs_context_t) {if(v&&v->type==SCS_VALUE_TYPE_float){navigationDistance=v->value_float.value;lastNavigationDistance=GetTickCount64();}else lastNavigationDistance=0;}
SCSAPI_VOID liveEvent(const scs_event_t event,const void* info,const scs_context_t) {
    try {
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
        if(event==SCS_TELEMETRY_EVENT_gameplay&&info&&!pendingCancelId.empty()) {const auto g=static_cast<const scs_telemetry_gameplay_event_t*>(info);if(g->id&&(strcmp(g->id,"job.cancelled")==0||strcmp(g->id,"car_job.cancelled")==0||strcmp(g->id,"bus_job.cancelled")==0)){observedCancelEvent=true;cancelReply(pendingCancelId,"game_confirmed");pendingCancelId.clear();}}
#endif
        if(event==SCS_TELEMETRY_EVENT_configuration&&info){const auto c=static_cast<const scs_telemetry_configuration_t*>(info);if(!c->id)return;const bool active=c->attributes&&c->attributes[0].name;
            if(strcmp(c->id,"job")==0){jobSeen=true;job.update(c->attributes);}else if(strcmp(c->id,"car_job")==0){carSeen=true;carActive=active;}else if(strcmp(c->id,"bus_job")==0){busSeen=true;busActive=active;}else if(strcmp(c->id,"trailer")==0||strcmp(c->id,"trailer.0")==0)trailer.update(c->attributes);return;}
        if(event==SCS_TELEMETRY_EVENT_gameplay&&info&&monitoring){const auto g=static_cast<const scs_telemetry_gameplay_event_t*>(info);
            if(g->id&&(strcmp(g->id,"job.delivered")==0||strcmp(g->id,"job.cancelled")==0)){
                const bool delivered=strcmp(g->id,"job.delivered")==0;Json details=Json::object();
                if(g->attributes)for(auto a=g->attributes;a->name;++a){if(a->value.type==SCS_VALUE_TYPE_u64)details[a->name]=a->value.value_u64.value;else if(a->value.type==SCS_VALUE_TYPE_s64)details[a->name]=a->value.value_s64.value;else if(a->value.type==SCS_VALUE_TYPE_float)details[a->name]=a->value.value_float.value;else if(a->value.type==SCS_VALUE_TYPE_u32)details[a->name]=a->value.value_u32.value;}
                if(chain.active()) {
                    if(delivered){if(!chain.delivered(working.id))chain.stop("A delivery event arrived before the expected plan phase.",true);}
                    else chain.stop("The active leg was cancelled; the next leg will not be assigned.");
                    saveChain();
                }
                const auto message=delivered?(chain.state=="waiting_handoff"?"First delivery confirmed. Return to Drive stopped for the loaded final leg.":chain.state=="complete"?"Both deliveries confirmed. Plan complete.":"Delivery confirmed. You can dispatch another job."):"Job cancellation confirmed.";
                result(working,delivered?"delivered":"cancelled",message,details);monitoring=false;resumedTracking=false;carryPending=carryConfirming=false;endedAt=0;setState(delivered?"delivery_confirmed":"cancellation_confirmed");}return;}
        if(event==SCS_TELEMETRY_EVENT_started){driving=true;menuSnapshot.beginDriving();lastSpeed=lastTrailer=lastIgnition=0;return;}
        if(event==SCS_TELEMETRY_EVENT_paused){menuSnapshot.pause(GetTickCount64(),lastSpeed,speed,lastIgnition,ignitionOn);driving=false;lastSpeed=lastTrailer=lastIgnition=0;return;}
        if(event!=SCS_TELEMETRY_EVENT_frame_end)return;
        if(callbackThread!=GetCurrentThreadId()){disabled=true;return;}
        appHotkey::poll();
        const auto now=GetTickCount64();if(now-lastPoll<1000)return;lastPoll=now;tick();
    }catch(const std::exception& ex){disabled=true;log((std::string("[GT_TELEMETRY] Disabled after storage/protocol error: ")+ex.what()).c_str());}
}
static void startupRejected(const char* reason,const std::string& hash) {
    log((std::string("[GT_TELEMETRY] Startup rejected: ")+reason+" Host SHA-256: "+hash).c_str());
    wchar_t executable[32768];if(!GetModuleFileNameW(nullptr,executable,32768)||_wcsicmp(fs::path(executable).filename().c_str(),dispatchGame::executable)!=0)return;
    try{const auto ipc=live::ipcFolder();fs::create_directories(ipc);live::writeJson(ipc/"status.json",Json{{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"online",false},{"pid",GetCurrentProcessId()},{"state","startup_rejected"},{"message",reason},{"host_sha256",hash},{"updated_utc_ms",live::utcMs()}});}catch(const std::exception& ex){log(ex.what());}
}
SCSAPI_RESULT scs_telemetry_init(const scs_u32_t version,const scs_telemetry_init_params_t* params) {
    if(version!=SCS_TELEMETRY_VERSION_1_01)return SCS_RESULT_unsupported;if(!params)return SCS_RESULT_invalid_parameter;
    const auto p=static_cast<const scs_telemetry_init_params_v101_t*>(params);gameLog=p->common.log;
    if(!p->common.game_id||strcmp(p->common.game_id,dispatchGame::sdkId)!=0)return SCS_RESULT_unsupported;
    std::string observedHash;
    if(!exactHost(&observedHash)){startupRejected(observedHash.empty()?"GT Telemetry could not read the game executable. Check the game log.":"This game executable is not supported by the installed GT Telemetry DLL. A game update requires a compatible DLL.",observedHash);return SCS_RESULT_generic_error;}
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));if(!assignmentCodeMatches()){startupRejected("GT Telemetry's native code checks failed. The game build needs verification before dispatch can run.",observedHash);return SCS_RESULT_generic_error;}
    if(!p->register_for_event||!p->unregister_from_event||!p->register_for_channel||!p->unregister_from_channel)return SCS_RESULT_invalid_parameter;
    try {
        folder=live::ipcFolder();for(const auto& dir:{folder,folder/"claims",folder/"results",folder/"duplicates",folder/"plans",folder/"cancel_claims",folder/"cancel_results"})fs::create_directories(dir);
        session=live::uniqueId();callbackThread=GetCurrentThreadId();lastPoll=lastSpeed=lastTrailer=deadline=endedAt=0;disabled=nativeFault=driving=jobSeen=carSeen=busSeen=carActive=busActive=awaiting=monitoring=false;
        lastPosition=0;truckX=truckZ=0;
        job={};trailer={};working={};pickupNavigation={};chain={};menuSnapshot={};lastIgnition=0;ignitionOn=false;resumedTracking=false;autoOpenedRequest.clear();lastAutoOpenCheck=0;carryMode=carryPending=carryConfirming=false;carryDeadline=0;beforeLoadRevision=0;expectedUnits=routeMaximumUnits=0;expectedUnitKg=0;navigationDistance=0;lastNavigationDistance=0;beforeRevision=0;state="loaded";lastId.clear();lastProgress.clear();
#ifdef GT_ENABLE_EXPERIMENTAL_CANCELLATION
        pendingCancelId.clear();observedCancelEvent=false;pendingCancelDeadline=0;
#endif
        // Verify status storage before registering callbacks, so an I/O failure
        // cannot leave partially registered callbacks after failed initialization.
        publish();
        const scs_event_t events[]={SCS_TELEMETRY_EVENT_started,SCS_TELEMETRY_EVENT_paused,SCS_TELEMETRY_EVENT_configuration,SCS_TELEMETRY_EVENT_gameplay,SCS_TELEMETRY_EVENT_frame_end};
        size_t registered=0;for(auto e:events){if(p->register_for_event(e,liveEvent,nullptr)!=SCS_RESULT_ok){for(size_t i=0;i<registered;++i)p->unregister_from_event(events[i]);return SCS_RESULT_generic_error;}++registered;}
        if(p->register_for_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float,3,liveSpeed,nullptr)!=SCS_RESULT_ok){for(auto e:events)p->unregister_from_event(e);return SCS_RESULT_generic_error;}
        if(p->register_for_channel("trailer.connected",SCS_U32_NIL,SCS_VALUE_TYPE_bool,3,liveTrailer,nullptr)!=SCS_RESULT_ok){p->unregister_from_channel("truck.speed",SCS_U32_NIL,SCS_VALUE_TYPE_float);for(auto e:events)p->unregister_from_event(e);return SCS_RESULT_generic_error;}
        // Optional; unknown ignition remains unknown if the channel is unavailable.
        p->register_for_channel("truck.electric.enabled",SCS_U32_NIL,SCS_VALUE_TYPE_bool,3,liveIgnition,nullptr);
        // Optional read-only diagnostic channel; assignment remains available if
        // another SDK implementation does not expose navigation distance.
        p->register_for_channel("truck.navigation.distance",SCS_U32_NIL,SCS_VALUE_TYPE_float,3,liveNavigationDistance,nullptr);
        p->register_for_channel("truck.world.placement",SCS_U32_NIL,SCS_VALUE_TYPE_dplacement,3,livePosition,nullptr);
        if(!appHotkey::initialize())log("[GT_TELEMETRY] Shift+F9 listener could not start; dispatch remains available.");
        log((std::string("[GT_TELEMETRY] Build: ")+dispatchGame::build).c_str());log("[GT_TELEMETRY] Configurable live dispatcher loaded (protocol 4). Requests are game-isolated, session-bound and claimed before mutation.");return SCS_RESULT_ok;
    }catch(const std::exception& ex){log(ex.what());return SCS_RESULT_generic_error;}
}
SCSAPI_VOID scs_telemetry_shutdown() {
    appHotkey::shutdown();
    try {if(!session.empty())live::writeJson(folder/"status.json",Json{{"protocol",live::protocolVersion},{"game",dispatchGame::id},{"session_id",session},{"online",false},{"updated_utc_ms",live::utcMs()},{"state","unloaded"}});}catch(...){}
    gameLog=nullptr;disabled=true;
}

