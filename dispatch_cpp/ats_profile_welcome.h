#pragma once
#include <string>

namespace ats {
// ATS writes one selected-profile line whenever a profile is selected. Use the
// most recent complete line; a partially written line is not evidence.
inline std::string selectedProfileFromLog(const std::string& log) {
    const std::string marker=" : New profile selected: '";
    std::string selected;
    size_t begin=0;
    while(begin<log.size()) {
        const auto end=log.find('\n',begin);if(end==std::string::npos)break;
        const auto line=log.substr(begin,end-begin);const auto at=line.find(marker);
        if(at!=std::string::npos) {
            const auto start=at+marker.size();auto last=line.size();
            if(last&&line[last-1]=='\r')--last;
            if(last>start&&line[last-1]=='\'') {
                const auto name=line.substr(start,last-start-1);
                if(!name.empty()&&name.size()<=256)selected=name;
            }
        }
        begin=end+1;
    }
    return selected;
}
}
