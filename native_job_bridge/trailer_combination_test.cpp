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
    put(second.unit,0x1b8,ptr(dolly2.unit));put(dolly2.unit,0x1b8,ptr(third.unit));c=read();check(c.known&&c.trailers()==3&&c.dollies()==2&&c.supported(),"Triple rejected");
    trailer.configured=true;trailer.id="vehicle.scs.box";trailer.body="dryvan";trailer.chain="single";trailer.combination=c;
    check(trailer.supported(),"Physical triple rejected");
    check(live::supportedTrailer(trailer.json())==trailer.supported(),"App and plugin limits disagree");
    check(trailer.signature().size()<512&&!trailer.signature().empty(),"Combination signature exceeds protocol bound");
    const auto prior=trailer.signature();third.body[third.body.find("dryvan")]='x';trailer.combination=read();check(trailer.signature()!=prior,"Rear body change retained request signature");
    put(third.unit,0x1b8,ptr(fourth.unit));c=read();check(c.known&&c.trailers()==4&&c.supported(),"Quad rejected");
    FixturePart dolly3(base+"chassis/dolly_conv_2a.sii"),fifth(base+"chassis/ch_28.sii",base+"body/dryvan_28.sii");
    put(third.unit,0x1b8,ptr(dolly3.unit));put(dolly3.unit,0x1b8,ptr(fourth.unit));c=read();
    check(c.known&&c.supported()&&c.trailers()==4&&c.dollies()==3,"Seven-component A-quad rejected");
    put(fourth.unit,0x1b8,ptr(fifth.unit));c=read();check(c.known&&!c.supported()&&c.trailers()==5,"Five cargo trailers accepted");
    trailer.combination=c;check(!live::supportedTrailer(trailer.json()),"App accepted five cargo trailers");
    put(fourth.unit,0x1b8,uintptr_t(0));
    put(third.unit,0x1b8,ptr(first.unit));check(!read().known,"Cyclic trailer chain accepted");put(third.unit,0x1b8,uintptr_t(0));
    put(dolly.unit,0x1b8,uintptr_t(0));check(!read().known,"Trailing dolly accepted");put(dolly.unit,0x1b8,ptr(second.unit));
    put(first.unit,0x20,uintptr_t(257));check(!read().known,"Oversized accessory array accepted");put(first.unit,0x20,uintptr_t(2));
    put(second.chassisAccessory,0x20,uint32_t(2048));check(!read().known,"Oversized accessory path accepted");put(second.chassisAccessory,0x20,static_cast<uint32_t>(second.chassis.size()));
    put(second.unit,0x18,uintptr_t(1));check(!read().known,"Unreadable accessory array accepted");put(second.unit,0x18,reinterpret_cast<uintptr_t>(second.accessories.data()));
    check(!recognisedDolly("/def/vehicle/trailer_owned/custom/chassis/dolly_conv.sii",""),"Unknown custom chassis excluded from count");
    check(!recognisedDolly(base+"chassis/dolly_conv.sii",base+"body/dryvan.sii"),"Load-bearing dolly excluded from count");
    put(image,layout.slave,uintptr_t(0x999));check(!read().known,"Changed native layout accepted");put(image,layout.slave,uintptr_t(0x1b8));
    // Exercise the real native capacity reader with an isolated double tanker.
    const unsigned char capacityCode[]={0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55,0x48,0x8d,0xac,0x24,0x10};
    const unsigned char cityCode[]={0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x81,0x00,0x01,0x00,0x00,0x48,0x8b,0xd9};
    memcpy(image.data()+capacityRva,capacityCode,sizeof(capacityCode));memcpy(image.data()+companyCityRva,cityCode,sizeof(cityCode));
    std::vector<unsigned char> definition(0x100),cargo(0x100);Objects o;o.definition=ptr(definition);o.cargo=ptr(cargo);
    put(definition,0x30,71500.0f);put(definition,0x34,13880.0f);put(definition,0x38,0.0f);put(definition,0x3c,uint32_t(8));put(definition,0x40,60.0f);
    put(cargo,0x98,1.0f);put(cargo,0x9c,850.0f);float unitKg=0;
    check(maximumForRoute(o,unitKg,true)==60&&unitKg==850,"Double tanker must load 60 diesel units (51 tonnes)");
    put(definition,0x30,20000.0f);check(maximumForRoute(o,unitKg,true)==7,"Full capacity exceeded trailer payload limit");
    put(cargo,0x98,0.0f);check(maximumForRoute(o,unitKg,true)==0,"Full capacity accepted missing cargo volume");put(cargo,0x98,1.0f);
    put(definition,0x34,21000.0f);check(maximumForRoute(o,unitKg,true)==0,"Full capacity accepted invalid trailer tare");put(definition,0x34,13880.0f);
    image[capacityRva]=0;check(maximumForRoute(o,unitKg,true)==0,"Full capacity bypassed native layout pin");
    activeHostHash="unknown";check(!read().known,"Unpinned native layout accepted");
    check(!live::supportedTrailer(live::Json{{"chain_type","single"}}),"Old plugin bypassed physical verification");
    std::cout<<"PASS: "<<checks<<" "<<dispatchGame::id<<" trailer-combination checks.\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
