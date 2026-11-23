#pragma once
#include "job_history.h"
#include "app_theme.h"
#include <memory>
class HistoryPage {
public:
    static constexpr UINT OpenMessage=WM_APP+75;
    HistoryPage(HWND parent,HINSTANCE instance,const std::filesystem::path& project,AppTheme& theme);
    void show(bool value);
    void layout(int width,int height,UINT dpi);
    void applyTheme();
    void tick();
    void setProfile(const std::filesystem::path& path);
    bool command(int id,int notification);
    bool notify(const NMHDR* message,LRESULT& result);
private:
    HWND parent,list,summary,help,details,exportButton,refreshButton,status;
    std::filesystem::path project,profileData;
    AppTheme& theme;
    std::unique_ptr<ats::Catalog> catalog;
    std::vector<ats::HistoryEntry> entries;
    bool visible=false,populating=false,exporting=false;
    std::wstring exportNotice;
    ULONGLONG lastRefresh=0;
    void refresh();
    void describeSelection();
    void fitColumns();
    void exportCsv();
};
