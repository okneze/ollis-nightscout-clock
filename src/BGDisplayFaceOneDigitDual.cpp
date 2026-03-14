#include "BGDisplayFaceOneDigitDual.h"

#include "BGDisplayManager.h"
#include "BGSourceManager.h"
#include "globals.h"

namespace {
int roundToNearestTenDual(int value) { return ((value + 5) / 10) * 10; }

uint16_t getRoundedBgColorDual(int roundedSgv) {
    if (roundedSgv < 100) {
        return COLOR_BLUE;
    }
    if (roundedSgv < 200) {
        return COLOR_GREEN;
    }
    if (roundedSgv < 300) {
        return COLOR_YELLOW;
    }
    return COLOR_RED;
}

void drawOnePixelTrendArrowDual(int x, BG_TREND trend, bool dataIsOld) {
    const uint16_t color = dataIsOld ? BG_COLOR_OLD : COLOR_WHITE;

    switch (trend) {
        case BG_TREND::DOUBLE_UP:
            DisplayManager.drawPixel(x, 0, color, false);
            DisplayManager.drawPixel(x, 1, color, false);
            DisplayManager.drawPixel(x, 2, color, false);
            DisplayManager.drawPixel(x, 3, color, false);
            break;
        case BG_TREND::SINGLE_UP:
            DisplayManager.drawPixel(x, 1, color, false);
            DisplayManager.drawPixel(x, 2, color, false);
            DisplayManager.drawPixel(x, 3, color, false);
            break;
        case BG_TREND::FORTY_FIVE_UP:
            DisplayManager.drawPixel(x, 2, color, false);
            DisplayManager.drawPixel(x, 3, color, false);
            break;
        case BG_TREND::FLAT:
            DisplayManager.drawPixel(x, 3, color, false);
            DisplayManager.drawPixel(x, 4, color, false);
            break;
        case BG_TREND::FORTY_FIVE_DOWN:
            DisplayManager.drawPixel(x, 4, color, false);
            DisplayManager.drawPixel(x, 5, color, false);
            break;
        case BG_TREND::SINGLE_DOWN:
            DisplayManager.drawPixel(x, 4, color, false);
            DisplayManager.drawPixel(x, 5, color, false);
            DisplayManager.drawPixel(x, 6, color, false);
            break;
        case BG_TREND::DOUBLE_DOWN:
            DisplayManager.drawPixel(x, 4, color, false);
            DisplayManager.drawPixel(x, 5, color, false);
            DisplayManager.drawPixel(x, 6, color, false);
            DisplayManager.drawPixel(x, 7, color, false);
            break;
        default:
            break;
    }
}
}  // namespace

void BGDisplayFaceOneDigitDual::showReadings(
    const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    if (readings.empty()) {
        showNoData();
        return;
    }

    // Primary reading: digit at x=0, arrow at x=3
    const auto firstReading = readings.back();
    const int roundedFirst = roundToNearestTenDual(firstReading.sgv);
    const int tensFirst = (roundedFirst / 10) % 10;

    DisplayManager.setFont(FONT_TYPE::SMALL);
    DisplayManager.setTextColor(dataIsOld ? BG_COLOR_OLD : getRoundedBgColorDual(roundedFirst));
    DisplayManager.printText(0, 6, String(tensFirst).c_str(), TEXT_ALIGNMENT::LEFT, 2);
    drawOnePixelTrendArrowDual(3, firstReading.trend, dataIsOld);

    // Secondary reading: 1px gap at x=4, digit at x=5, arrow at x=8
    const auto secondReadings = bgSourceManager.getSecondaryGlucoseData();
    if (!secondReadings.empty()) {
        const auto secondReading = secondReadings.back();
        const bool secondIsOld = secondReading.getSecondsAgo() >
                                 60 * SettingsManager.settings.bg_data_too_old_threshold_minutes;
        const int roundedSecond = roundToNearestTenDual(secondReading.sgv);
        const int tensSecond = (roundedSecond / 10) % 10;

        DisplayManager.setTextColor(secondIsOld ? BG_COLOR_OLD : getRoundedBgColorDual(roundedSecond));
        DisplayManager.printText(5, 6, String(tensSecond).c_str(), TEXT_ALIGNMENT::LEFT, 2);
        drawOnePixelTrendArrowDual(8, secondReading.trend, secondIsOld);
    }

    BGDisplayManager_::drawTimerBlocks(firstReading, MATRIX_WIDTH, 0, 7);
}
