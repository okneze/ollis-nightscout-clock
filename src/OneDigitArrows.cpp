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
    // Standalone arrows/dots are disabled. Trend is now integrated directly into the BG digit.
}