// Build check: the firmware must still compile and run with the U8g2 fonts disabled
// (USE_U8G2 = 0 in inkblink.ino) - then only the built-in "Classic" font family exists.
#include "inkblink.ino"
int main() {
  for (int i = 0; i < 64; i++) g_pin[i] = 1;
  fontInit();
  cfg.famIdx = 3;                 // a stored family that does not exist without U8g2
  applyLayout();
  drawMenu();
  return !(cfg.famIdx == 0 && fontFamilyCount() == 1);
}
