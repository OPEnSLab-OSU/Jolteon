/** Jolteon Rewire / Feather M0 protocol bench console.
 * See README.md and docs/Feather_M0_Wiring.svg before connecting hardware.
 * Serial Monitor: 115200, newline or CRLF. Run baseline nosd without a card.
 * Baseline enables Hypnos first; separate manual enable/disable commands remain.
 */
#include "Bringup.h"
void setup() { bringupBegin(); }
void loop() { bringupLoop(); }
