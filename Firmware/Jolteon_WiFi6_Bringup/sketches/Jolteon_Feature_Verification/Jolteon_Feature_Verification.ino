#include "Verification.h"

// Builds with Arduino Zero (Native USB). GPIO uses physical PA/PB names,
// continuing the direct-register approach of the original blinky sketch.
void setup() { verificationBegin(); }
void loop() { verificationLoop(); }
