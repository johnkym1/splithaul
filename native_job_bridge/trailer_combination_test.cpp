// Native reads against isolated synthetic units; never opens or mutates a game.
#include "live_dispatch.cpp"
#include <iostream>
#include <memory>
static unsigned checks=0;
static void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
template<class T>static void put(std::vector<unsigned char>& b,size_t at,T v){memcpy(b.data()+at,&v,sizeof(v));}
static uintptr_t ptr(std::vector<unsigned char>& b){return reinterpret_cast<uintptr_t>(b.data());}
struct FixturePart {
    std::vector<unsigned char> unit=std::vector<unsigned char>(0x200),chassisAccessory=std::vector<unsigned char>(0x40),bodyAccessory=std::vector<unsigned char>(0x40);
    std::vector<uintptr_t> accessories;std::string chassis,body;
    FixturePart(std::string c,std::string b=""):chassis(std::move(c)),body(std::move(b)) {
        for(auto* v:{&unit,&chassisAccessory,&bodyAccessory})put(*v,8,uint32_t(0x80000000));
        put(chassisAccessory,0x18,reinterpret_cast<uintptr_t>(chassis.c_str()));put(chassisAccessory,0x20,static_cast<uint32_t>(chassis.size()));
        put(bodyAccessory,0x18,reinterpret_cast<uintptr_t>(body.c_str()));put(bodyAccessory,0x20,static_cast<uint32_t>(body.size()));
        accessories.push_back(ptr(chassisAccessory));if(!body.empty())accessories.push_back(ptr(bodyAccessory));
        put(unit,0x18,reinterpret_cast<uintptr_t>(accessories.data()));put(unit,0x20,static_cast<uintptr_t>(accessories.size()));
    }
};
int main(){try {
    const auto& layout=trailerLayouts[dispatchGame::isETS2()?1:0];std::vector<unsigned char> image(0x3000000);imageBase=ptr(image);
    check(selectHost(layout.hash),"Host pin missing");
    auto field=[&](uintptr_t r,uintptr_t off,uintptr_t type,const char* name){put(image,r,off);put(image,r+8,type);put(image,r+24,reinterpret_cast<uintptr_t>(name));};
    field(layout.slave,0x1b8,61,"slave_trailer");field(layout.accessories,0x10,62,"accessories");field(layout.path,0x10,1,"data_path");field(layout.definition,0x1a0,65,"trailer_definition");
    check(trailerLayoutMatches(),"Verified reflection layout rejected");
    const std::string base="/def/vehicle/trailer_owned/scs.box/";
    FixturePart first(base+"chassis/ch_28.sii",base+"body/dryvan_28.sii"),second(base+"chassis/ch_28.sii",base+"body/dryvan_28.sii"),third(base+"chassis/ch_28.sii",base+"body/dryvan_28.sii"),fourth(base+"chassis/ch_28.sii"),dolly(base+"chassis/dolly_conv.sii"),dolly2(base+"chassis/dolly_conv_2a.sii");
    auto read=[&](){return readTrailerCombination(ptr(first.unit));};
    auto c=read();check(c.known&&c.supported()&&c.trailers()==1&&c.dollies()==0,"Single trailer failed");const auto single=c.signature();
    put(first.unit,0x1b8,ptr(second.unit));c=read();check(c.supported()&&c.trailers()==2&&c.dollies()==0&&c.signature()!=single,"B-double failed");
    put(first.unit,0x1b8,ptr(dolly.unit));put(dolly.unit,0x1b8,ptr(second.unit));c=read();check(c.supported()&&c.trailers()==2&&c.dollies()==1,"Dolly was counted as a cargo trailer");
    put(second.unit,0x1b8,ptr(dolly2.unit));put(dolly2.unit,0x1b8,ptr(third.unit));c=read();check(c.known&&c.trailers()==3&&c.dollies()==2&&c.supported()==!dispatchGame::isETS2(),"Game-specific triple limit failed");
    trailer.configured=true;trailer.id="vehicle.scs.box";trailer.body="dryvan";trailer.chain="single";trailer.combination=c;
    check(trailer.supported()==!dispatchGame::isETS2(),"Mislabelled single bypassed physical count");
    check(live::supportedTrailer(trailer.json())==trailer.supported(),"App and plugin limits disagree");
    check(trailer.signature().size()<512&&!trailer.signature().empty(),"Combination signature exceeds protocol bound");
    const auto prior=trailer.signature();third.body[third.body.find("dryvan")]='x';trailer.combination=read();check(trailer.signature()!=prior,"Rear body change retained request signature");
    put(third.unit,0x1b8,ptr(fourth.unit));c=read();check(c.known&&c.trailers()==4&&!c.supported(),"Four cargo trailers accepted");
    put(third.unit,0x1b8,ptr(first.unit));check(!read().known,"Cyclic trailer chain accepted");put(third.unit,0x1b8,uintptr_t(0));
    put(dolly.unit,0x1b8,uintptr_t(0));check(!read().known,"Trailing dolly accepted");put(dolly.unit,0x1b8,ptr(second.unit));
    put(first.unit,0x20,uintptr_t(257));check(!read().known,"Oversized accessory array accepted");put(first.unit,0x20,uintptr_t(2));
    put(second.chassisAccessory,0x20,uint32_t(2048));check(!read().known,"Oversized accessory path accepted");put(second.chassisAccessory,0x20,static_cast<uint32_t>(second.chassis.size()));
    put(second.unit,0x18,uintptr_t(1));check(!read().known,"Unreadable accessory array accepted");put(second.unit,0x18,reinterpret_cast<uintptr_t>(second.accessories.data()));
    check(!recognisedDolly("/def/vehicle/trailer_owned/custom/chassis/dolly_conv.sii",""),"Unknown custom chassis excluded from count");
    check(!recognisedDolly(base+"chassis/dolly_conv.sii",base+"body/dryvan.sii"),"Load-bearing dolly excluded from count");
    put(image,layout.slave,uintptr_t(0x999));check(!read().known,"Changed native layout accepted");put(image,layout.slave,uintptr_t(0x1b8));
    activeHostHash="unknown";check(!read().known,"Unpinned native layout accepted");
    check(!live::supportedTrailer(live::Json{{"chain_type","single"}}),"Old plugin bypassed physical verification");
    std::cout<<"PASS: "<<checks<<" "<<dispatchGame::id<<" trailer-combination checks.\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
