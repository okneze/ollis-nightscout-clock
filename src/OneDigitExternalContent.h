#ifndef ONEDIGITEXTERNALCONTENT_H
#define ONEDIGITEXTERNALCONTENT_H

#include <Arduino.h>

#include "BGSource.h"

void refreshOneDigitExternalContentCache(
    const char* view, const GlucoseReading& primaryReading, uint8_t contentStartX, uint8_t contentWidth,
    uint8_t contentHeight);

bool renderOneDigitExternalContent(
    const char* view, const GlucoseReading& primaryReading, uint8_t contentStartX, uint8_t contentWidth,
    uint8_t contentHeight);

bool renderOneDigitBgtir1(const char* view, int16_t x, int16_t y = 1);

bool renderOneDigitBgtir2(const char* view, int16_t x, int16_t y = 1);

String getOneDigitExternalContentStatusJson();

bool isOneDigitExternalContentScrolling(const char* view);

#endif  // ONEDIGITEXTERNALCONTENT_H
