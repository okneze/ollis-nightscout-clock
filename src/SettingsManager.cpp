#include "SettingsManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "globals.h"

namespace {
BG_SOURCE parseBgSource(String data_source) {
    data_source.toLowerCase();
    if (data_source == "nightscout") {
        return BG_SOURCE::NIGHTSCOUT;
    } else if (data_source == "dexcom") {
        return BG_SOURCE::DEXCOM;
    } else if (data_source == "medtronic" || data_source == "carelink") {
        return BG_SOURCE::MEDTRONIC;
    } else if (data_source == "api") {
        return BG_SOURCE::API;
    } else if (data_source == "librelinkup") {
        return BG_SOURCE::LIBRELINKUP;
    } else if (data_source == "medtrum") {
        return BG_SOURCE::MEDTRUM;
    }
    return BG_SOURCE::NO_SOURCE;
}

String bgSourceToConfigString(BG_SOURCE source) {
    switch (source) {
        case BG_SOURCE::NIGHTSCOUT:
            return "nightscout";
        case BG_SOURCE::DEXCOM:
            return "dexcom";
        case BG_SOURCE::MEDTRONIC:
            return "medtronic";
        case BG_SOURCE::API:
            return "api";
        case BG_SOURCE::LIBRELINKUP:
            return "librelinkup";
        case BG_SOURCE::MEDTRUM:
            return "medtrum";
        default:
            return "no_source";
    }
}

DEXCOM_SERVER parseDexcomServer(String dexcomServerStr) {
    dexcomServerStr.toLowerCase();
    if (dexcomServerStr == "us") {
        return DEXCOM_SERVER::US;
    } else if (dexcomServerStr == "ous") {
        return DEXCOM_SERVER::NON_US;
    } else if (dexcomServerStr == "jp") {
        return DEXCOM_SERVER::JAPAN;
    }
    return DEXCOM_SERVER::NON_US;
}

String dexcomServerToConfigString(DEXCOM_SERVER server) {
    switch (server) {
        case DEXCOM_SERVER::US:
            return "us";
        case DEXCOM_SERVER::NON_US:
            return "ous";
        case DEXCOM_SERVER::JAPAN:
            return "jp";
        default:
            return "ous";
    }
}
}  // namespace

// The getter for the instantiated singleton instance
SettingsManager_& SettingsManager_::getInstance() {
    static SettingsManager_ instance;
    return instance;
}

// Initialize the global shared instance
SettingsManager_& SettingsManager = SettingsManager.getInstance();

void SettingsManager_::setup() { LittleFS.begin(); }

bool copyFile(const char* srcPath, const char* destPath) {
    File srcFile = LittleFS.open(srcPath, "r");
    if (!srcFile) {
        DEBUG_PRINTLN("Failed to open source file");
        return false;
    }

    File destFile = LittleFS.open(destPath, "w");
    if (!destFile) {
        DEBUG_PRINTLN("Failed to open destination file");
        srcFile.close();
        return false;
    }

    while (srcFile.available()) {
        char data = srcFile.read();
        destFile.write(data);
    }

    srcFile.close();
    destFile.close();

    DEBUG_PRINTLN("File copied successfully");
    return true;
}

void SettingsManager_::factoryReset() {
    copyFile(CONFIG_JSON_FACTORY, CONFIG_JSON);
    LittleFS.end();
    ESP.restart();
}

JsonDocument* SettingsManager_::readConfigJsonFile() {
    JsonDocument* doc;
    if (LittleFS.exists(CONFIG_JSON)) {
        auto settings = Settings();
        File file = LittleFS.open(CONFIG_JSON);
        if (!file || file.isDirectory()) {
            DEBUG_PRINTLN("Failed to open config file for reading");
            return NULL;
        }

        doc = new JsonDocument();
        DeserializationError error = deserializeJson(*doc, file);
        if (error) {
            DEBUG_PRINTF(
                "Deserialization error. File size: %d, requested memory: %d. Error: %s\n", file.size(),
                (int)(file.size() * 2), error.c_str());
            file.close();
            return NULL;
        }
        return doc;
    } else {
        DEBUG_PRINTLN("Cannot read configuration file");
        factoryReset();
        return NULL;
    }
}

bool SettingsManager_::loadSettingsFromFile() {
    auto doc = readConfigJsonFile();
    if (doc == NULL)
        return false;

    settings.ssid = (*doc)["ssid"].as<String>();
    settings.wifi_password = (*doc)["password"].as<String>();

    settings.bg_low_warn_limit = (*doc)["low_mgdl"].as<int>();
    settings.bg_high_warn_limit = (*doc)["high_mgdl"].as<int>();
    settings.bg_low_urgent_limit = (*doc)["low_urgent_mgdl"].as<int>();
    settings.bg_high_urgent_limit = (*doc)["high_urgent_mgdl"].as<int>();
    settings.bg_units = (*doc)["units"].as<String>() == "mmol" ? BG_UNIT::MMOLL : BG_UNIT::MGDL;

    String brightness_mode = (*doc)["brightness_mode"].as<String>();
    if (brightness_mode == "manual") {
        settings.brightness_mode = BRIGHTNES_MODE::MANUAL;
    } else if (brightness_mode == "auto_linear") {
        settings.brightness_mode = BRIGHTNES_MODE::AUTO_LINEAR;
    } else if (brightness_mode == "auto_dimmed") {
        settings.brightness_mode = BRIGHTNES_MODE::AUTO_DIMMED;
    } else {
        DEBUG_PRINTLN(
            "Unknown brightness mode in config: " + brightness_mode + ", defaulting to AUTO_LINEAR");
        settings.brightness_mode = BRIGHTNES_MODE::AUTO_LINEAR;
    }

    settings.brightness_level = (*doc)["brightness_level"].as<int>() - 1;
    settings.default_clockface = (*doc)["default_face"].as<int>();

    String data_source = (*doc)["data_source"].as<String>();
    settings.bg_source = parseBgSource(data_source);
    String data_source_secondary = (*doc)["data_source_secondary"] | "no_source";
    settings.bg_source_secondary = parseBgSource(data_source_secondary);

    settings.medtrum_email = (*doc)["medtrum_email"].as<String>();
    settings.medtrum_password = (*doc)["medtrum_password"].as<String>();
    settings.medtrum_email_secondary = (*doc)["medtrum_email_secondary"].as<String>();
    settings.medtrum_password_secondary = (*doc)["medtrum_password_secondary"].as<String>();

    settings.dexcom_username = (*doc)["dexcom_username"].as<String>();
    settings.dexcom_password = (*doc)["dexcom_password"].as<String>();
    settings.dexcom_server = parseDexcomServer((*doc)["dexcom_server"].as<String>());
    settings.dexcom_username_secondary = (*doc)["dexcom_username_secondary"].as<String>();
    settings.dexcom_password_secondary = (*doc)["dexcom_password_secondary"].as<String>();
    settings.dexcom_server_secondary = parseDexcomServer((*doc)["dexcom_server_secondary"] | "ous");

    settings.librelinkup_email = (*doc)["librelinkup_email"].as<String>();
    settings.librelinkup_password = (*doc)["librelinkup_password"].as<String>();
    settings.librelinkup_region = (*doc)["librelinkup_region"].as<String>();
    settings.librelinkup_patient_id = (*doc)["librelinkup_patient_id"].as<String>();
    settings.librelinkup_email_secondary = (*doc)["librelinkup_email_secondary"].as<String>();
    settings.librelinkup_password_secondary = (*doc)["librelinkup_password_secondary"].as<String>();
    settings.librelinkup_region_secondary = (*doc)["librelinkup_region_secondary"] | "EU";
    settings.librelinkup_patient_id_secondary = (*doc)["librelinkup_patient_id_secondary"].as<String>();

    settings.nightscout_url = (*doc)["nightscout_url"].as<String>();
    settings.onedigit_external_api_url = (*doc)["onedigit_external_api_url"] | "";
    settings.nightscout_api_key = (*doc)["api_secret"].as<String>();
    settings.nightscout_simplified_api = (*doc)["nightscout_simplified_api"].as<bool>();
    settings.nightscout_url_secondary = (*doc)["nightscout_url_secondary"].as<String>();
    settings.nightscout_api_key_secondary = (*doc)["api_secret_secondary"].as<String>();
    settings.nightscout_simplified_api_secondary = (*doc)["nightscout_simplified_api_secondary"] | false;

    settings.tz_libc_value = (*doc)["tz_libc"].as<String>();
    settings.time_format =
        (*doc)["time_format"].as<String>() == "12" ? TIME_FORMAT::HOURS_12 : TIME_FORMAT::HOURS_24;

    // read alarms data
    settings.alarm_urgent_low_enabled = (*doc)["alarm_urgent_low_enabled"].as<bool>();
    settings.alarm_urgent_low_mgdl = (*doc)["alarm_urgent_low_value"].as<int>();
    settings.alarm_urgent_low_snooze_minutes = (*doc)["alarm_urgent_low_snooze_interval"].as<int>();
    settings.alarm_urgent_low_silence_interval =
        (*doc)["alarm_urgent_low_silence_interval"].as<String>();
    settings.alarm_low_enabled = (*doc)["alarm_low_enabled"].as<bool>();
    settings.alarm_low_mgdl = (*doc)["alarm_low_value"].as<int>();
    settings.alarm_low_snooze_minutes = (*doc)["alarm_low_snooze_interval"].as<int>();
    settings.alarm_low_silence_interval = (*doc)["alarm_low_silence_interval"].as<String>();
    settings.alarm_high_enabled = (*doc)["alarm_high_enabled"].as<bool>();
    settings.alarm_high_mgdl = (*doc)["alarm_high_value"].as<int>();
    settings.alarm_high_snooze_minutes = (*doc)["alarm_high_snooze_interval"].as<int>();
    settings.alarm_high_silence_interval = (*doc)["alarm_high_silence_interval"].as<String>();
    settings.alarm_high_melody = (*doc)["alarm_high_melody"].as<String>();
    settings.alarm_low_melody = (*doc)["alarm_low_melody"].as<String>();
    settings.alarm_urgent_low_melody = (*doc)["alarm_urgent_low_melody"].as<String>();
    settings.alarm_intensive_mode = (*doc)["alarm_intensive_mode"].as<bool>();

    // Additional WiFi
    settings.additional_wifi_enable = (*doc)["additional_wifi_enable"].as<bool>();
    settings.additional_wifi_type = (*doc)["additional_wifi_type"].as<String>();
    settings.additional_wifi_ssid = (*doc)["additional_ssid"].as<String>();
    settings.additional_wifi_username = (*doc)["additional_wifi_username"].as<String>();
    settings.additional_wifi_password = (*doc)["additional_wifi_password"].as<String>();

    // Custom hostname
    settings.custom_hostname_enable = (*doc)["custom_hostname_enable"].as<bool>();
    settings.custom_hostname = (*doc)["custom_hostname"].as<String>();

    // Custom No Data Timer
    settings.custom_nodatatimer_enable = (*doc)["custom_nodatatimer_enable"].as<bool>();
    settings.custom_nodatatimer = (*doc)["custom_nodatatimer"].as<int>();
    if (settings.custom_nodatatimer_enable == true && settings.custom_nodatatimer > 5 &&
        settings.custom_nodatatimer <= 60) {
        settings.bg_data_too_old_threshold_minutes = settings.custom_nodatatimer;
    } else {
        settings.bg_data_too_old_threshold_minutes = 20;  // default value
        if (settings.custom_nodatatimer_enable == true) {
            DEBUG_PRINTLN("Custom No Data Timer value is invalid, using default value of 20 minutes.");
        }
    }

    // Web interface authentication
    settings.web_auth_enable = (*doc)["web_auth_enable"].as<bool>();
    settings.web_auth_password = (*doc)["web_auth_password"].as<String>();

    delete doc;

    this->settings = settings;
    return true;
}

bool SettingsManager_::saveSettingsToFile() {
    auto doc = readConfigJsonFile();
    if (doc == NULL)
        return false;

    (*doc)["ssid"] = settings.ssid;
    (*doc)["password"] = settings.wifi_password;

    (*doc)["low_mgdl"] = settings.bg_low_warn_limit;
    (*doc)["high_mgdl"] = settings.bg_high_warn_limit;
    (*doc)["low_urgent_mgdl"] = settings.bg_low_urgent_limit;
    (*doc)["high_urgent_mgdl"] = settings.bg_high_urgent_limit;

    (*doc)["units"] = settings.bg_units == BG_UNIT::MMOLL ? "mmol" : "mgdl";

    (*doc)["brightness_mode"] = settings.brightness_mode == BRIGHTNES_MODE::AUTO_LINEAR   ? "auto_linear"
                                : settings.brightness_mode == BRIGHTNES_MODE::AUTO_DIMMED ? "auto_dimmed"
                                                                                          : "manual";
    (*doc)["brightness_level"] = settings.brightness_level + 1;
    (*doc)["default_face"] = settings.default_clockface;

    (*doc)["data_source"] = bgSourceToConfigString(settings.bg_source);
    (*doc)["data_source_secondary"] = bgSourceToConfigString(settings.bg_source_secondary);

    (*doc)["medtrum_email"] = settings.medtrum_email;
    (*doc)["medtrum_password"] = settings.medtrum_password;
    (*doc)["medtrum_email_secondary"] = settings.medtrum_email_secondary;
    (*doc)["medtrum_password_secondary"] = settings.medtrum_password_secondary;

    (*doc)["dexcom_username"] = settings.dexcom_username;
    (*doc)["dexcom_password"] = settings.dexcom_password;
    (*doc)["dexcom_server"] = dexcomServerToConfigString(settings.dexcom_server);
    (*doc)["dexcom_username_secondary"] = settings.dexcom_username_secondary;
    (*doc)["dexcom_password_secondary"] = settings.dexcom_password_secondary;
    (*doc)["dexcom_server_secondary"] = dexcomServerToConfigString(settings.dexcom_server_secondary);

    (*doc)["librelinkup_email"] = settings.librelinkup_email;
    (*doc)["librelinkup_password"] = settings.librelinkup_password;
    (*doc)["librelinkup_region"] = settings.librelinkup_region;
    (*doc)["librelinkup_patient_id"] = settings.librelinkup_patient_id;
    (*doc)["librelinkup_email_secondary"] = settings.librelinkup_email_secondary;
    (*doc)["librelinkup_password_secondary"] = settings.librelinkup_password_secondary;
    (*doc)["librelinkup_region_secondary"] = settings.librelinkup_region_secondary;
    (*doc)["librelinkup_patient_id_secondary"] = settings.librelinkup_patient_id_secondary;

    (*doc)["nightscout_url"] = settings.nightscout_url;
    (*doc)["onedigit_external_api_url"] = settings.onedigit_external_api_url;
    (*doc)["api_secret"] = settings.nightscout_api_key;
    (*doc)["nightscout_simplified_api"] = settings.nightscout_simplified_api;
    (*doc)["nightscout_url_secondary"] = settings.nightscout_url_secondary;
    (*doc)["api_secret_secondary"] = settings.nightscout_api_key_secondary;
    (*doc)["nightscout_simplified_api_secondary"] = settings.nightscout_simplified_api_secondary;

    (*doc)["tz_libc"] = settings.tz_libc_value;
    (*doc)["time_format"] = settings.time_format == TIME_FORMAT::HOURS_12 ? "12" : "24";

    // save alarms data
    (*doc)["alarm_urgent_low_enabled"] = settings.alarm_urgent_low_enabled;
    (*doc)["alarm_urgent_low_value"] = settings.alarm_urgent_low_mgdl;
    (*doc)["alarm_urgent_low_snooze_interval"] = settings.alarm_urgent_low_snooze_minutes;
    (*doc)["alarm_urgent_low_silence_interval"] = settings.alarm_urgent_low_silence_interval;
    (*doc)["alarm_low_enabled"] = settings.alarm_low_enabled;
    (*doc)["alarm_low_value"] = settings.alarm_low_mgdl;
    (*doc)["alarm_low_snooze_interval"] = settings.alarm_low_snooze_minutes;
    (*doc)["alarm_low_silence_interval"] = settings.alarm_low_silence_interval;
    (*doc)["alarm_high_enabled"] = settings.alarm_high_enabled;
    (*doc)["alarm_high_value"] = settings.alarm_high_mgdl;
    (*doc)["alarm_high_snooze_interval"] = settings.alarm_high_snooze_minutes;
    (*doc)["alarm_high_silence_interval"] = settings.alarm_high_silence_interval;
    (*doc)["alarm_high_melody"] = settings.alarm_high_melody;
    (*doc)["alarm_low_melody"] = settings.alarm_low_melody;
    (*doc)["alarm_urgent_low_melody"] = settings.alarm_urgent_low_melody;
    (*doc)["alarm_intensive_mode"] = settings.alarm_intensive_mode;

    // Additional WiFi
    (*doc)["additional_wifi_enable"] = settings.additional_wifi_enable;
    (*doc)["additional_wifi_type"] = settings.additional_wifi_type;
    (*doc)["additional_ssid"] = settings.additional_wifi_ssid;
    (*doc)["additional_wifi_username"] = settings.additional_wifi_username;
    (*doc)["additional_wifi_password"] = settings.additional_wifi_password;

    // Custom hostname
    (*doc)["custom_hostname_enable"] = settings.custom_hostname_enable;
    (*doc)["custom_hostname"] = settings.custom_hostname;

    // Custom No Data Timer
    (*doc)["custom_nodatatimer_enable"] = settings.custom_nodatatimer_enable;
    (*doc)["custom_nodatatimer"] = settings.custom_nodatatimer;

    // Web interface authentication
    (*doc)["web_auth_enable"] = settings.web_auth_enable;
    (*doc)["web_auth_password"] = settings.web_auth_password;

    if (trySaveJsonAsSettings(*doc) == false)
        return false;

    delete doc;

    return true;
}

bool SettingsManager_::trySaveJsonAsSettings(JsonDocument doc) {
    auto file = LittleFS.open(CONFIG_JSON, FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("Failed to open config file for writing");
        return false;
    }

    size_t bytesWritten = serializeJson(doc, file);

    file.close();
    if (bytesWritten == 0) {
        DEBUG_PRINTLN("Failed to serialize config JSON to file");
        return false;
    }

    return true;
}
