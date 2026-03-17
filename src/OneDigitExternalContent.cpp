#include "OneDigitExternalContent.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include <vector>

#include "DisplayManager.h"
#include "ServerManager.h"
#include "SettingsManager.h"
#include "globals.h"

namespace {
struct CachedContent {
    bool available = false;
    bool selectedIsBitmap = false;
    bool scrollEnabled = false;
    String selectedText;
    String selectedFrameHex;
    uint16_t selectedColor = COLOR_WHITE;
    int selectedBitmapWidth = 0;
    int selectedBitmapHeight = 0;
    unsigned long validUntilMs = 0;
    TEXT_ALIGNMENT selectedAlign = TEXT_ALIGNMENT::LEFT;
    int scrollOffset = 0;
    unsigned long lastScrollStepMs = 0;
};

CachedContent oneDigitCache;
CachedContent oneDigitDualCache;
WiFiClientSecure oneDigitSecureClient;
WiFiClient oneDigitPlainClient;
unsigned long nextRetryAfterMs = 0;
int consecutiveFailures = 0;
unsigned long lastAttemptMs = 0;
unsigned long lastSuccessMs = 0;
int lastHttpCode = 0;
String lastError = "not_started";
String lastEndpoint = "";
String lastDeviceId = "";
String lastView = "";
String lastConnectHost = "";
String lastConnectIp = "";
String lastRequestHostHeader = "";
constexpr bool kDisableExternalApiFetchForStability = false;

unsigned long computeRetryDelayMs(int failures) {
    if (failures >= 3) {
        return 60000UL;
    }
    return 30000UL;
}

String httpErrorWithBody(int code, const String& body) {
    String base = "http_error_" + String(code);
    if (body.length() == 0) {
        return base;
    }

    JsonDocument errDoc;
    if (!deserializeJson(errDoc, body)) {
        String msg = errDoc["message"] | "";
        if (msg.length() > 0) {
            return base + "_" + msg;
        }
    }

    String preview = body.substring(0, 80);
    preview.replace(' ', '_');
    preview.replace('\n', '_');
    return base + "_" + preview;
}

String trendToApiString(BG_TREND trend) {
    switch (trend) {
        case BG_TREND::DOUBLE_UP:
            return "rising_fast";
        case BG_TREND::SINGLE_UP:
            return "rising";
        case BG_TREND::FORTY_FIVE_UP:
            return "rising";
        case BG_TREND::FLAT:
            return "flat";
        case BG_TREND::FORTY_FIVE_DOWN:
            return "falling";
        case BG_TREND::SINGLE_DOWN:
            return "falling";
        case BG_TREND::DOUBLE_DOWN:
            return "falling_fast";
        case BG_TREND::NOT_COMPUTABLE:
            return "unknown";
        case BG_TREND::RATE_OUT_OF_RANGE:
            return "unknown";
        default:
            return "unknown";
    }
}

String httpErrorToString(int code) {
    switch (code) {
        case -1:
            return "connection_refused";
        case -2:
            return "send_header_failed";
        case -3:
            return "send_payload_failed";
        case -4:
            return "not_connected";
        case -5:
            return "connection_lost";
        case -6:
            return "no_stream";
        case -7:
            return "no_http_server";
        case -8:
            return "too_less_ram";
        case -9:
            return "encoding";
        case -10:
            return "stream_write";
        case -11:
            return "read_timeout";
        default:
            return "unknown";
    }
}

String getApiBaseUrl() {
    String url = SettingsManager.settings.onedigit_external_api_url;
    url.trim();

    // Accept both base URL and full endpoint URL in config.
    String lowerUrl = url;
    lowerUrl.toLowerCase();
    const char* endpointSuffix = "/v1/watchface/content";
    if (lowerUrl.endsWith(endpointSuffix)) {
        url.remove(url.length() - strlen(endpointSuffix));
    }

    while (url.endsWith("/")) {
        url.remove(url.length() - 1);
    }
    return url;
}

bool extractHostPort(const String& url, String& host, uint16_t& port) {
    bool isHttps = url.startsWith("https://");
    bool isHttp = url.startsWith("http://");
    if (!isHttps && !isHttp) {
        return false;
    }

    String rest = url.substring(isHttps ? 8 : 7);
    int slashIdx = rest.indexOf('/');
    if (slashIdx >= 0) {
        rest = rest.substring(0, slashIdx);
    }

    int colonIdx = rest.indexOf(':');
    if (colonIdx >= 0) {
        host = rest.substring(0, colonIdx);
        port = (uint16_t)rest.substring(colonIdx + 1).toInt();
        if (port == 0) {
            return false;
        }
    } else {
        host = rest;
        port = isHttps ? 443 : 80;
    }

    return host.length() > 0;
}

bool parseEndpointUrl(
    const String& url, bool& isHttps, String& host, uint16_t& port, String& pathWithQuery) {
    isHttps = url.startsWith("https://");
    bool isHttp = url.startsWith("http://");
    if (!isHttps && !isHttp) {
        return false;
    }

    String rest = url.substring(isHttps ? 8 : 7);
    int slashIdx = rest.indexOf('/');
    String hostPort = slashIdx >= 0 ? rest.substring(0, slashIdx) : rest;
    String pathPart = slashIdx >= 0 ? rest.substring(slashIdx) : "/";

    int colonIdx = hostPort.indexOf(':');
    if (colonIdx >= 0) {
        host = hostPort.substring(0, colonIdx);
        port = (uint16_t)hostPort.substring(colonIdx + 1).toInt();
        if (port == 0) {
            return false;
        }
    } else {
        host = hostPort;
        port = isHttps ? 443 : 80;
    }

    pathWithQuery = pathPart.length() > 0 ? pathPart : "/";
    return host.length() > 0;
}

uint16_t hexToColor565(const String& color) {
    if (color.length() != 7 || color.charAt(0) != '#') {
        return COLOR_WHITE;
    }

    auto parseHexByte = [](char hi, char lo) -> int {
        auto toVal = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return -1;
        };

        int high = toVal(hi);
        int low = toVal(lo);
        if (high < 0 || low < 0) {
            return -1;
        }
        return (high << 4) | low;
    };

    int r = parseHexByte(color.charAt(1), color.charAt(2));
    int g = parseHexByte(color.charAt(3), color.charAt(4));
    int b = parseHexByte(color.charAt(5), color.charAt(6));
    if (r < 0 || g < 0 || b < 0) {
        return COLOR_WHITE;
    }

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

bool decodeHexFrame(const String& hex, int expectedBytes, std::vector<uint8_t>& out) {
    if (hex.length() != expectedBytes * 2) {
        return false;
    }

    out.clear();
    out.reserve(expectedBytes);

    auto hexToNibble = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    };

    for (int i = 0; i < expectedBytes; i++) {
        int hi = hexToNibble(hex.charAt(i * 2));
        int lo = hexToNibble(hex.charAt(i * 2 + 1));
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }

    return true;
}

bool textFits(const String& text, int availableWidthPx) {
    DisplayManager.setFont(FONT_TYPE::SMALL);
    const int widthPx = static_cast<int>(ceil(DisplayManager.getTextWidth(text.c_str(), 2)));
    return widthPx <= availableWidthPx;
}

bool isPlaceholderText(const String& text) {
    String normalized = text;
    normalized.trim();
    return normalized.length() == 0 || normalized == "--";
}

TEXT_ALIGNMENT parseAlign(const char* align) {
    if (align && strcmp(align, "right") == 0)
        return TEXT_ALIGNMENT::RIGHT;
    if (align && strcmp(align, "center") == 0)
        return TEXT_ALIGNMENT::CENTER;
    return TEXT_ALIGNMENT::LEFT;
}

void setFallback(CachedContent& cache, JsonObject fallbackObj, bool enableScroll) {
    String fallbackText = fallbackObj["text"] | "--";
    if (isPlaceholderText(fallbackText)) {
        cache.available = false;
        cache.selectedIsBitmap = false;
        cache.scrollEnabled = false;
        cache.selectedText = "";
        cache.selectedColor = COLOR_WHITE;
        cache.selectedFrameHex = "";
        cache.selectedBitmapWidth = 0;
        cache.selectedBitmapHeight = 0;
        return;
    }

    cache.available = true;
    cache.selectedIsBitmap = false;
    cache.scrollEnabled = enableScroll;
    cache.selectedText = fallbackText;
    cache.selectedColor = hexToColor565(fallbackObj["color"] | "#FFFFFF");
    cache.selectedFrameHex = "";
    cache.selectedBitmapWidth = 0;
    cache.selectedBitmapHeight = 0;
}

bool buildRequestBody(
    const char* view, const GlucoseReading& reading, uint8_t contentStartX, uint8_t contentHeight,
    String& out, String& outDeviceId) {
    JsonDocument requestDoc;
    String deviceId = SettingsManager.settings.hostname;
    if (deviceId.length() == 0) {
        deviceId = WiFi.macAddress();
    }
    outDeviceId = deviceId;
    requestDoc["deviceId"] = deviceId;
    requestDoc["view"] = view;

    JsonObject display = requestDoc["display"].to<JsonObject>();
    display["widthPx"] = MATRIX_WIDTH;
    display["heightPx"] = MATRIX_HEIGHT;
    display["reservedLeftPx"] = contentStartX;
    display["reservedBottomPx"] = MATRIX_HEIGHT - contentHeight;

    JsonObject capabilities = requestDoc["clientCapabilities"].to<JsonObject>();
    capabilities["canScroll"] = true;
    capabilities["canAnimate"] = true;
    capabilities["supportsBitmap"] = true;
    capabilities["maxFps"] = 10;

    return serializeJson(requestDoc, out) > 0;
}

String getFallbackConnectHost(const String& host) {
    if (host == "ulanziapi.robotsknowbest.com") {
        return "7rmj38fn.up.railway.app";
    }
    return host;
}

bool getDirectConnectIp(const String& host, IPAddress& ip) {
    if (host == "ulanziapi.robotsknowbest.com" || host == "7rmj38fn.up.railway.app") {
        ip = IPAddress(151, 101, 2, 15);
        return true;
    }
    return false;
}

String diagnoseTcpReachability(const String& connectHost, const IPAddress* connectIp, uint16_t port) {
    WiFiClient tcpProbe;
    bool tcpOk = false;
    if (connectIp != nullptr) {
        tcpOk = tcpProbe.connect(*connectIp, port, 1500);
    } else {
        tcpOk = tcpProbe.connect(connectHost.c_str(), port, 1500);
    }
    tcpProbe.stop();
    return tcpOk ? "tcp_ok_tls_failed" : "tcp_failed";
}

bool parseStatusCode(const String& statusLine, int& statusCode) {
    int firstSpace = statusLine.indexOf(' ');
    if (firstSpace < 0) {
        return false;
    }

    int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
    String codePart = secondSpace > firstSpace ? statusLine.substring(firstSpace + 1, secondSpace)
                                               : statusLine.substring(firstSpace + 1);
    codePart.trim();
    statusCode = codePart.toInt();
    return statusCode > 0;
}

bool readHttpResponseBody(Client& client, String& responseBody) {
    int contentLength = -1;
    bool isChunked = false;

    while (client.connected()) {
        String headerLine = client.readStringUntil('\n');
        if (headerLine.length() == 0) {
            continue;
        }

        if (headerLine == "\r") {
            break;
        }

        String normalized = headerLine;
        normalized.trim();
        String lower = normalized;
        lower.toLowerCase();

        if (lower.startsWith("content-length:")) {
            contentLength = normalized.substring(strlen("Content-Length:")).toInt();
        } else if (lower.startsWith("transfer-encoding:") && lower.indexOf("chunked") >= 0) {
            isChunked = true;
        }
    }

    if (isChunked) {
        lastError = "chunked_response_unsupported";
        return false;
    }

    unsigned long startMs = millis();
    responseBody = "";

    if (contentLength >= 0) {
        while ((int)responseBody.length() < contentLength && millis() - startMs < 4000) {
            while (client.available()) {
                responseBody += static_cast<char>(client.read());
                startMs = millis();
                if ((int)responseBody.length() >= contentLength) {
                    break;
                }
            }

            if (!client.connected() && !client.available()) {
                break;
            }
            delay(5);
        }
        return (int)responseBody.length() == contentLength;
    }

    while ((client.connected() || client.available()) && millis() - startMs < 4000) {
        while (client.available()) {
            responseBody += static_cast<char>(client.read());
            startMs = millis();
        }
        delay(5);
    }

    return responseBody.length() > 0;
}

template <typename TClient>
bool postJsonRequest(
    TClient& client, const String& connectHost, const IPAddress* connectIp, const String& hostHeader,
    uint16_t port, const String& pathWithQuery, const String& body, int& statusCode,
    String& responseBody, String& transportError) {
    client.stop();
    client.setTimeout(4000);

    bool connected = false;
    if (connectIp != nullptr) {
        connected = client.connect(*connectIp, port, 4000);
    } else {
        connected = client.connect(connectHost.c_str(), port, 4000);
    }

    if (!connected) {
        transportError = "connect_failed";
        return false;
    }

    client.print(String("POST ") + pathWithQuery + " HTTP/1.1\r\n");
    client.print(String("Host: ") + hostHeader + "\r\n");
    client.print("User-Agent: Nightscout-clock\r\n");
    client.print("Accept: application/json\r\n");
    client.print("Content-Type: application/json\r\n");
    client.print("Connection: close\r\n");
    client.print(String("Content-Length: ") + body.length() + "\r\n\r\n");
    client.print(body);

    unsigned long startMs = millis();
    while (client.connected() && !client.available() && millis() - startMs < 4000) {
        delay(10);
    }

    if (!client.available()) {
        client.stop();
        transportError = "no_response";
        return false;
    }

    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    if (!parseStatusCode(statusLine, statusCode)) {
        client.stop();
        transportError = "invalid_status_line";
        return false;
    }

    if (!readHttpResponseBody(client, responseBody)) {
        client.stop();
        if (lastError == "chunked_response_unsupported") {
            transportError = lastError;
        } else {
            transportError = "body_read_failed";
        }
        return false;
    }

    client.stop();
    return true;
}

bool updateCacheFromApi(
    CachedContent& cache, const char* view, const GlucoseReading& reading, uint8_t contentStartX,
    uint8_t contentWidth, uint8_t contentHeight) {
    const unsigned long nowMs = millis();
    if ((long)(nowMs - nextRetryAfterMs) < 0) {
        lastError = "backoff_active";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        lastError = "wifi_not_connected";
        return false;
    }

    if (ESP.getFreeHeap() < 50000) {
        lastError = "low_heap_" + String(ESP.getFreeHeap());
        return false;
    }

    String body;
    String deviceId;
    if (!buildRequestBody(view, reading, contentStartX, contentHeight, body, deviceId)) {
        lastError = "request_body_error";
        return false;
    }

    String endpoint = getApiBaseUrl() + "/v1/watchface/content";
    bool isHttps = false;
    String host;
    uint16_t port = 0;
    String pathWithQuery;
    if (!parseEndpointUrl(endpoint, isHttps, host, port, pathWithQuery)) {
        lastError = "invalid_endpoint";
        consecutiveFailures++;
        nextRetryAfterMs = millis() + (consecutiveFailures >= 3 ? 300000UL : 60000UL);
        return false;
    }
    lastEndpoint = endpoint;
    lastDeviceId = deviceId;
    lastView = view;
    lastAttemptMs = millis();

    String connectHost = host;
    uint16_t connectPort = port;
    String connectPath = pathWithQuery;
    String connectIp = "";
    if (isHttps && host == "ulanziapi.robotsknowbest.com") {
        // Connect directly to CDN edge IP to avoid DNS/hostname connect issues on device.
        connectHost = "151.101.2.15";
        connectIp = connectHost;
    }

    lastConnectHost = connectHost;
    lastConnectIp = connectIp;
    lastRequestHostHeader = host;

    if (isHttps && connectHost != host) {
        oneDigitSecureClient.setInsecure();

        IPAddress ip;
        IPAddress* ipPtr = nullptr;
        if (ip.fromString(connectHost)) {
            ipPtr = &ip;
        }

        int rawStatusCode = 0;
        String rawResponse;
        String transportError;
        if (!postJsonRequest(
                oneDigitSecureClient, connectHost, ipPtr, host, connectPort, connectPath, body,
                rawStatusCode, rawResponse, transportError)) {
            lastHttpCode = -1;
            lastError = transportError;
            consecutiveFailures++;
            nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
            return false;
        }

        lastHttpCode = rawStatusCode;
        if (rawStatusCode < 200 || rawStatusCode >= 300 || rawResponse.length() == 0) {
            if (rawStatusCode < 200 || rawStatusCode >= 300) {
                lastError = httpErrorWithBody(rawStatusCode, rawResponse);
            } else {
                lastError = "empty_response";
            }
            consecutiveFailures++;
            nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
            return false;
        }

        JsonDocument responseDoc;
        auto parseError = deserializeJson(responseDoc, rawResponse);
        if (parseError) {
            lastError = "json_parse_" + String(parseError.c_str());
            consecutiveFailures++;
            nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
            return false;
        }

        consecutiveFailures = 0;
        nextRetryAfterMs = 0;

        int validForSec = responseDoc["validForSec"] | 60;
        if (validForSec < 60) {
            validForSec = 60;
        }
        cache.validUntilMs = nowMs + static_cast<unsigned long>(validForSec) * 1000UL;

        bool scrollEnabled = responseDoc["renderPlan"]["scroll"]["enabled"] | false;
        TEXT_ALIGNMENT align = parseAlign(responseDoc["renderPlan"]["align"] | "left");
        JsonArray candidates = responseDoc["candidates"].as<JsonArray>();
        JsonObject fallbackObj = responseDoc["fallback"].as<JsonObject>();

        cache.available = false;
        cache.selectedText = "";
        cache.selectedFrameHex = "";
        cache.selectedColor = COLOR_WHITE;
        cache.selectedBitmapWidth = 0;
        cache.selectedBitmapHeight = 0;
        cache.selectedIsBitmap = false;
        cache.scrollEnabled = false;
        cache.selectedAlign = align;

        String firstTextCandidate = "";
        uint16_t firstTextColor = COLOR_WHITE;

        for (auto candidate : candidates) {
            if (!candidate.is<JsonObject>()) {
                continue;
            }

            JsonObject candidateObj = candidate.as<JsonObject>();
            String type = candidateObj["type"] | "";
            type.toLowerCase();

            if (type == "text") {
                String text = candidateObj["text"] | "";
                if (text.length() == 0) {
                    continue;
                }
                uint16_t textColor = hexToColor565(candidateObj["color"] | "#FFFFFF");

                if (firstTextCandidate.length() == 0) {
                    firstTextCandidate = text;
                    firstTextColor = textColor;
                }

                if (textFits(text, contentWidth)) {
                    cache.available = true;
                    cache.selectedIsBitmap = false;
                    cache.selectedText = text;
                    cache.selectedColor = textColor;
                    cache.scrollEnabled = false;
                    lastSuccessMs = millis();
                    lastError = "ok";
                    return true;
                }
                continue;
            }

            if (type == "bitmap") {
                int widthPx = candidateObj["widthPx"] | 0;
                int heightPx = candidateObj["heightPx"] | 0;
                JsonArray frames = candidateObj["frames"].as<JsonArray>();
                if (frames.isNull() || frames.size() == 0) {
                    continue;
                }
                String firstFrame = frames[0] | "";
                if (widthPx <= 0 || heightPx <= 0 || firstFrame.length() == 0) {
                    continue;
                }

                if (widthPx <= contentWidth && heightPx <= contentHeight) {
                    cache.available = true;
                    cache.selectedIsBitmap = true;
                    cache.selectedFrameHex = firstFrame;
                    cache.selectedBitmapWidth = widthPx;
                    cache.selectedBitmapHeight = heightPx;
                    cache.selectedColor = hexToColor565(candidateObj["color"] | "#FFFFFF");
                    cache.scrollEnabled = false;
                    lastSuccessMs = millis();
                    lastError = "ok";
                    return true;
                }
            }
        }

        if (scrollEnabled && firstTextCandidate.length() > 0) {
            cache.available = true;
            cache.selectedIsBitmap = false;
            cache.selectedText = firstTextCandidate;
            cache.selectedColor = firstTextColor;
            cache.scrollEnabled = true;
            cache.scrollOffset = 0;
            cache.lastScrollStepMs = 0;
            lastSuccessMs = millis();
            lastError = "ok";
            return true;
        }

        setFallback(cache, fallbackObj, false);
        lastSuccessMs = millis();
        lastError = "ok";
        return true;
    }

    HTTPClient http;
    int code = -1;
    String response;
    bool beginOk = false;

    if (isHttps) {
        oneDigitSecureClient.setInsecure();
        beginOk = http.begin(oneDigitSecureClient, connectHost, connectPort, connectPath, true);
    } else {
        beginOk = http.begin(oneDigitPlainClient, connectHost, connectPort, connectPath, false);
    }

    if (!beginOk) {
        lastHttpCode = -1;
        lastError = "http_begin_failed";
        consecutiveFailures++;
        nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
        return false;
    }

    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setTimeout(800);
    http.setReuse(false);
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "Nightscout-clock");
    http.addHeader("Content-Type", "application/json");
    if (connectHost != host) {
        http.addHeader("Host", host);
    }
    code = http.POST(body);
    if (code > 0) {
        response = http.getString();
    }
    http.end();

    lastHttpCode = code;

    if (code < 200 || code >= 300 || response.length() == 0) {
        if (code < 200 || code >= 300) {
            if (code < 0) {
                lastError = "http_error_" + String(code) + "_" + httpErrorToString(code);
            } else {
                lastError = httpErrorWithBody(code, response);
            }
        } else {
            lastError = "empty_response";
        }
        consecutiveFailures++;
        nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
        return false;
    }

    JsonDocument responseDoc;
    auto parseError = deserializeJson(responseDoc, response);
    if (parseError) {
        lastError = "json_parse_" + String(parseError.c_str());
        consecutiveFailures++;
        nextRetryAfterMs = millis() + computeRetryDelayMs(consecutiveFailures);
        return false;
    }

    consecutiveFailures = 0;
    nextRetryAfterMs = 0;

    int validForSec = responseDoc["validForSec"] | 60;
    if (validForSec < 60) {
        validForSec = 60;
    }
    cache.validUntilMs = nowMs + static_cast<unsigned long>(validForSec) * 1000UL;

    bool scrollEnabled = responseDoc["renderPlan"]["scroll"]["enabled"] | false;
    TEXT_ALIGNMENT align = parseAlign(responseDoc["renderPlan"]["align"] | "left");
    JsonArray candidates = responseDoc["candidates"].as<JsonArray>();
    JsonObject fallbackObj = responseDoc["fallback"].as<JsonObject>();

    cache.available = false;
    cache.selectedText = "";
    cache.selectedFrameHex = "";
    cache.selectedColor = COLOR_WHITE;
    cache.selectedBitmapWidth = 0;
    cache.selectedBitmapHeight = 0;
    cache.selectedIsBitmap = false;
    cache.scrollEnabled = false;
    cache.selectedAlign = align;

    String firstTextCandidate = "";
    uint16_t firstTextColor = COLOR_WHITE;

    for (auto candidate : candidates) {
        if (!candidate.is<JsonObject>()) {
            continue;
        }

        JsonObject candidateObj = candidate.as<JsonObject>();
        String type = candidateObj["type"] | "";
        type.toLowerCase();

        if (type == "text") {
            String text = candidateObj["text"] | "";
            if (isPlaceholderText(text)) {
                continue;
            }
            uint16_t textColor = hexToColor565(candidateObj["color"] | "#FFFFFF");

            if (firstTextCandidate.length() == 0) {
                firstTextCandidate = text;
                firstTextColor = textColor;
            }

            if (textFits(text, contentWidth)) {
                cache.available = true;
                cache.selectedIsBitmap = false;
                cache.selectedText = text;
                cache.selectedColor = textColor;
                cache.scrollEnabled = false;
                lastSuccessMs = millis();
                lastError = "ok";
                return true;
            }
            continue;
        }

        if (type == "bitmap") {
            int widthPx = candidateObj["widthPx"] | 0;
            int heightPx = candidateObj["heightPx"] | 0;
            JsonArray frames = candidateObj["frames"].as<JsonArray>();
            if (frames.isNull() || frames.size() == 0) {
                continue;
            }
            String firstFrame = frames[0] | "";
            if (widthPx <= 0 || heightPx <= 0 || firstFrame.length() == 0) {
                continue;
            }

            if (widthPx <= contentWidth && heightPx <= contentHeight) {
                cache.available = true;
                cache.selectedIsBitmap = true;
                cache.selectedFrameHex = firstFrame;
                cache.selectedBitmapWidth = widthPx;
                cache.selectedBitmapHeight = heightPx;
                cache.selectedColor = hexToColor565(candidateObj["color"] | "#FFFFFF");
                cache.scrollEnabled = false;
                lastSuccessMs = millis();
                lastError = "ok";
                return true;
            }
        }
    }

    if (scrollEnabled && firstTextCandidate.length() > 0) {
        cache.available = true;
        cache.selectedIsBitmap = false;
        cache.selectedText = firstTextCandidate;
        cache.selectedColor = firstTextColor;
        // Text doesn't fit; enable device-side scrolling
        cache.scrollEnabled = true;
        lastSuccessMs = millis();
        lastError = "ok";
        return true;
    }

    setFallback(cache, fallbackObj, false);
    lastSuccessMs = millis();
    lastError = "ok";
    return true;
}

CachedContent& getCacheForView(const char* view) {
    if (String(view) == "onedigit_dual") {
        return oneDigitDualCache;
    }
    return oneDigitCache;
}

}  // namespace

bool isOneDigitExternalContentScrolling(const char* view) {
    const CachedContent& cache = getCacheForView(view);
    return cache.available && !cache.selectedIsBitmap && cache.scrollEnabled;
}

namespace {

void renderTextCandidate(
    CachedContent& cache, uint8_t contentStartX, uint8_t contentWidth, uint8_t contentHeight) {
    (void)contentHeight;
    DisplayManager.setFont(FONT_TYPE::SMALL);
    DisplayManager.setTextColor(cache.selectedColor);

    if (cache.scrollEnabled) {
        // Advance scroll position at ~80 ms per pixel (~12 px/s)
        unsigned long now = millis();
        if (cache.lastScrollStepMs == 0)
            cache.lastScrollStepMs = now;
        if (now - cache.lastScrollStepMs >= 80) {
            cache.scrollOffset++;
            cache.lastScrollStepMs = now;
            int textW = (int)DisplayManager.getTextWidth(cache.selectedText.c_str(), 2);
            if (cache.scrollOffset > contentWidth + textW) {
                cache.scrollOffset = 0;
            }
        }
        // Draw text entering from the right edge of the content area.
        // Left-side masking is handled by the face renderer to preserve pixel-wise scrolling.
        int16_t x = (int16_t)(contentStartX + contentWidth) - cache.scrollOffset;
        DisplayManager.printText(
            x, 6, cache.selectedText.c_str(), TEXT_ALIGNMENT::LEFT, 2,
            false);  // no immediate show to avoid transient overlap/flicker
        return;
    }

    // Text fits: render with alignment
    int16_t x = contentStartX;
    if (cache.selectedAlign == TEXT_ALIGNMENT::RIGHT) {
        x = contentStartX + contentWidth;  // rightmost edge of content area
    } else if (cache.selectedAlign == TEXT_ALIGNMENT::CENTER) {
        x = contentStartX + contentWidth / 2;  // center of content area
    }
    DisplayManager.printText(
        x, 6, cache.selectedText.c_str(), cache.selectedAlign, 2,
        false);  // shown by following face draw calls
}

void renderBitmapCandidate(
    const CachedContent& cache, uint8_t contentStartX, uint8_t contentWidth, uint8_t contentHeight) {
    int bytesPerRow = (cache.selectedBitmapWidth + 7) / 8;
    int expectedBytes = bytesPerRow * cache.selectedBitmapHeight;

    std::vector<uint8_t> bitmap;
    if (!decodeHexFrame(cache.selectedFrameHex, expectedBytes, bitmap)) {
        return;
    }

    int x = contentStartX + (contentWidth - cache.selectedBitmapWidth) / 2;
    int y = (contentHeight - cache.selectedBitmapHeight) / 2;
    if (y < 0) {
        y = 0;
    }

    DisplayManager.drawBitmap(
        x, y, bitmap.data(), cache.selectedBitmapWidth, cache.selectedBitmapHeight, cache.selectedColor);
}
}  // namespace

bool renderOneDigitExternalContent(
    const char* view, const GlucoseReading& primaryReading, uint8_t contentStartX, uint8_t contentWidth,
    uint8_t contentHeight) {
    String apiBaseUrl = getApiBaseUrl();
    if (apiBaseUrl.length() == 0) {
        return false;
    }

    CachedContent& cache = getCacheForView(view);

    if (!cache.available) {
        return false;
    }

    if (cache.selectedIsBitmap) {
        renderBitmapCandidate(cache, contentStartX, contentWidth, contentHeight);
    } else {
        renderTextCandidate(cache, contentStartX, contentWidth, contentHeight);
    }

    return true;
}

void refreshOneDigitExternalContentCache(
    const char* view, const GlucoseReading& primaryReading, uint8_t contentStartX, uint8_t contentWidth,
    uint8_t contentHeight) {
    String apiBaseUrl = getApiBaseUrl();
    if (apiBaseUrl.length() == 0) {
        return;
    }

    if (kDisableExternalApiFetchForStability) {
        lastError = "disabled_for_stability";
        return;
    }

    CachedContent& cache = getCacheForView(view);
    const unsigned long nowMs = millis();
    if (cache.available && (long)(nowMs - cache.validUntilMs) < 0) {
        return;
    }

    if (cache.available && consecutiveFailures >= 3 && (long)(nowMs - cache.validUntilMs) >= 0) {
        // Avoid showing very old stale external content after repeated API failures.
        cache.available = false;
    }

    // Keep existing cached content if refresh fails, to avoid display dropouts.
    updateCacheFromApi(cache, view, primaryReading, contentStartX, contentWidth, contentHeight);
}

String getOneDigitExternalContentStatusJson() {
    JsonDocument doc;
    doc["configuredUrl"] = SettingsManager.settings.onedigit_external_api_url;
    doc["normalizedBaseUrl"] = getApiBaseUrl();
    doc["lastEndpoint"] = lastEndpoint;
    doc["lastDeviceId"] = lastDeviceId;
    doc["lastView"] = lastView;
    doc["lastConnectHost"] = lastConnectHost;
    doc["lastConnectIp"] = lastConnectIp;
    doc["lastRequestHostHeader"] = lastRequestHostHeader;
    doc["lastAttemptMs"] = lastAttemptMs;
    doc["lastSuccessMs"] = lastSuccessMs;
    doc["lastHttpCode"] = lastHttpCode;
    doc["lastError"] = lastError;
    doc["consecutiveFailures"] = consecutiveFailures;
    doc["nextRetryInMs"] = nextRetryAfterMs > millis() ? nextRetryAfterMs - millis() : 0;
    doc["cacheOneDigitAvailable"] = oneDigitCache.available;
    doc["cacheOneDigitDualAvailable"] = oneDigitDualCache.available;
    doc["cacheOneDigitValidForMs"] =
        oneDigitCache.validUntilMs > millis() ? oneDigitCache.validUntilMs - millis() : 0;
    doc["cacheOneDigitDualValidForMs"] =
        oneDigitDualCache.validUntilMs > millis() ? oneDigitDualCache.validUntilMs - millis() : 0;

    String json;
    serializeJson(doc, json);
    return json;
}
