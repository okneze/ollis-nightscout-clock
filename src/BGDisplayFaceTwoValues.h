#ifndef BGDISPLAYFACETWOVALUES_H
#define BGDISPLAYFACETWOVALUES_H

#include "BGDisplayFaceTextBase.h"

class BGDisplayFaceTwoValues : public BGDisplayFaceTextBase {
public:
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;
};

#endif  // BGDISPLAYFACETWOVALUES_H
