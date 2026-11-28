#pragma once
#include <windows.h>
#include "ats_catalog.h"
#include "multi_drop.h"
#include <array>
#include <memory>
#include <random>

class LivePanel {
public:
    static constexpr int DispatchId=500;
    static constexpr int RoadTrainWarningId=415;
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
    void setMultiDrop(bool value);
    void setRandom(bool value);
    void setFullCapacity(bool value){fullCapacity=value;}
    bool shouldWarnOnClose() const;
    bool connectedGameExited() const {return connectedDuringRun&&gameClosed;}
    std::filesystem::path activeProfileData() const {return profileData;}
private:
    bool fullCapacity=false;
    friend struct LivePanelSelectionTest;
    HWND parent;HINSTANCE instance;std::filesystem::path project,bridge,profileData;
    std::unique_ptr<ats::Catalog> catalog;
    std::array<HWND,7> labels{},combos{};
    std::array<std::vector<ats::Choice>,7> options;
    std::array<std::string,7> selected{};
    HWND trailerText=nullptr,roadTrainWarning=nullptr,help=nullptr,dispatchButton=nullptr,acknowledgeButton=nullptr,cancelButton=nullptr,cancelTooltip=nullptr,statusText=nullptr,jobText=nullptr;
    HWND percentLabel=nullptr,percentEdit=nullptr,percentSpin=nullptr;
    HWND stopCountCombo=nullptr,stopEditorCombo=nullptr,selectionCombo=nullptr,generateButton=nullptr;
    HWND distanceLabel=nullptr,distanceEdit=nullptr,roundTripCheck=nullptr;
    unsigned preferredKm=0,availableDropCount=0;
    bool roundTrip=false;
    std::string tripKey,dlcSession;
    ats::TripChoices tripChoices;
    void loadTripPreferences();
    void saveTripPreferences();
    void refreshTripChoices();
    unsigned readPreferredKm() const;
    bool multiDrop=false;
    unsigned dropCount=3,editingDrop=0;
    std::vector<std::array<std::string,2>> intermediateDrops{2};
    std::vector<live::Route> dropRoutes() const;
    void refreshDropEditor();
    void generateDrops();
    live::Json status=live::Json::object(),result=live::Json::object(),plan=live::Json::object(),observedGame=live::Json::object();
    std::string lastId,lastSession,body,signature,catalogError,sendError,offlineReason,formClearedId,historyError,historyReceipt,reviewedSession;
    std::string displayedProfile;
    std::string pendingCancelId,followCancelId,cancelMessage;
    std::wstring cancelTooltipText;
    uint64_t submitted=0,observedGameCreated=0;
    bool gameClosed=false,reviewPromptPosted=false,connectedDuringRun=false;
    bool visible=false,syncing=false,online=false,twoStop=false,savedPlan=false;
    bool randomMiddle=false;
    std::mt19937 randomEngine{std::random_device{}()};
    unsigned savedPercent=50;
    size_t historyRows=0;
    uint64_t lastHistorySync=0,lastResumeAttempt=0;
    void loadProfile(const std::string& name);
    void choices();
    bool defaultPickupCity();
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
