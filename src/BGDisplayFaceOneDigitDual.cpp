#include "BGDisplayFaceOneDigitDual.h"

#include "BGDisplayManager.h"
#include "BGSourceManager.h"
#include "OneDigitArrows.h"
#include "OneDigitExternalContent.h"
#include "globals.h"

namespace {
int roundToNearestTenDual(int value) { return (((value > 394 ? 394 : value) + 5) / 10) * 10; }

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
    const bool firstIsHigh = firstReading.sgv >= 400;
    const int roundedFirst = roundToNearestTenDual(firstReading.sgv);
    const int tensFirst = (roundedFirst / 10) % 10;

    // Clear content area first so stale external text is removed when API returns no content.
    DisplayManager.clearMatrixPartNoUpdate(12, 0, MATRIX_WIDTH - 12, MATRIX_HEIGHT);
    // External API content is rendered first so local BG/trend visuals always stay visible.
    renderOneDigitExternalContent("onedigit_dual", firstReading, 12, MATRIX_WIDTH - 12, 6);
    DisplayManager.clearMatrixPartNoUpdate(0, 0, 12, MATRIX_HEIGHT);

    const String displayFirst = firstIsHigh ? "H" : String(tensFirst);
    const uint16_t colorFirst = dataIsOld ? BG_COLOR_OLD : (firstIsHigh ? COLOR_RED : getRoundedBgColorDual(roundedFirst));
    DisplayManager.drawDigitWithTrend(0, 6, displayFirst[0], colorFirst, firstReading.trend, dataIsOld, false);
    renderOneDigitBgtir1("onedigit_dual", 4, 1);

    // Secondary reading: 1px gap at x=5, digit at x=6, arrow at x=9
    const auto secondReadings = bgSourceManager.getSecondaryGlucoseData();
    if (!secondReadings.empty()) {
        const auto secondReading = secondReadings.back();
        const bool secondIsOld = secondReading.getSecondsAgo() >
                                 60 * SettingsManager.settings.bg_data_too_old_threshold_minutes;
        const bool secondIsHigh = secondReading.sgv >= 400;
        const int roundedSecond = roundToNearestTenDual(secondReading.sgv);
        const int tensSecond = (roundedSecond / 10) % 10;

        const String displaySecond = secondIsHigh ? "H" : String(tensSecond);
        const uint16_t colorSecond = secondIsOld ? BG_COLOR_OLD : (secondIsHigh ? COLOR_RED : getRoundedBgColorDual(roundedSecond));
        DisplayManager.drawDigitWithTrend(6, 6, displaySecond[0], colorSecond, secondReading.trend, secondIsOld, false);
        renderOneDigitBgtir2("onedigit_dual", 10, 1);
    }

    BGDisplayManager_::drawTimerBlocks(firstReading, MATRIX_WIDTH, 0, 7);
}
