#pragma once
#include "../dispatch_shared/trailer_combination.h"

// Read-only reflection evidence: research/multi-trailer-20261125/layout.json.
// These are field descriptors in the exact pinned executables, not new calls.
struct TrailerLayout {const char* hash;uintptr_t slave,accessories,path,definition;};
static constexpr TrailerLayout trailerLayouts[]={
    {"63ecae85a070934530d9fd971d4d56e5479f1214d7077b403f79b8981c613e08",0x2debde0,0x2dd3718,0x2df0bd0,0x2debc78},
    {"055ec5343d53bd427de54b06eca74937f0d348dbb0c5d71862edddcf45e2db66",0x2de5080,0x2dcc9b8,0x2de9e70,0x2de4f18},
    {"28ca69a7eca99519371adcc3e78b978946ad28ce57332780e9fe4175be92f6a7",0x2deabc0,0x2dd2648,0x2def980,0x2deaa58}
};
static bool trailerField(uintptr_t rva,uintptr_t offset,uintptr_t type,const char* name) {
    uintptr_t actual=0,kind=0,text=0;char bytes[32]={};const auto size=strlen(name)+1;
    return size<=sizeof(bytes)&&word(imageBase+rva,actual)&&actual==offset&&word(imageBase+rva+8,kind)&&kind==type
        &&word(imageBase+rva+24,text)&&readMemory(text,bytes,size)&&!memcmp(bytes,name,size);
}
static bool trailerLayoutMatches() {
    for(const auto& l:trailerLayouts)if(activeHostHash&&!strcmp(activeHostHash,l.hash))
        return trailerField(l.slave,0x1b8,61,"slave_trailer")&&trailerField(l.accessories,0x10,62,"accessories")
            &&trailerField(l.path,0x10,1,"data_path")&&trailerField(l.definition,0x1a0,65,"trailer_definition");
    return false;
}
static bool accessoryPath(uintptr_t accessory,std::string& value) {
    uintptr_t data=0;uint32_t length=0;
    if(!liveUnit(accessory)||!word(accessory+0x18,data)||!readMemory(accessory+0x20,&length,4)||!data||!length||length>1024)return false;
    std::vector<char> text(length+1);if(!readMemory(data,text.data(),text.size())||text[length]!=0)return false;
    value.assign(text.data(),length);
    return value.find('\0')==std::string::npos&&value.rfind("/def/vehicle/",0)==0;
}
static bool recognisedDolly(const std::string& chassis,const std::string& body) {
    // Only exclude known SCS converter-dolly chassis. Unknown/modded components
    // count as trailers; labels such as chain_type=single are never trusted.
    if(!body.empty()||chassis.rfind("/def/vehicle/trailer_owned/scs.",0)!=0)return false;
    const auto at=chassis.find("/chassis/");if(at==std::string::npos)return false;
    const auto name=chassis.substr(at+9);
    return name=="dolly.sii"||name=="dolly_conv.sii"||name=="dolly_conv_2a.sii"||name=="dolly_conv_long.sii"||name=="dolly_conv_sp.sii"||name=="dolly_conv_sp_long.sii";
}
static live::TrailerCombination readTrailerCombination(uintptr_t first) {
    live::TrailerCombination result;
    if(!trailerLayoutMatches())return result;
    std::vector<uintptr_t> visited;
    for(auto current=first;current;) {
        if(visited.size()>=10||!liveUnit(current)||std::find(visited.begin(),visited.end(),current)!=visited.end())return {};
        visited.push_back(current);result.identity+=":"+std::to_string(current);
        uintptr_t data=0,count=0;
        if(!word(current+0x18,data)||!word(current+0x20,count)||!data||!count||count>256)return {};
        live::TrailerPart part;
        for(uintptr_t i=0;i<count;++i) {
            uintptr_t accessory=0;std::string path;
            if(!word(data+i*8,accessory)||!accessoryPath(accessory,path))return {};
            if(path.find("/chassis/")!=std::string::npos){if(!part.chassis.empty())return {};part.chassis=path;}
            if(path.find("/body/")!=std::string::npos){if(!part.body.empty())return {};part.body=path;}
        }
        if(part.chassis.empty())return {};
        part.dolly=recognisedDolly(part.chassis,part.body);
        // A converter must sit between two load-bearing trailers.
        if(part.dolly&&(result.parts.empty()||result.parts.back().dolly))return {};
        result.parts.push_back(part);
        if(!word(current+0x1b8,current))return {};
    }
    if(result.parts.empty()||result.parts.back().dolly)return {};
    result.known=true;return result;
}
