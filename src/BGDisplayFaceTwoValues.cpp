#include "BGDisplayFaceTwoValues.h"

#include "BGDisplayManager.h"
#include "BGSourceManager.h"
#include "globals.h"

namespace {
int getTextPixelWidth(const String& value) {
    return static_cast<int>(DisplayManager.getTextWidth(value.c_str(), 2));
}

void trimLeftToWidth(String& value, int maxWidth) {
    while (value.length() > 1 && getTextPixelWidth(value) > maxWidth) {
        value = value.substring(1);
    }
}

void drawMiniTrendArrow(const GlucoseReading& reading, int16_t x, int16_t y, bool dataIsOld) {
    uint16_t color = BG_COLOR_OLD;
    if (!dataIsOld) {
        color = (reading.trend == BG_TREND::DOUBLE_UP || reading.trend == BG_TREND::DOUBLE_DOWN)
                    ? COLOR_RED
                    : COLOR_WHITE;
    }

    if (dataIsOld) {
        DisplayManager.drawPixel(x + 1, y + 1, color, false);
        return;
    }

    switch (reading.trend) {
        case BG_TREND::DOUBLE_UP:
        case BG_TREND::SINGLE_UP:
            DisplayManager.drawPixel(x + 1, y + 0, color, false);
            DisplayManager.drawPixel(x + 0, y + 1, color, false);
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            DisplayManager.drawPixel(x + 2, y + 1, color, false);
            break;
        case BG_TREND::DOUBLE_DOWN:
        case BG_TREND::SINGLE_DOWN:
            DisplayManager.drawPixel(x + 0, y + 1, color, false);
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            DisplayManager.drawPixel(x + 2, y + 1, color, false);
            DisplayManager.drawPixel(x + 1, y + 2, color, false);
            break;
        case BG_TREND::FORTY_FIVE_UP:
            DisplayManager.drawPixel(x + 2, y + 0, color, false);
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            DisplayManager.drawPixel(x + 0, y + 2, color, false);
            break;
        case BG_TREND::FORTY_FIVE_DOWN:
            DisplayManager.drawPixel(x + 0, y + 0, color, false);
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            DisplayManager.drawPixel(x + 2, y + 2, color, false);
            break;
        case BG_TREND::FLAT:
            DisplayManager.drawPixel(x + 0, y + 1, color, false);
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            DisplayManager.drawPixel(x + 2, y + 1, color, false);
            break;
        default:
            DisplayManager.drawPixel(x + 1, y + 1, color, false);
            break;
    }
}
}  // namespace

void BGDisplayFaceTwoValues::showReadings(
    const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    if (readings.empty()) {
        showNoData();
        return;
    }

    const auto firstReading = readings.back();
    const auto secondReadings = bgSourceManager.getSecondaryGlucoseData();
    const bool hasSecondReading = !secondReadings.empty();
    const auto secondReading =
        hasSecondReading ? secondReadings.back() : GlucoseReading{0, BG_TREND::NONE, 0};

    bool secondDataIsOld = true;
    if (hasSecondReading) {
        secondDataIsOld = secondReading.getSecondsAgo() >
                          60 * SettingsManager.settings.bg_data_too_old_threshold_minutes;
    }

    DisplayManager.setFont(FONT_TYPE::SMALL);

    String firstValue = getPrintableReading(firstReading.sgv);
    String secondValue = hasSecondReading ? getPrintableReading(secondReading.sgv) : "--";

    const int arrowWidth = 3;
    const int gapAfterFirstValue = 0;
    const int gapBetweenValues = 1;
    const int gapBeforeSecondArrow = 0;
    const int totalPadding =
        gapAfterFirstValue + arrowWidth + gapBetweenValues + gapBeforeSecondArrow + arrowWidth;

    int availableForText = MATRIX_WIDTH - totalPadding;
    if (availableForText < 2) {
        availableForText = 2;
    }

    int firstWidth = getTextPixelWidth(firstValue);
    int secondWidth = getTextPixelWidth(secondValue);

    if (firstWidth + secondWidth > availableForText) {
        int targetFirst = availableForText / 2;
        int targetSecond = availableForText - targetFirst;
        trimLeftToWidth(firstValue, targetFirst);
        trimLeftToWidth(secondValue, targetSecond);
        firstWidth = getTextPixelWidth(firstValue);
        secondWidth = getTextPixelWidth(secondValue);
    }

    const int secondArrowX = MATRIX_WIDTH - arrowWidth;
    const int secondTextX = secondArrowX - gapBeforeSecondArrow - secondWidth;

    int firstTextX = 0;
    int firstArrowX = firstTextX + firstWidth + gapAfterFirstValue;
    if (firstArrowX + arrowWidth + gapBetweenValues > secondTextX) {
        firstTextX = secondTextX - gapBetweenValues - arrowWidth - gapAfterFirstValue - firstWidth;
        if (firstTextX < 0) {
            firstTextX = 0;
        }
        firstArrowX = firstTextX + firstWidth + gapAfterFirstValue;
    }

    if (dataIsOld) {
        DisplayManager.setTextColor(BG_COLOR_OLD);
    } else {
        SetDisplayColorByBGValue(firstReading);
    }
    DisplayManager.printText(firstTextX, 6, firstValue.c_str(), TEXT_ALIGNMENT::LEFT, 2);
    drawMiniTrendArrow(firstReading, firstArrowX, 2, dataIsOld);

    if (hasSecondReading && !secondDataIsOld) {
        SetDisplayColorByBGValue(secondReading);
    } else {
        DisplayManager.setTextColor(BG_COLOR_OLD);
    }
    DisplayManager.printText(secondTextX, 6, secondValue.c_str(), TEXT_ALIGNMENT::LEFT, 2);
    drawMiniTrendArrow(secondReading, secondArrowX, 2, secondDataIsOld);

    BGDisplayManager_::drawTimerBlocks(firstReading, MATRIX_WIDTH, 0, 7);
}
