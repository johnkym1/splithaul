#pragma once
#include "ats_catalog.h"
#include <optional>

namespace ats {
struct HistoryExport {size_t rows=0;bool changed=false;};
struct HistoryEntry {
    std::string id,session,cargo,pickupCity,pickupCompany,deliveryCity,deliveryCompany,outcome,source,plan;
    uint64_t completedMs=0;unsigned stop=0,stopCount=0;bool driverConfirmed=false;
    std::optional<double> massKg,revenue,distanceKm,damage;
    std::optional<uint32_t> units;
};
bool completedForm(bool twoStop,const std::string& id,const std::string& session,const live::Json& plan,const live::Json& result);
bool awaitingAcknowledgement(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result);
bool verifiedDeliveryReady(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result);
live::Json acknowledgeCompletedForm(const live::Json& saved,const live::Json& plan,const live::Json& result);
bool canConfirmUnrecordedCompletion(bool twoStop,const std::string& id,const std::string& session,const std::string& cleared,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now);
live::Json acknowledgeUnrecordedCompletion(const live::Json& saved,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now);
bool restartReviewDue(const live::Json& saved,const live::Json& plan,const live::Json& result,const live::Json& status,uint64_t now);
void retainUserConfirmedDelivery(const std::filesystem::path& project,const std::filesystem::path& bridge,const live::Json& saved);
std::string csvText(const std::string& value);
HistoryExport exportJobHistory(const std::filesystem::path& project,const std::filesystem::path& bridge,const Catalog* catalog,bool profileScoped=false);
std::vector<HistoryEntry> readHistoryEntries(const std::filesystem::path& project,const Catalog* catalog);
void saveHistoryCsvCopy(const std::filesystem::path& project,const std::filesystem::path& destination,bool profileScoped=false);
void saveHistoryCsvSelection(const std::filesystem::path& profile,const std::filesystem::path& destination,const Catalog* catalog,const std::string& id={},const std::string& session={});
}
