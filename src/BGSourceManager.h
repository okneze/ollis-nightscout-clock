#ifndef BGSOURCEMANAGER_H
#define BGSOURCEMANAGER_H

#include <Arduino.h>

#include <list>

#include "BGSource.h"
#include "BGSourceApi.h"
#include "BGSourceDexcom.h"
#include "BGSourceLibreLinkUp.h"
#include "BGSourceMedtronic.h"
#include "BGSourceMedtrum.h"
#include "BGSourceNightscout.h"
#include "DisplayManager.h"
#include "enums.h"

class BGSourceManager_ {
public:
    static BGSourceManager_& getInstance();

    void setup(BG_SOURCE bgSource, BG_SOURCE secondaryBgSource = BG_SOURCE::NO_SOURCE);
    void tick();
    bool hasNewData(unsigned long long epochToCompare);
    std::list<GlucoseReading> getGlucoseData();
    std::list<GlucoseReading> getSecondaryGlucoseData();
    BG_SOURCE getCurrentSourceType() const { return currentSourceType; }
    BG_SOURCE getSecondarySourceType() const { return currentSecondarySourceType; }
    String getSourceStatus() const { return (bgSource) ? bgSource->getStatus() : "unknown"; }
    String getSecondarySourceStatus() const {
        return (secondaryBgSource) ? secondaryBgSource->getStatus() : "disabled";
    }

private:
    struct SourceSettingsBackup {
        String nightscout_url;
        String nightscout_api_key;
        bool nightscout_simplified_api;
        String dexcom_username;
        String dexcom_password;
        DEXCOM_SERVER dexcom_server;
        String librelinkup_email;
        String librelinkup_password;
        String librelinkup_region;
        String librelinkup_patient_id;
        String medtrum_email;
        String medtrum_password;
    };

    BGSourceManager_();
    ~BGSourceManager_();
    BGSource* createSource(BG_SOURCE type, bool allowApi = true);
    void loadSecondarySettingsFromConfig();
    SourceSettingsBackup backupPrimarySourceSettings() const;
    void applySecondarySettingsAsPrimary();
    void restorePrimarySourceSettings(const SourceSettingsBackup& backup);

    BGSourceManager_(const BGSourceManager_&) = delete;
    BGSourceManager_& operator=(const BGSourceManager_&) = delete;
    BGSource* bgSource = nullptr;
    BGSource* secondaryBgSource = nullptr;
    BG_SOURCE currentSourceType;
    BG_SOURCE currentSecondarySourceType = BG_SOURCE::NO_SOURCE;
    unsigned long long lastPollEpoch = 0;

    String secondaryNightscoutUrl;
    String secondaryNightscoutApiKey;
    bool secondaryNightscoutSimplifiedApi = false;
    String secondaryDexcomUsername;
    String secondaryDexcomPassword;
    DEXCOM_SERVER secondaryDexcomServer = DEXCOM_SERVER::NON_US;
    String secondaryLibreLinkupEmail;
    String secondaryLibreLinkupPassword;
    String secondaryLibreLinkupRegion;
    String secondaryLibreLinkupPatientId;
    String secondaryMedtrumEmail;
    String secondaryMedtrumPassword;
};

extern BGSourceManager_& bgSourceManager;  // Declare extern variable

#endif  // BGSOURCEMANAGER_H