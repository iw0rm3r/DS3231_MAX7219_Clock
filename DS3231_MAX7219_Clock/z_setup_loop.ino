void setup() {
  // задать режимы работы пинов
  pinMode(RTC_INT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  // подключить внешнее прерывание INT0 для будильника
  attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), alarmIsr, FALLING);

  // связать нажатие на кнопки с вызовами обработчиков
  button1.attachClick(btn1Click);
  button1.attachLongPressStart(btn1LPressS);

  Serial.begin(115200);

  // проверка корректности даты/времени в DS3231
  if ( RTC.oscStopped(false) ) { // проверить, не остановлен ли осциллятор RTC
    Serial.println(F("Похоже, дата/время в памяти DS3231 некорректны. Сбрасываем до времени компиляции..."));
    beep(2, 1000); // пропищать 2 раза

    RTC.set(compileTime()); // установить дату/время компиляции прошивки
    RTC.oscStopped(true); // сбросить флаг остановки осцилятора
  }

  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, true); // включить вызов прерывания для будильника ALARM_1
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);

  // синхронизировать время ардуины с модулем RTC
  setSyncProvider(RTC.get);
  if (timeStatus() != timeSet) {
    Serial.println(F("Не удалось синхронизировать время с модулем RTC!"));
    beep(3, 1000); // пропищать 3 раза
    digitalWrite(LED_PIN, HIGH); // включить светодиод
    sleepMode(); // уйти в спячку
  } else {
    Serial.println(F("Модуль RTC успешно задал системное время."));
    tone(BUZZ_PIN, 500, 100); // пропищать 1 раз
  }

  // инициализация LED матрицы
  ajustBrightness();
  matrixClear();

  Serial.println(F("Выполнение функции setup() завершено, начинаем выполнение loop()..."));
}

void loop() {
  // обновления состояний
  ajustBrightness(); // регулировать яркость LED матрицы
  button1.tick(); // проверить состояние кнопки 1

  // резидентные события
  alarmEvent(); // будильник

  // обработчики режимов работы
  switch (currMode) {
    case 0: // часы
      clockEvent();
      break;
    case 1: // текущая дата бегущей строкой
      dateEvent(); // вывести дату на LED матрицу
      scrollText(); // прокручивать текст
      break;
    case 2: // термометр
      thermoEvent();
      break;
    case 3: // визуализатор спектра по UART
      spectrumEvent();
      break;
  }
}
