#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if(vessel.call_sign == ""){
        return "empty call sign";
    }
    if(std::find(vessel.call_sign.begin(), vessel.call_sign.end(), ' ') != vessel.call_sign.end() or std::find(vessel.call_sign.begin(), vessel.call_sign.end(), '\n') != vessel.call_sign.end() or std::find(vessel.call_sign.begin(), vessel.call_sign.end(), '\t') != vessel.call_sign.end()){
        return "bad call sign";
    }
    if(vessel.length_m<=0 or vessel.length_m>kChamberLengthM){
        return "bad length";
    }
    if(vessel.draft_dm<=0){
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>&inside, const Vessel&next) {
    int total_length = 0;
    for(int i=0;i<inside.size();++i){
        total_length += inside[i].length_m;
    }
    total_length += inside.size() * kGapM;
    total_length += next.length_m;
    if(total_length <= kChamberLengthM){
        return true;
    }
    else{
        return false;
    }
}

}  // namespace kitezh
