#include "job_history.h"
#include <iostream>
int wmain(int argc,wchar_t** argv){
    try{if(argc!=2&&(argc!=4||std::wstring(argv[2])!=L"--import-confirmed"))throw std::runtime_error("Usage: HistoryExport.exe project-folder [--import-confirmed saved-dispatch.json]");const auto project=live::fs::absolute(argv[1]);if(argc==4)ats::retainUserConfirmedDelivery(project,live::ipcFolder(),live::readJson(live::fs::absolute(argv[3])));ats::Catalog catalog(project);const auto result=ats::exportJobHistory(project,live::ipcFolder(),&catalog);std::cout<<result.rows<<" finished jobs in job_history.csv"<<(result.changed?" (updated)":" (unchanged)")<<"\n";return 0;}
    catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
