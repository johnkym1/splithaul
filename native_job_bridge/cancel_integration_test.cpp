// Isolated callback-thread simulation. No game process or real native routine is
// used: the test-only macro replaces the cancellation wrapper with a stub.
#define GT_ENABLE_EXPERIMENTAL_CANCELLATION
#define GT_CANCELLATION_TEST
#include "live_dispatch.cpp"
#include <iostream>

template<class T> static void put(std::vector<unsigned char>& bytes,size_t offset,T value) {memcpy(bytes.data()+offset,&value,sizeof(value));}
static uintptr_t ptr(std::vector<unsigned char>& bytes){return reinterpret_cast<uintptr_t>(bytes.data());}
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main() {
    try {
        std::vector<unsigned char> image(0x3800000),economy(0x400),player(0x300),vehicles(0x100),owned(0x300),definition(0x100),root(0x3300),actor(0x100),ui(0x100),table(0x100),other(0x100),jobUnit(0x300);
        imageBase=ptr(image);require(selectHost(cancellationBuild.hash),"Cannot select pinned test host");
        require(activeCancellationBuild()!=nullptr,"Pinned cancellation build unavailable");
        for(auto* unit:{&economy,&player,&vehicles,&owned,&definition,&actor,&jobUnit})put(*unit,8,uint32_t(0x80000000));
        put(image,economyRva,ptr(economy));put(image,mainGameRva,ptr(root));put(image,cancellationBuild.otherRoot,ptr(other));
        put(economy,0x18,ptr(player));put(player,0xa0,ptr(vehicles));put(player,0x28,ptr(jobUnit));put(jobUnit,0x1a0,static_cast<uintptr_t>(-1));put(vehicles,0x48,ptr(owned));put(owned,0x1a0,ptr(definition));
        put(root,0x31b0,ptr(actor));put(root,0x2b30,ptr(ui));put(ui,0,ptr(table));put(table,0x98,ptr(actor));
        const unsigned char code[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x48};memcpy(image.data()+cancellationBuild.wrapper,code,sizeof(code));
        require(cancellationCodeMatches(),"Mapped wrapper prefix rejected");
        require(!selectHost("unknown")&&!activeCancellationBuild()&&!cancellationCodeMatches(),"Unknown host enabled cancellation");
#ifndef GT_TARGET_ETS2
        require(selectHost(hostBuilds[0].hash)&&!activeCancellationBuild()&&!cancellationCodeMatches(),"Older dispatch host enabled cancellation");
#endif
        require(selectHost(cancellationBuild.hash),"Cannot restore pinned test host");
        folder=live::fs::absolute(live::fs::path("native_job_bridge/build/cancel-test-data")/live::uniqueId());
        for(const auto& dir:{folder,folder/"claims",folder/"duplicates",folder/"results",folder/"plans",folder/"cancel_claims",folder/"cancel_results"})fs::create_directories(dir);
        session=live::uniqueId();callbackThread=GetCurrentThreadId();driving=disabled=nativeFault=awaiting=monitoring=false;job.revision=7;menuSnapshot.pause(10000,9999,0,9999,false);
        live::Request dispatch;dispatch.id=live::uniqueId();dispatch.session=session;dispatch.context="test";dispatch.trailer="test";dispatch.created=live::utcMs();dispatch.expires=dispatch.created+20000;dispatch.route={"wood","alpha","depot_a","beta","depot_b"};
        dispatch.twoStop=true;dispatch.secondId=live::uniqueId();dispatch.finalRoute={"wood","alpha","depot_a","gamma","depot_c"};chain.start(dispatch);working=dispatch;monitoring=true;
        live::writeJson(folder/"request.json",live::requestJson(dispatch),false);live::writeJson(folder/"resume.json",Json{{"test",true}},false);
        Objects objects;require(basicObjects(objects),"Mock game objects are invalid");
        live::CancelRequest request;request.id=live::uniqueId();request.session=session;request.context=context(objects);request.profileId=ats::profileFolderId("Cancellation Test");request.target=cancellationTarget(objects.player);request.created=live::utcMs();request.expires=request.created+20000;
        auto wrongGame=live::cancelRequestJson(request);wrongGame["game"]=dispatchGame::isETS2()?"ats":"ets2";
        live::writeJson(folder/"cancel_request.json",wrongGame,false);pollCancel();
        require(testCancelCalls==0&&chain.active()&&fs::exists(folder/"request.json")&&!fs::exists(folder/"cancel_request.json"),"Cross-game cancellation affected current game");
        live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==1,"Cancellation wrapper stub was not invoked exactly once");
        require(live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_confirmed","Confirmed result was not written");
        require(live::readJson(folder/"cancel_results"/(request.id+".json")).at("game")==dispatchGame::id,"Cancellation reply has wrong game");
        require(live::readJson(folder/"results"/(dispatch.id+".json")).at("state")=="cancelled","Simulated game event was not recorded");
        require(!chain.active()&&!chain.canHandoff(true,true,true),"Two-stop handoff was not stopped");
        require(!fs::exists(folder/"request.json")&&!fs::exists(folder/"resume.json")&&fs::exists(folder/"claims"/(dispatch.id+".json")),"Queued work was not durably stopped");
        live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();require(testCancelCalls==1&&!fs::exists(folder/"cancel_request.json"),"Duplicate cancellation invoked twice");
        auto secondDispatch=dispatch;secondDispatch.id=live::uniqueId();secondDispatch.twoStop=false;working=secondDispatch;monitoring=true;testCancelEmit=false;
        request.id=live::uniqueId();live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&monitoring&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_requested","Delayed event was treated as confirmed or monitoring stopped");
        scs_telemetry_gameplay_event_t delayed={};delayed.id="job.cancelled";liveEvent(SCS_TELEMETRY_EVENT_gameplay,&delayed,nullptr);
        require(!monitoring&&pendingCancelId.empty()&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_confirmed"&&live::readJson(folder/"results"/(secondDispatch.id+".json")).at("state")=="cancelled","Delayed game event was not recorded");
        testCancelEmit=true;
        auto reject=[&](const char* outcome,const char* message){request.id=live::uniqueId();live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")==outcome,message);};
        image[cancellationBuild.wrapper]=0;require(!cancellationCodeMatches(),"Modified wrapper advertised support");reject("game_unavailable","Modified wrapper invoked cancellation");image[cancellationBuild.wrapper]=code[0];
        const auto pinnedHash=activeHostHash;activeHostHash="unknown";reject("game_unavailable","Unknown cancellation host invoked wrapper");activeHostHash=pinnedHash;
        menuSnapshot.speedMps=2;reject("rejected_state","Moving truck invoked cancellation");menuSnapshot.speedMps=0;
        callbackThread=0;reject("rejected_state","Wrong callback thread invoked cancellation");callbackThread=GetCurrentThreadId();
        const auto correctSession=request.session;request.session=live::uniqueId();reject("rejected_state","Old session invoked cancellation");request.session=correctSession;
        request.id=live::uniqueId();request.target="different-target";live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Changed job was not rejected");
        request.id=live::uniqueId();request.target=cancellationTarget(objects.player);driving=true;live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Drive mode did not block cancellation");
        driving=false;request.id=live::uniqueId();request.profileId=ats::profileFolderId("Other Profile");live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Different profile was not rejected");
        request.id=live::uniqueId();request.profileId=ats::profileFolderId("Cancellation Test");request.context="stale:context";live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Changed game context was not rejected");
        request.id=live::uniqueId();request.context=context(objects);menuSnapshot.ignitionKnown=false;live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Unknown ignition was accepted");menuSnapshot.ignitionKnown=true;
        request.id=live::uniqueId();request.profileId=ats::profileFolderId("Cancellation Test");request.created=live::utcMs()-31000;request.expires=request.created+30000;live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="rejected_state","Expired cancellation was not rejected");
        request.created=live::utcMs();request.expires=request.created+20000;request.id=live::uniqueId();put(player,0x28,uintptr_t(0));request.target=cancellationTarget(objects.player);live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="no_game_job","No-job state invoked cancellation");
        request.id=live::uniqueId();put(player,0x28,ptr(jobUnit));request.target=cancellationTarget(objects.player);put(table,0x98,uintptr_t(0));live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_unavailable","Missing game object invoked cancellation");
        request.id=live::uniqueId();put(table,0x98,ptr(actor));put(player,0x30,ptr(jobUnit));request.target=cancellationTarget(objects.player);live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_unavailable","Multiple game jobs invoked an ambiguous cancellation");
        request.id=live::uniqueId();put(player,0x30,uintptr_t(0));put(jobUnit,0x1a0,uintptr_t(123));request.target=cancellationTarget(objects.player);live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==2&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_unavailable","External job invoked an unverified cancellation");
        request.id=live::uniqueId();put(jobUnit,0x1a0,static_cast<uintptr_t>(-1));testCancelSucceeds=false;live::writeJson(folder/"cancel_request.json",live::cancelRequestJson(request),false);pollCancel();
        require(testCancelCalls==3&&disabled&&live::readJson(folder/"cancel_results"/(request.id+".json")).at("outcome")=="game_unconfirmed","Failed native call was treated as confirmed");
        std::cout<<"PASS: "<<dispatchGame::id<<" isolated cancellation checks: host/code pinning, cross-game rejection, queue, plan, replay, context, profile, menu, thread, session, expiry, no-job and failure; native call stubbed.\n";return 0;
    }catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}
}
