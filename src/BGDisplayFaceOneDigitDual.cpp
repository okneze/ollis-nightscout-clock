#include "BGDisplayFaceOneDigitDual.h"

#include "BGDisplayManager.h"
#include "BGSourceManager.h"
#include "OneDigitArrows.h"
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
    ODA::draw(3, firstReading.trend, dataIsOld);

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
        ODA::draw(8, secondReading.trend, secondIsOld);
    }

    BGDisplayManager_::drawTimerBlocks(firstReading, MATRIX_WIDTH, 0, 7);
}
