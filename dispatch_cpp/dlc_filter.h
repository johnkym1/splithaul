#pragma once
#include <regex>
#include <set>
#include <string>
namespace ats {
// Use mounted game content, not the presence of unowned archives in Steam.
inline std::set<std::string> mountedDlcs(const std::string& log) {
    std::set<std::string> result;
    const std::regex mounted(R"(\[fs\] device [^\r\n]*[/\\](dlc_[a-z0-9_]+)\.scs mounted to scs pool\.)");
    for(auto it=std::sregex_iterator(log.begin(),log.end(),mounted);it!=std::sregex_iterator();++it)result.insert((*it)[1].str());
    return result;
}
}
