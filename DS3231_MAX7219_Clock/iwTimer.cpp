// Класс, реализующий простейший таймер на основе функции millis()

#include "Arduino.h"

class iwTimer
{
  uint32_t lastMillis;
  uint32_t interval;

  public:
  iwTimer(uint32_t anInterval) {
    lastMillis = 0;
    interval = anInterval;
  }

  bool check() { // проверить, не прошёл ли заданный интервал
    uint32_t currMillis = millis();
    
    if ( (currMillis - lastMillis) >= interval) {
      lastMillis = currMillis;
      return true;
    } else return false;
  }
  void reSet() { // сбросить lastMillis
    lastMillis = 0;
  }
};
