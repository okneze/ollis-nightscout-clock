#include "BGSourceManager.h"

#include "ServerManager.h"
#include "SettingsManager.h"

// Define the static getInstance method
BGSourceManager_& BGSourceManager_::getInstance() {
    static BGSourceManager_ instance;
    return instance;
}

// Define the constructor
BGSourceManager_::BGSourceManager_() {}

// Define the destructor
BGSourceManager_::~BGSourceManager_() {}

// Define the extern variable
BGSourceManager_& bgSourceManager = BGSourceManager_::getInstance();

BGSource* BGSourceManager_::createSource(BG_SOURCE bgSourceType, bool allowApi) {
    switch (bgSourceType) {
        case BG_SOURCE::NIGHTSCOUT:
            return new BGSourceNightscout();
        case BG_SOURCE::DEXCOM:
            return new BGSourceDexcom();
        case BG_SOURCE::API:
            if (allowApi) {
                return new BGSourceApi();
            }
            DEBUG_PRINTLN("Secondary BG source API is not supported.");
            return nullptr;
        case BG_SOURCE::LIBRELINKUP:
            return new BGSourceLibreLinkUp();
        case BG_SOURCE::MEDTRONIC:
            return new BGSourceMedtronic();
        case BG_SOURCE::MEDTRUM:
            return new BGSourceMedtrum();
        default:
            return nullptr;
    }
}

void BGSourceManager_::loadSecondarySettingsFromConfig() {
    secondaryNightscoutUrl = SettingsManager.settings.nightscout_url_secondary;
    secondaryNightscoutApiKey = SettingsManager.settings.nightscout_api_key_secondary;
    secondaryNightscoutSimplifiedApi = SettingsManager.settings.nightscout_simplified_api_secondary;

    secondaryDexcomUsername = SettingsManager.settings.dexcom_username_secondary;
    secondaryDexcomPassword = SettingsManager.settings.dexcom_password_secondary;
    secondaryDexcomServer = SettingsManager.settings.dexcom_server_secondary;

    secondaryLibreLinkupEmail = SettingsManager.settings.librelinkup_email_secondary;
    secondaryLibreLinkupPassword = SettingsManager.settings.librelinkup_password_secondary;
    secondaryLibreLinkupRegion = SettingsManager.settings.librelinkup_region_secondary;
    secondaryLibreLinkupPatientId = SettingsManager.settings.librelinkup_patient_id_secondary;

    secondaryMedtrumEmail = SettingsManager.settings.medtrum_email_secondary;
    secondaryMedtrumPassword = SettingsManager.settings.medtrum_password_secondary;
}

BGSourceManager_::SourceSettingsBackup BGSourceManager_::backupPrimarySourceSettings() const {
    SourceSettingsBackup backup;
    backup.nightscout_url = SettingsManager.settings.nightscout_url;
    backup.nightscout_api_key = SettingsManager.settings.nightscout_api_key;
    backup.nightscout_simplified_api = SettingsManager.settings.nightscout_simplified_api;
    backup.dexcom_username = SettingsManager.settings.dexcom_username;
    backup.dexcom_password = SettingsManager.settings.dexcom_password;
    backup.dexcom_server = SettingsManager.settings.dexcom_server;
    backup.librelinkup_email = SettingsManager.settings.librelinkup_email;
    backup.librelinkup_password = SettingsManager.settings.librelinkup_password;
    backup.librelinkup_region = SettingsManager.settings.librelinkup_region;
    backup.librelinkup_patient_id = SettingsManager.settings.librelinkup_patient_id;
    backup.medtrum_email = SettingsManager.settings.medtrum_email;
    backup.medtrum_password = SettingsManager.settings.medtrum_password;
    return backup;
}

void BGSourceManager_::applySecondarySettingsAsPrimary() {
    SettingsManager.settings.nightscout_url = secondaryNightscoutUrl;
    SettingsManager.settings.nightscout_api_key = secondaryNightscoutApiKey;
    SettingsManager.settings.nightscout_simplified_api = secondaryNightscoutSimplifiedApi;

    SettingsManager.settings.dexcom_username = secondaryDexcomUsername;
    SettingsManager.settings.dexcom_password = secondaryDexcomPassword;
    SettingsManager.settings.dexcom_server = secondaryDexcomServer;

    SettingsManager.settings.librelinkup_email = secondaryLibreLinkupEmail;
    SettingsManager.settings.librelinkup_password = secondaryLibreLinkupPassword;
    SettingsManager.settings.librelinkup_region = secondaryLibreLinkupRegion;
    SettingsManager.settings.librelinkup_patient_id = secondaryLibreLinkupPatientId;

    SettingsManager.settings.medtrum_email = secondaryMedtrumEmail;
    SettingsManager.settings.medtrum_password = secondaryMedtrumPassword;
}

void BGSourceManager_::restorePrimarySourceSettings(const SourceSettingsBackup& backup) {
    SettingsManager.settings.nightscout_url = backup.nightscout_url;
    SettingsManager.settings.nightscout_api_key = backup.nightscout_api_key;
    SettingsManager.settings.nightscout_simplified_api = backup.nightscout_simplified_api;
    SettingsManager.settings.dexcom_username = backup.dexcom_username;
    SettingsManager.settings.dexcom_password = backup.dexcom_password;
    SettingsManager.settings.dexcom_server = backup.dexcom_server;
    SettingsManager.settings.librelinkup_email = backup.librelinkup_email;
    SettingsManager.settings.librelinkup_password = backup.librelinkup_password;
    SettingsManager.settings.librelinkup_region = backup.librelinkup_region;
    SettingsManager.settings.librelinkup_patient_id = backup.librelinkup_patient_id;
    SettingsManager.settings.medtrum_email = backup.medtrum_email;
    SettingsManager.settings.medtrum_password = backup.medtrum_password;
}

void BGSourceManager_::setup(BG_SOURCE bgSourceType, BG_SOURCE secondarySourceType) {
    bgSource = createSource(bgSourceType, true);
    if (bgSource == nullptr) {
        DEBUG_PRINTLN(
            "BGSourceManager_::setup: Unknown BG_SOURCE: " + toString(bgSourceType) + " (" +
            String((int)bgSourceType) + ")");
        DisplayManager.showFatalError(
            "Unknown data source: " + toString(bgSourceType) + " (" + String((int)bgSourceType) + ")");
        return;
    }

    bgSource->setup();
    currentSourceType = bgSourceType;

    loadSecondarySettingsFromConfig();
    currentSecondarySourceType = secondarySourceType;
    if (secondarySourceType == BG_SOURCE::NO_SOURCE) {
        secondaryBgSource = nullptr;
        return;
    }

    secondaryBgSource = createSource(secondarySourceType, false);
    if (secondaryBgSource == nullptr) {
        currentSecondarySourceType = BG_SOURCE::NO_SOURCE;
        return;
    }

    auto backup = backupPrimarySourceSettings();
    applySecondarySettingsAsPrimary();
    secondaryBgSource->setup();
    restorePrimarySourceSettings(backup);
}

void BGSourceManager_::tick() {
    if (bgSource != nullptr) {
        bgSource->tick();
    }

    if (secondaryBgSource != nullptr && currentSecondarySourceType != BG_SOURCE::NO_SOURCE) {
        auto backup = backupPrimarySourceSettings();
        applySecondarySettingsAsPrimary();
        secondaryBgSource->tick();
        restorePrimarySourceSettings(backup);
    }
}

bool BGSourceManager_::hasNewData(unsigned long long epochToCompare) {
    if (bgSource == nullptr) {
        return false;
    }
    return bgSource->hasNewData(epochToCompare);
}

std::list<GlucoseReading> BGSourceManager_::getGlucoseData() {
    if (bgSource == nullptr) {
        return std::list<GlucoseReading>();
    }
    return bgSource->getGlucoseData();
}

std::list<GlucoseReading> BGSourceManager_::getSecondaryGlucoseData() {
    if (secondaryBgSource == nullptr) {
        return std::list<GlucoseReading>();
    }
    return secondaryBgSource->getGlucoseData();
}
