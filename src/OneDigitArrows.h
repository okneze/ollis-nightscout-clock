#ifndef ONEDIGITARROWS_H
#define ONEDIGITARROWS_H

#include <Arduino.h>

#include "BGSource.h"

namespace OneDigitArrows {
void draw(int16_t x, BG_TREND trend, bool dataIsOld, int16_t topY = 1, int16_t bottomY = 5);
}

namespace ODA = OneDigitArrows;

#endif  // ONEDIGITARROWS_H