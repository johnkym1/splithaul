#pragma once
#include "protocol.h"
#include <vector>

namespace live {
inline unsigned trailerLimit(){return 4u;}
struct TrailerPart {
    std::string chassis,body;
    bool dolly=false;
};
struct TrailerCombination {
    bool known=false;
    std::vector<TrailerPart> parts;
    std::string identity; // Native addresses, used only for this session's context.
    unsigned trailers() const {unsigned n=0;for(const auto& p:parts)if(!p.dolly)++n;return n;}
    unsigned dollies() const {return static_cast<unsigned>(parts.size())-trailers();}
    bool supported() const {return known&&trailers()>0&&trailers()<=trailerLimit();}
    std::string signature() const {
        Json values=Json::array();for(const auto& p:parts)values.push_back({p.chassis,p.body,p.dolly});
        if(!known)return {};
        const auto data=values.dump();BCRYPT_ALG_HANDLE alg=nullptr;unsigned char digest[32]={};
        if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
        const auto status=BCryptHash(alg,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),static_cast<ULONG>(data.size()),digest,sizeof(digest));
        BCryptCloseAlgorithmProvider(alg,0);if(status<0)return {};
        constexpr char hex[]="0123456789abcdef";std::string result;for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
    }
};
inline bool supportedTrailer(const Json& trailer) {
    // Old plugins cannot prove physical counts: a mod may label a triple single.
    if(!trailer.is_object()||!trailer.value("combination_known",false))return false;
    const auto n=trailer.value("trailer_count",0u);
    return n>0&&n<=trailerLimit();
}
inline std::string trailerSupportMessage(const Json& trailer) {
    if(!trailer.value("combination_known",false))return "Trailer combination is not verified. Attach owned trailers and use the matching updated game plugin.";
    return "This combination has "+std::to_string(trailer.value("trailer_count",0u))+" trailers. SplitHaul supports 1-4 trailers, excluding recognised dollies.";
}
inline bool overBDoubleWarning(const Json& trailer) {
    if(!dispatchGame::isETS2()||!trailer.is_object()||!trailer.value("connected",false)||!trailer.value("combination_known",false))return false;
    const auto count=trailer.value("trailer_count",0u);
    // HCT/A-double has two cargo trailers plus a converter dolly. Physical
    // counts also catch road trains whose mod labels them as a single.
    return count>2||(count==2&&(trailer.value("dolly_count",0u)>0||trailer.value("chain_type",std::string())=="hct"));
}
}
