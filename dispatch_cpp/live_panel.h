#pragma once
#include <windows.h>
#include "ats_catalog.h"
#include <array>
#include <memory>

class LivePanel {
public:
    static constexpr int DispatchId=500;
    static constexpr UINT RestartReviewMessage=WM_APP+74;
    static constexpr UINT ProfileTitleMessage=WM_APP+89;
    LivePanel(HWND parent,HINSTANCE instance,const std::filesystem::path& project);
    void show(bool visible);
    void selectGame(bool ets);
    void layout(int width,int height,UINT dpi);
    bool command(int id,int notification);
    void tick();
    void reviewRestart();
    void setTwoStop(bool value);
    bool shouldWarnOnClose() const;
    bool connectedGameExited() const {return connectedDuringRun&&gameClosed;}
    std::filesystem::path activeProfileData() const {return profileData;}
private:
    HWND parent;HINSTANCE instance;std::filesystem::path project,bridge,profileData;
    std::unique_ptr<ats::Catalog> catalog;
    std::array<HWND,7> labels{},combos{};
    std::array<std::vector<ats::Choice>,7> options;
    std::array<std::string,7> selected{};
    HWND trailerText=nullptr,help=nullptr,dispatchButton=nullptr,acknowledgeButton=nullptr,cancelButton=nullptr,cancelTooltip=nullptr,statusText=nullptr,jobText=nullptr;
    HWND percentLabel=nullptr,percentEdit=nullptr;
    live::Json status=live::Json::object(),result=live::Json::object(),plan=live::Json::object(),observedGame=live::Json::object();
    std::string lastId,lastSession,body,signature,catalogError,sendError,offlineReason,formClearedId,historyError,historyReceipt,reviewedSession;
    std::string displayedProfile;
    std::string pendingCancelId,followCancelId,cancelMessage;
    std::wstring cancelTooltipText;
    uint64_t submitted=0,observedGameCreated=0;
    bool gameClosed=false,reviewPromptPosted=false,connectedDuringRun=false;
    bool visible=false,syncing=false,online=false,twoStop=false,savedPlan=false;
    unsigned savedPercent=50;
    size_t historyRows=0;
    uint64_t lastHistorySync=0,lastResumeAttempt=0;
    void loadProfile(const std::string& name);
    void choices();
    void render();
    live::Route route() const;
    live::Route finalRoute() const;
    void send();
    void requestCancel();
    void pollCancelResult();
    unsigned remainingPercent() const;
    bool clearCompletedForm();
    void syncHistory();
};
