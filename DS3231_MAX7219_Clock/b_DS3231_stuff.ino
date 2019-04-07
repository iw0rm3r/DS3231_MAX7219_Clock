// Файл с функциями для работы с RTC модулем DS3231 (основными)

void fullMonthName(char *strBuffer, uint8_t monthNum) { // получение полного названия месяца
  strcpy_P(strBuffer, (char *)pgm_read_word(&(fullMonthNames[monthNum - 1])));
}

void shortMonthName(char *strBuffer, uint8_t monthNum) { // получение короткого названия месяца
  uint8_t monthOffset = (monthNum - 1) * 6; // сдвиг символов по номерам месяцев
  for (uint8_t i = 0; i < 6; i++) // копировать 3 символа из строки с названиями
    strBuffer[i] = pgm_read_byte(&(shortMonthNames[i + monthOffset]));
  strBuffer[6] = 0; // добавить терминирующий символ
}

void getDateTime(char *strBuffer, uint8_t format = 6) { // подготовка строки для вывода текущей даты/времени
  /* Формат вывода 22 Mar 2019 17:31:18:
   * 0 - 17:31 с мигаюшим двоеточием (двоеточие/пробел)
   * 1 - 17:31 со статичным двоеточием
   * 2 - 17:31:18
   * 3 - 22.03.2019
   * 4 - 22 March
   * 5 - 22 Mar 2019
   * 6 - 22 Mar 2019 17:31:18 (если аргумент не указан)*/
   
  time_t t = RTC.get(); // получить текущую дату/время
  PString str(strBuffer, 24); // строковый буфер

  if (format >= 3) // день
    str << ( (day(t) < 10) ? "0" : "" ) << day(t);
  if (format == 3) // месяц числом
    str << '.' << ( (month(t) < 10) ? "0" : "" ) << month(t) << '.';
  if (format >= 4) { // месяц прописью
    char monthBuff[17]; // буфер для названия месяца
    if (format == 4) fullMonthName(monthBuff, month(t)); // полностью
    else shortMonthName(monthBuff, month(t)); // сокращённо
    str << ' ' << monthBuff;
    if (format != 4) // пробел между месяцем и годом
       str << ' ';
  }
  if (format >= 3 && format != 4) // год
    str << year(t);
  if (format == 6) // разделитель между датой и временем
    str << ' '; 
  if (format <= 2 || format == 6) { // часы и минуты
    str << ( (hour(t) < 10) ? "0" : "" ) << hour(t);
    if (format == 0) // мигающее двоеточие
      str << ( (second(t) & 1) ? ":" : " " );
    else
      str << ':';
    str << ( (minute(t) < 10) ? "0" : "" ) << minute(t);
  }
  if (format == 2 || format == 6) // секунды
    str << ':' << ( (second(t) < 10) ? "0" : "" ) << second(t);
}

void alarmIsr() { // обработчик прерывания по будильнику
  alarmState = true;
}

void alarmEvent() { // обработчик события будильника
  if (!alarmState) return; // выйти из функции, если будильник не работает
  
  if (alarmLastState == false) { // если ранее будильник не был включён
    RTC.alarm(ALARM_1); // сбросить флаг будильника ALARM_1
    char dateTime[24]; // строковый буфер
    getDateTime(dateTime);
    Serial << F("Сработал будильник ALARM_1: ") << dateTime << endl;
    alarmLastState = true; // поставить флаг предыдущего состояния
  }

  // проверить таймер и выйти из функции, если время сигнализировать еще не пришло
  if ( alarmEventTimer.check() == false ) return;

  ledState = !ledState; // включить/выключить светодиод
  buzzState = !buzzState; // включить/выключить пищалку
  alarmApplySignals(); // применить изменения состояний
}

void alarmApplySignals() { // функция применения состояний светодиода и пищалки будильника
  digitalWrite(LED_PIN, ledState);

  if (buzzState) tone(BUZZ_PIN, 500);
  else noTone(BUZZ_PIN);
}

void alarmOffEvent() { // обработчик события выключения будильника
  // сбросить флаги будильника
  alarmState = false;
  alarmLastState = false;
  // сбросить флаги светодиода и пищалки
  ledState = false;
  buzzState = false;
  // применить изменения состояний
  alarmApplySignals();

  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1); // сброс будильника 1 в RTC
  RTC.alarm(ALARM_1); // сбросить флаг будильника ALARM_1
}

void shiftAlarmTime() { // сдвинуть время будильника на заданное кол-во секунд вперёд
  time_t t = RTC.get(); // получить текущую дату/время
  time_t alarmTime = t + ALARM_INTERVAL; // вычислить следующее время срабатывания
  // поставить будильник
  RTC.setAlarm(ALM1_MATCH_HOURS, second(alarmTime), minute(alarmTime), hour(alarmTime), 0);
}

void switchMode(int8_t mode = -1, bool next = true) { // переключение режима работы
  if (mode != -1) currMode = mode; // при явном указании режима
  else { // без указания режима
    // изменить значение с зацикливанием
    if (next) {
      currMode++;
      if (currMode == MODES_NUM) currMode = 0;
    } else {
      currMode--;
      if (currMode == -1) currMode = MODES_NUM;
    }

    // действия по смене режима работы
    currFont = modeFonts[currMode];

    switch (currMode) { // сброс таймеров - чтобы обновление матрицы происходило сразу
      case 0: // часы
        clockTimer.reSet();
        break;
      case 1: // текущая дата бегущей строкой
        dateTimer.reSet();
        break;
      case 2: // термометр
        thermoTimer.reSet();
        break;
      case 3: // визуализатор спектра по UART
        clearColumns(0, matrixWidth - 1); // очистить матрицу
        break;
    }
  }
}

void clockEvent() { // обработчик события вывода времени
  if (!clockTimer.check()) return;
  
  char dateTime[6]; // строковый буфер
  getDateTime(dateTime, 0); // получить время в формате 0
  setText(dateTime, 3); // вывести на LED матрицу со сдвигом в 3 пикселя от левого края
}

void dateEvent() { // обработчик события вывода даты
  if (!dateTimer.check()) return;

  char newDate[20]; // строковый буфер
  getDateTime(newDate, 4); // формат 4 - "22 March"
  if (strcmp(newDate, scrollBuffer) != 0) { // если дата отличается от буфера
    setScrollText(newDate, false);
  }
}

void thermoEvent() { // обработчик события вывода температуры
  if (!thermoTimer.check()) return;

  char tempStr[7]; // строковый буфер, 7 символов для "25.6°C"
  PString str(tempStr, 7);

  float voltage = analogRead(THERMR_PIN) * 5.0 / 1023.0;
  float r1 = voltage / (5.0 - voltage);
  float temperature = 1./( 1./(4300)*log(r1)+1./(25. + 273.) ) - 273;
  
  str << _FLOAT(temperature, 1) << (char)247 << (char)67; // коды символов °C
  alignText(tempStr);
}

void spectrumEvent() { // обработчик события визуализатора спектра
  // парсим входящий по UART поток байтов от ПК
  while (Serial.available()) { // пока в буфере UART что-то есть
    byte incomingByte = Serial.read();
    if(!commStarted) { // если передача команды ещё не началась...
      if(incomingByte == 'S') { // S - "spectrum"
        // если это начало команды...
        commStarted = true; // поставить флаг
      }
    } else {// если передача команды уже началась...
      if (uartByte < matrixWidth) { // если буфер команды ещё не заполнен...
        uartBuffer[uartByte] = incomingByte; // добавить символ к строке буфера
        uartByte++;
      } else { // если буфер команды заполнен...
        // ...и пришёл завершающий команду символ (новой строки)
        if (incomingByte == '\n' || incomingByte == '\r') {
          for (uint8_t i = 0; i < matrixWidth; i++) { // вывести буфер на LED матрицу
            setColumnM(i, uartBuffer[i]);
          }
        }
        commStarted = false; //снять флаг
        memset(uartBuffer, 0, matrixWidth); //очистить буфер команды
        uartByte = 0;
      }
    }
  }
}

void beep(uint8_t times, uint16_t freq) { // пищание заданное кол-во раз
  for (uint8_t i = 0; i < times; i++) {
    tone(BUZZ_PIN, freq, 100);
    delay(150);
  }
}

void btn1Click () { // обработчик одинарного нажатия на первую кнопку
  // если в данный момент сигнализирует будильник - выключить его
  if (alarmState) alarmOffEvent();
  else { // иначе - переключать режимы работы
    tone(BUZZ_PIN, 500, 100); // пропищать 1 раз
    switchMode();
  }
}

void btn1LPressS () { // обработчик начала длинного нажатия на первую кнопку
  tone(BUZZ_PIN, 1000, 100); // пропищать 1 раз
  shiftAlarmTime(); // переставить будильник на 10 сек вперёд
}

// function to return the compile date and time as a time_t value
time_t compileTime() {
  const time_t FUDGE(10);    //fudge factor to allow for upload time, etc. (seconds, YMMV)
  const char *compDate = __DATE__, *compTime = __TIME__, *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char compMon[3], *m;

  strncpy(compMon, compDate, 3);
  compMon[3] = '\0';
  m = strstr(months, compMon);

  tmElements_t tm;
  tm.Month = ((m - months) / 3 + 1);
  tm.Day = atoi(compDate + 4);
  tm.Year = atoi(compDate + 7) - 1970;
  tm.Hour = atoi(compTime);
  tm.Minute = atoi(compTime + 3);
  tm.Second = atoi(compTime + 6);

  time_t t = makeTime(tm);
  return t + FUDGE;        //add fudge factor to allow for compile time
}
