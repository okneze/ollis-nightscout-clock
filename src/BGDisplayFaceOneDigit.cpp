#include "BGDisplayFaceOneDigit.h"

#include "BGDisplayManager.h"
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

void drawOnePixelTrendArrow(int x, BG_TREND trend, bool dataIsOld) {
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
    drawOnePixelTrendArrow(3, lastReading.trend, dataIsOld);

    BGDisplayManager_::drawTimerBlocks(lastReading, MATRIX_WIDTH, 0, 7);
}
