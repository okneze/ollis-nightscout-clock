#include "OneDigitArrows.h"

#include "DisplayManager.h"
#include "globals.h"

namespace {
void drawVerticalSegment(int16_t x, int16_t startY, int count, uint16_t color) {
    for (int offset = 0; offset < count; ++offset) {
        DisplayManager.drawPixel(x, startY + offset, color, false);
    }
}
}  // namespace

void OneDigitArrows::draw(int16_t x, BG_TREND trend, bool dataIsOld, int16_t topY, int16_t bottomY) {
    const uint16_t color = dataIsOld ? BG_COLOR_OLD : COLOR_WHITE;

    switch (trend) {
        case BG_TREND::DOUBLE_UP:
        case BG_TREND::RATE_OUT_OF_RANGE:
            drawVerticalSegment(x, topY, 3, color);
            break;
        case BG_TREND::SINGLE_UP:
            drawVerticalSegment(x, topY, 2, color);
            break;
        case BG_TREND::FORTY_FIVE_UP:
            DisplayManager.drawPixel(x, topY, color, false);
            break;
        case BG_TREND::FORTY_FIVE_DOWN:
            DisplayManager.drawPixel(x, bottomY, color, false);
            break;
        case BG_TREND::SINGLE_DOWN:
            drawVerticalSegment(x, bottomY - 1, 2, color);
            break;
        case BG_TREND::DOUBLE_DOWN:
            drawVerticalSegment(x, bottomY - 2, 3, color);
            break;
        case BG_TREND::FLAT:
        case BG_TREND::NONE:
        case BG_TREND::NOT_COMPUTABLE:
        default:
            break;
    }
}