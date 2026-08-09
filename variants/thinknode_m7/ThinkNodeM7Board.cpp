#include "ThinkNodeM7Board.h"

void ThinkNodeM7Board::begin() {
  ESP32Board::begin();

  // A pin left as INPUT sources no current, so the LED that onBeforeTransmit() drives stays dark.
  // Both are active low, so the idle level is HIGH.
#ifdef P_LORA_TX_LED
  pinMode(P_LORA_TX_LED, OUTPUT);
  digitalWrite(P_LORA_TX_LED, HIGH);
#endif
#ifdef PIN_STATUS_LED
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, !LED_STATE_ON);
#endif
}

void ThinkNodeM7Board::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
  esp_deep_sleep_start();
}

void ThinkNodeM7Board::powerOff()  {
  enterDeepSleep(0);
}

const char* ThinkNodeM7Board::getManufacturerName() const {
  return "Elecrow ThinkNode M7";
}
