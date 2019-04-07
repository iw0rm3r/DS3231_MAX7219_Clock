// --------------------БИБЛИОТЕКИ--------------------
// Arduino DS3232RTC Library by Jack Christensen
// https://github.com/JChristensen/DS3232RTC
// --------------------------------------------------
// OneButton library created by Matthias Hertel
// http://www.mathertel.de/Arduino/OneButtonLibrary.aspx
// --------------------------------------------------
// Faster_LedControl library by Vincent Fischer
// https://github.com/C0br4/Faster_LedControl

#include <DS3232RTC.h>        // работа с DS3231 RTC
#include <Streaming.h>        // вывод множества строк по UART одной строкой
#include <PString.h>          // печать в C-strings, вместо использования sptintf/String
#include "iwTimer.cpp"        // класс таймера на основе millis()
#include "OneButton.h"        // работа с тактовыми кнопками
#include <avr/sleep.h>        // работа со спящим режимом МК
#include "LedControl.h"       // управление MAX7219 (ускоренная версия с HW SPI)
#include "5bite_rus_forum.h"  // файл шрифта с русскими буквами
#include "7segment_fixed.h"   // файл шрифта с символами часов

// --------------------НАСТРОЙКИ--------------------
#define M7219_NUM 4           // число модулей MAX7219
#define MAX_BRIGHT 3          // максимальное значение яркости
#define SCROLL_DELAY 50       // задержка при прокрутке текста, мс
#define MODES_NUM 4           // число режимов работы

#define RTC_INT_PIN 2         // пин приёма прерывания от будильника RTC
#define LED_PIN A2            // пин светодиода
#define BUZZ_PIN 7            // пин пищалки
#define BTN1_PIN A0           // пин кнопки
#define PHOTOR_PIN A1         // пин фоторезистора
#define THERMR_PIN A3         // пин терморезистора
#define CS_PIN 10             // пин chip select для LED матрицы

// --------------------typedef'ы и struct'ы--------------------
typedef uint8_t fontArray[6]; // тип указателей на массивы шрифтов

struct font { // структура-описание шрифта
  fontArray    *fArray;       // указатель на массив шрифта в PROGMEM
  uint8_t       charOffset;   // сдвиг первого символа
  uint8_t       charTotal;    // число символов в шрифте
};

// --------------------ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ--------------------
const uint8_t modeFonts[] = {1, 0, 0, 0};   // номера шрифтов для разных режимов
const font aFonts[] = { // массив шрифтов
  {_5bite_rus,        0,    255},           // нулевой шрифт - основной, универсальный
  {_7segment_fixed,   32,   27}             // шрифт часов (цифры и знаки)
};
const char monthStr1[] PROGMEM = "Января"; const char monthStr2[] PROGMEM = "Февраля";
const char monthStr3[] PROGMEM = "Марта"; const char monthStr4[] PROGMEM = "Апреля";
const char monthStr5[] PROGMEM = "Мая"; const char monthStr6[] PROGMEM = "Июня";
const char monthStr7[] PROGMEM = "Июля"; const char monthStr8[] PROGMEM = "Августа";
const char monthStr9[] PROGMEM = "Сентября"; const char monthStr10[] PROGMEM = "Октября";
const char monthStr11[] PROGMEM = "Ноября"; const char monthStr12[] PROGMEM = "Декабря";
const PROGMEM char * const PROGMEM fullMonthNames[] = { // полные названия месяцев на русском
    monthStr1,monthStr2,monthStr3,monthStr4,monthStr5,monthStr6,monthStr7,monthStr8,monthStr9,monthStr10,monthStr11,monthStr12};
const PROGMEM char shortMonthNames[] = // краткие названия месяцев на русском
  {"ЯнвФевМарАпрМайИюнИюлАвгСенОктНояДек"};

const uint8_t firstModule = M7219_NUM - 1;  // адрес первого модуля MAX7219 по порядку отображения
const uint8_t matrixWidth = M7219_NUM * 8;  // ширина матрицы в пикселях
volatile bool alarmState = false;           // флаг будильника
bool alarmLastState = false;                // флаг последнего состояния будильника
bool ledState = false;                      // флаг светодиода будильника
bool buzzState = false;                     // флаг пищалки будильника
bool scrollState = false;                   // флаг прокрутки текста
bool scrollOnce;                            // флаг однократной прокрутки
bool matrixState = false;                   // флаг состояния матрицы (вкл/выкл)
bool commStarted = false;                   // флаг начала передачи команды по UART
int8_t currMode = 0;                        // номер текущего режима работы (0 - часы)
uint8_t currFont = 1;                       // номер текущего шрифта
uint8_t brightness = 0;                     // яркость LED матрицы
uint8_t uartByte = 0;                       // номер текущего байта в буфере UART
uint16_t scrollTxtWidth;                    // ширина текста бегущей строки
int scrollOffset;                           // текущий сдвиг прокрутки текста
char scrollBuffer[256];                     // буфер бегущей строки
byte uartBuffer[matrixWidth];               // буфер данных UART

iwTimer alarmEventTimer(500);               // таймер события будильника, задержка в 500 мс
iwTimer clockTimer(1000);                   // таймер обновления часов
iwTimer dateTimer(1000);                    // таймер обновления даты
iwTimer thermoTimer(5000);                  // таймер обновления термометра
iwTimer scrollTimer(SCROLL_DELAY);          // таймер задержки прокрутки текста
iwTimer brightAdjTimer(1000);               // таймер регулировки яркости матрицы
OneButton button1(BTN1_PIN, true);          // кнопка смены режимов/управления будильником
LedControl matrix(CS_PIN, M7219_NUM);       // LED матрица

const time_t ALARM_INTERVAL(10);            // сдвиг будильника в сек (ВРЕМЕННО)

// в этом .ino-файле должна быть хотя бы одна функция, иначе IDE не компилирует проект
void sleepMode() { // ввод контроллера в режим сна
  ADCSRA = 0; // выключить АЦП
  set_sleep_mode (SLEEP_MODE_PWR_DOWN); // задать режим максимально глубокого сна
  sleep_enable(); // включить спящий режим
  sleep_cpu (); // перевести в спящий режим
}
