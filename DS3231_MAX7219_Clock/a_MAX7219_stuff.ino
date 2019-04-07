// Файл со вспомогательными функциями для управления LED матрицей через библиотеку Faster_LedControl

void matrixShutdown(bool state) { // задать режим всей LED матрицы
  for (uint8_t i = firstModule; i < 255; i--) matrix.shutdown(i, state);
}

void matrixIntensity(uint8_t intensity) { // задать яркость всей LED матрицы
  for (uint8_t i = firstModule; i < 255; i--) matrix.setIntensity(i, intensity);
}

void matrixClear() { // очистить всю LED матрицу
  for (uint8_t i = firstModule; i < 255; i--) matrix.clearDisplay(i);
}

void ajustBrightness() { // регулировка яркости по фоторезистору
  if (!brightAdjTimer.check()) return; // выйти из функции, если время не пришло
  
  uint16_t light = analogRead(PHOTOR_PIN); // начальное значение от 1023 до 0
  if (light >= 900) { // выключение при малом освещении
    if (matrixState) {
      matrixState = false;
      matrixShutdown(true);
    }
  }
  else {
    if (!matrixState) {
      matrixState = true;
      matrixShutdown(false);
    }
    light = constrain(light, 100, 300); // ограничиваем минимальное и максимальное значения
    uint8_t newBrightness = map(light, 300, 100, 0, MAX_BRIGHT); // конвертируем в яркость
    if (newBrightness != brightness) { // если яркость отличается от текущей - задать
      brightness = newBrightness;
      matrixIntensity(brightness);
    }
  }
}

byte getFontByte(uint8_t charNum, uint8_t byteNum) { // возвращает байт из текущего шрифта
   uint8_t corrCharNum = charNum - aFonts[currFont].charOffset; // сдвинуть номер символа
   // проверка на выход за пределы набора символов шрифта
   if (corrCharNum > aFonts[currFont].charTotal) return 0;
   
   byte aByte = pgm_read_byte_near( &( aFonts[currFont].fArray[corrCharNum][byteNum] ) );
   return aByte;
}

void setText (char text[], int xOffset = 0) { // вывод текста на матрицу
  // очищаем колонки до текста
  if (xOffset > 0) clearColumns(0, xOffset - 1);
  
  int xCoord = xOffset; // счётчик колонок, задаётся со сдвигом по оси X
  uint16_t textLength = strlen(text);
  // перебор символов текста
  for (uint16_t charNum = 0; charNum < textLength; charNum++) {
    uint8_t aChar = text[charNum]; // взять символ из текста
    uint8_t charWidth = getFontByte(aChar, 0); // найти ширину символа
    if (charWidth == 0) continue; // пропуск символов нулевой ширины
    // вывести все байты символа последовательно
    for (uint8_t byteNum = 1; byteNum <= charWidth; byteNum++) {
      if (xCoord >= 0 && xCoord <= matrixWidth) { // проверка на выход за пределы матрицы
        byte aByte = getFontByte(aChar, byteNum); // найти байт для столбца
        setColumnM(xCoord, aByte);
      }
      xCoord++; // переход на след. байт символа
    }
    // добавить пробел между символами
    setColumnM(xCoord, 0);
    xCoord++;
  }
  
  // очищаем колонки после текста
  if (xCoord < matrixWidth) clearColumns(xCoord, matrixWidth - 1);
}

void clearColumns(uint8_t aColumn, uint8_t lastColumn) { // очистка колонкок матрицы
  for (aColumn; aColumn <= lastColumn; aColumn++)
    setColumnM(aColumn, 0);
}

uint16_t textWidth (char text[]) { // функция определения ширины текста в пикселях
  uint16_t totalWidth = 0;
  uint16_t textLength = strlen(text);
  
  // перебор символов текста
  for (uint16_t charNum = 0; charNum < textLength; charNum++) {
    uint8_t aChar = text[charNum]; // взять символ из текста
    uint8_t charWidth = getFontByte(aChar, 0); // найти ширину символа
    if (charWidth == 0) continue; // пропуск символов нулевой ширины
    totalWidth += charWidth + 1; // добавить ширину символа и пробела
  }

  return totalWidth - 1; // вернуть ширину без последнего пробела
}

void setScrollText(char *text, bool once) {
  strcpy(scrollBuffer, text);
  scrollState = true;
  scrollOnce = once;
  scrollOffset = matrixWidth + 1;
  scrollTxtWidth = textWidth(text);
}

void scrollText() { // вывод буфера на матрицу бегущей строкой
  if (!scrollState) return; // выйти из функции, если прокрутка не включена
  if (!scrollTimer.check()) return; // выйти из функции, если время крутить ещё не пришло
  
  if ( scrollOffset == (matrixWidth + 1) ) { // если это первая итерация прокрутки:
    clearColumns(0, matrixWidth - 1); // очистить матрицу
    scrollOffset--;
  } else if (scrollOffset != -scrollTxtWidth) { // пока не достигнута последняя колонка
    setText(scrollBuffer, scrollOffset);
    scrollOffset--;
  } else { // если достигнута последняя колонка
    clearColumns(0, matrixWidth - 1); // очистить матрицу
    if (scrollOnce) scrollState = false;
    else scrollOffset = matrixWidth + 1;
  }
}

void alignText(char text[], bool center = true) { // 1 - по центру, 0 - по правому краю
  uint8_t tWidth = textWidth(text);

  if (tWidth > matrixWidth) setText(text); // если текст шире матрицы
  else {
    uint8_t xOffset;
    
    if (center) xOffset = (matrixWidth - tWidth) / 2;
    else xOffset = matrixWidth - tWidth;
    setText(text, xOffset);
  }
}

byte bitswap (byte x) { // функция переворачивания byte на asm'е
  byte result;
 
    asm("mov __tmp_reg__, %[in] \n\t"
      "lsl __tmp_reg__  \n\t"   /* shift out high bit to carry */
      "ror %[out] \n\t"  /* rotate carry __tmp_reg__to low bit (eventually) */
      "lsl __tmp_reg__  \n\t"   /* 2 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 3 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 4 */
      "ror %[out] \n\t"
 
      "lsl __tmp_reg__  \n\t"   /* 5 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 6 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 7 */
      "ror %[out] \n\t"
      "lsl __tmp_reg__  \n\t"   /* 8 */
      "ror %[out] \n\t"
      : [out] "=r" (result) : [in] "r" (x));
      return(result);
}

void setColumnM (uint8_t xCoord, byte aByte) { // вывод столбца на матрицу (а не на модуль)
  uint8_t module, column;
  aByte = bitswap(aByte); // переворачиваем байт
  module = xCoord / 8; // находим номер модуля MAX7219
  module = firstModule - module; // конвертируем его в адрес
  column = xCoord % 8; // находим номер столбца на указаном модуле
  matrix.setColumn(module, column, aByte); // выводим байт на матрицу
}
