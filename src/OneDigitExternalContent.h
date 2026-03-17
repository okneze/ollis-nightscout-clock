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

String getOneDigitExternalContentStatusJson();

bool isOneDigitExternalContentScrolling(const char* view);

#endif  // ONEDIGITEXTERNALCONTENT_H
