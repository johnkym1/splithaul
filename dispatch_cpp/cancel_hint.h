#pragma once
#include "../dispatch_shared/protocol.h"

namespace ats {
// Availability of the action is separate from readiness for a native cancel.
// A driver can open the action as soon as dispatch is submitted and receive
// guidance instead of waiting for an unexplained disabled button.
inline bool cancelActionAvailable(bool online,bool hasProfile,bool pending,const live::Json& status,bool hasPendingDispatch) {
    try{return online&&hasProfile&&!pending&&status.value("experimental_cancel_available",false)&&(hasPendingDispatch||status.value("job",live::Json::object()).value("active",false));}catch(...){return false;}
}
inline std::string cancelUnavailableReason(bool online,bool hasProfile,bool pending,const live::Json& status,bool hasSavedJob) {
    try {
        if(!online)return "Connect to ATS to cancel the game job.";
        if(!hasProfile)return "Wait for SplitHaul to identify the active ATS profile.";
        if(pending)return "A cancellation request is already being processed.";
        if(!status.value("experimental_cancel_available",false))return "This ATS cancellation test is not available with the installed telemetry DLL.";
        const bool hasMenuDetection=status.contains("cancel_menu_snapshot");
        if(!hasMenuDetection&&status.value("driving",true))return "Stop the truck and open the ATS menu to cancel the job.";
        const auto menu=status.value(hasMenuDetection?"cancel_menu_snapshot":"menu_snapshot",live::Json::object());
        if(!menu.value("captured",false))return "Return to Drive, stop the truck, then open the ATS menu.";
        if(!menu.contains("speed_mps")||!menu["speed_mps"].is_number())return "Truck speed is unavailable. Return to Drive, stop, then open the menu.";
        if(!menu.value("stopped",false))return "Stop the truck before opening the ATS menu.";
        if(!menu.contains("ignition_on")||!menu["ignition_on"].is_boolean())return "Ignition status is unavailable. Return to Drive, then open the menu.";
        if(status.value("context_id",std::string()).empty()||status.value("cancel_target_token",std::string()).empty())return "Wait for ATS to report the current truck and job.";
        const auto job=status.value("job",live::Json::object());
        if(!job.value("active",false)&&!hasSavedJob)return "There is no active or pending dispatch to cancel.";
        return {};
    }catch(...){return "ATS information is unavailable. Reconnect and try again.";}
}
}
