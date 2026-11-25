#pragma once
#include "job_history.h"
#include "app_theme.h"
#include "profile_manager.h"
#include <memory>
class HistoryPage {
public:
    static constexpr UINT OpenMessage=WM_APP+75;
    HistoryPage(HWND parent,HINSTANCE instance,const std::filesystem::path& project,AppTheme& theme);
    ~HistoryPage();
    void show(bool value);
    void layout(int width,int height,UINT dpi);
    void applyTheme();
    void tick();
    void setProfile(const std::filesystem::path& path);
    bool command(int id,int notification);
    bool notify(const NMHDR* message,LRESULT& result);
private:
    friend struct HistoryPageTest;
    HWND parent,list,summary,help,details,exportButton,selectedExportButton,refreshButton,status;
    HWND gameLabel,gameCombo,profileLabel,profileCombo,followButton;
    HWND detailHeading,routeHeading,resultHeading,resultDetails;
    HFONT detailFont=nullptr,detailHeadingFont=nullptr,detailSectionFont=nullptr;
    UINT detailDpi=0;
    std::filesystem::path project,profileData,liveProfile;
    std::string profileName;
    std::vector<profileManager::Entry> profiles;
    int selectedGame=-1;
    bool manualBrowsing=false;
    AppTheme& theme;
    std::unique_ptr<ats::Catalog> catalog;
    std::vector<ats::HistoryEntry> entries;
    bool visible=false,populating=false,exporting=false;
    std::wstring exportNotice;
    ULONGLONG lastRefresh=0;
    void refresh();
    void refreshProfiles();
    void selectProfile(const std::filesystem::path& path,const std::string& name);
    void describeSelection();
    void detailFonts(UINT dpi);
    void detailsMessage(const std::wstring& message);
    void fitColumns();
    void exportCsv(bool selectedOnly=false);
};
