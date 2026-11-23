#include "../dispatch_shared/protocol.h"
#include "host_builds.h"
#include <cassert>
#include <cstdio>
int main() {
    assert(std::string(dispatchGame::id)=="ets2");
    assert(std::string(dispatchGame::sdkId)=="eut2");
    assert(live::ipcFolder().filename()==L"MSDCLiveDispatchETS2");
    assert(std::string(dispatchGame::profiles)=="profiles_ets2");
    assert(std::wstring(dispatchGame::executable)==L"eurotrucks2.exe");
    assert(selectHost("055ec5343d53bd427de54b06eca74937f0d348dbb0c5d71862edddcf45e2db66"));
    assert(handlerRva==0x5bf3d0 && acceptRva==0x7899f0 && economyRva==0x30aa900);
    assert(!selectHost("63ecae85a070934530d9fd971d4d56e5479f1214d7077b403f79b8981c613e08"));
    assert(!registryRva && !acceptRva && !activeHostHash);
    assert(!selectHost("unknown"));
    live::Request r;r.route={"apples","paris","depot_a","lyon","depot_b"};
    r.id=std::string(32,'a');r.session=std::string(32,'b');r.context="test";r.trailer="trailer";r.created=100;r.expires=1000;
    auto j=live::requestJson(r);assert(j.at("game")=="ets2");assert(live::parseRequest(j).id==r.id);
    j["game"]="ats";bool rejected=false;try{live::parseRequest(j);}catch(...){rejected=true;}assert(rejected);
    live::Json status={{"protocol",live::protocolVersion},{"game","ets2"},{"session_id",r.session},{"updated_utc_ms",100},{"online",true}};
    assert(live::freshStatus(status,100));status["game"]="ats";assert(!live::freshStatus(status,100));
    status["game"]="ets2";assert(!live::freshStatus(status,10000));
    puts("PASS: ETS2 host pinning, ATS/unknown host rejection, address clearing, isolated storage, request roundtrip, cross-game request/status rejection, stale heartbeat rejection.");
}
