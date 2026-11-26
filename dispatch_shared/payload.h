#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace live {
struct TrailerCapacity {
    float grossKg=0,chassisKg=0,bodyKg=0,volumeM3=0;
    uint32_t axles=0;
    double payloadKg() const {return double(grossKg)-chassisKg-bodyKg;}
    bool valid() const {
        return std::isfinite(grossKg)&&std::isfinite(chassisKg)&&std::isfinite(bodyKg)&&std::isfinite(volumeM3)
            &&grossKg>0&&grossKg<=1000000&&chassisKg>=0&&bodyKg>=0&&payloadKg()>0&&volumeM3>0&&volumeM3<=2000&&axles>0&&axles<=20;
    }
    uint32_t maximumUnits(double unitKg,double unitM3) const {
        if(!valid()||!std::isfinite(unitKg)||!std::isfinite(unitM3)||unitKg<=0||unitM3<=0)return 0;
        const auto units=std::floor((std::min)(payloadKg()/unitKg,double(volumeM3)/unitM3));
        return units>=1&&units<=1000000?static_cast<uint32_t>(units):0;
    }
};
inline uint32_t remainingUnits(uint32_t initial,unsigned percent) {
    if(!initial||initial>1000000||percent<1||percent>99)throw std::runtime_error("Final remaining load must be between 1 and 99 percent.");
    const auto n=static_cast<uint32_t>(uint64_t(initial)*percent/100);
    if(!n||n>=initial)throw std::runtime_error("This cargo cannot be split into the selected remaining percentage using whole units.");
    return n;
}
inline bool matchingPayload(uint32_t count,double kg,uint32_t expected,double unitKg) {
    const auto target=double(expected)*unitKg;
    return expected>0&&count==expected&&std::isfinite(kg)&&std::isfinite(unitKg)&&unitKg>0&&std::fabs(kg-target)<=(std::max)(0.5,target*0.0001);
}
inline uint32_t multiDropRemaining(uint32_t initial,unsigned stops,unsigned delivered) {
    if(stops<3||stops>8||initial<stops||initial>1000000||delivered>stops)throw std::runtime_error("There must be at least one whole cargo unit for every delivery.");
    // Give the first remainder stops one extra unit. Nothing is lost to rounding.
    return initial-(initial/stops)*delivered-(std::min)(initial%stops,delivered);
}
}
