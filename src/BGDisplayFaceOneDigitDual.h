#ifndef BGDISPLAYFACEONEDIGITDUAL_H
#define BGDISPLAYFACEONEDIGITDUAL_H

#include "BGDisplayFaceTextBase.h"

class BGDisplayFaceOneDigitDual : public BGDisplayFaceTextBase {
public:
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;
};

#endif  // BGDISPLAYFACEONEDIGITDUAL_H
