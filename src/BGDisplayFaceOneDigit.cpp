#include "BGDisplayFaceOneDigit.h"

#include "BGDisplayManager.h"
#include "OneDigitArrows.h"
#include "OneDigitExternalContent.h"
#include "globals.h"

namespace {
int roundToNearestTen(int value) { return (((value > 394 ? 394 : value) + 5) / 10) * 10; }

uint16_t getRoundedBgColor(int roundedSgv) {
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

void BGDisplayFaceOneDigit::showReadings(
    const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    if (readings.empty()) {
        showNoData();
        return;
    }

    const auto lastReading = readings.back();
    const bool isHigh = lastReading.sgv >= 400;
    const int roundedSgv = roundToNearestTen(lastReading.sgv);
    const int tensDigit = (roundedSgv / 10) % 10;

    // Clear content area first so stale external text is removed when API returns no content.
    DisplayManager.clearMatrixPartNoUpdate(6, 0, MATRIX_WIDTH - 6, MATRIX_HEIGHT);
    // External API content is rendered first so local BG/trend visuals always stay visible.
    renderOneDigitExternalContent("onedigit", lastReading, 6, MATRIX_WIDTH - 6, 6);
    DisplayManager.clearMatrixPartNoUpdate(0, 0, 6, MATRIX_HEIGHT);

    const String displayText = isHigh ? "H" : String(tensDigit);
    const uint16_t baseColor = dataIsOld ? BG_COLOR_OLD : (isHigh ? COLOR_RED : getRoundedBgColor(roundedSgv));
    DisplayManager.drawDigitWithTrend(0, 6, displayText[0], baseColor, lastReading.trend, dataIsOld, false);
    renderOneDigitBgtir1("onedigit", 4, 1);

    BGDisplayManager_::drawTimerBlocks(lastReading, MATRIX_WIDTH, 0, 7);
}
