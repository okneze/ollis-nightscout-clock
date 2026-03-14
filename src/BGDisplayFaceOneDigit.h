#ifndef BGDISPLAYFACEONEDIGIT_H
#define BGDISPLAYFACEONEDIGIT_H

#include "BGDisplayFaceTextBase.h"

class BGDisplayFaceOneDigit : public BGDisplayFaceTextBase {
public:
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;
};

#endif  // BGDISPLAYFACEONEDIGIT_H
