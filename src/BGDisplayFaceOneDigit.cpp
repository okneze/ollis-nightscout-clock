#include "BGDisplayFaceOneDigit.h"

#include "BGDisplayManager.h"
#include "OneDigitArrows.h"
#include "globals.h"

namespace {
int roundToNearestTen(int value) { return ((value + 5) / 10) * 10; }

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
    const int roundedSgv = roundToNearestTen(lastReading.sgv);
    const int tensDigit = (roundedSgv / 10) % 10;

    DisplayManager.setFont(FONT_TYPE::SMALL);
    DisplayManager.setTextColor(dataIsOld ? BG_COLOR_OLD : getRoundedBgColor(roundedSgv));
    DisplayManager.printText(0, 6, String(tensDigit).c_str(), TEXT_ALIGNMENT::LEFT, 2);

    // Draw arrow immediately after the 3px-wide digit, with no extra spacing.
    ODA::draw(3, lastReading.trend, dataIsOld);

    BGDisplayManager_::drawTimerBlocks(lastReading, MATRIX_WIDTH, 0, 7);
}
