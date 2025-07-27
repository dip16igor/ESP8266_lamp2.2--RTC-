#include <Arduino.h>
#include <EncButton.h>
#include <Adafruit_NeoPixel.h>
#include <FastBot.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

#define DEBUG
// LED pin definition
// const int ledPin = 2;
#define OffsetTime 5 * 60 * 60 // difference with UTC

#define ENCODER_PIN_A D1
#define ENCODER_PIN_B D2
#define ENCODER_BUTTON D6
#define DC_DC_EN D8
#define PIN D7     // Pin connected to the data input of the NeoPixel strip
#define NUM_LEDS 2 // Number of LEDs in the strip

#define STATE_OFF 0
#define STATE_ON_MANUAL 1
#define STATE_ON_AUTO 2
#define STATE_ON_AUTO_SLOW 3
#define STATE_OFF_AUTO 4

#define MAX_BRIGHT_AUTO_ON_SLOW 120
#define MAX_BRIGHT_AUTO_ON 120

FastBot bot(BOT_TOKEN);
String idAdmin1 = ADMIN_ID;      // ID чата, в который бот пишет лог и слушает команды
#define TelegramDdosTimeout 5000 // таймаут
// unsigned long bot_lasttime; // last time messages' scan has been done
bool Start = false;
const unsigned long BOT_MTBS = 3600; // mean time between scan messages

WiFiClient wclient;

HTTPClient http; // Create Object of HTTPClient
int httpCode;    // код ответа сервера
String payload;  // строка ответ сервера
String error;    // расшифровка кода ошибки
bool restart = 0;
unsigned long uptime;
bool telegram = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

EncButton eb(D1, D2, D6, INPUT_PULLUP, INPUT_PULLUP);

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

const int pwmPin = D4;
// const int pwmPin2 = D0;
const int pwmFrequency = 10000; // PWM frequency in Hz
const int pwmResolution = 8;    // PWM resolution (8 bit)
volatile int encoderPos = 0;
volatile boolean encoderButtonPressed = false;
volatile boolean encoderButtonReleased = false;

int bright = 240;
int bright_old_on_slow;
int bright_old_on;
int bright_old_off;
int bright_temp;
int State = STATE_OFF;

long time_cur;
long time_cur2;

int gammaCorrectedValue;

bool time_trigger = false;

AlarmId id;

bool Online = true;

void setup_wifi(void);
void newMsg(FB_msg &msg);

void PowerON_1(void);
void PowerON_2(void);
void PowerOFF_1(void);
void PowerOFF_2(void);

void digitalClockDisplay(void);
void printDigits(int digits);
void init_telegram(void);

// float gamma = 2.2; // Значение гаммы
// void IRAM_ATTR handleEncoderInterrupt()
// {
//   static byte encoderLastState = 0;
//   byte encoderState = digitalRead(ENCODER_PIN_A) << 1 | digitalRead(ENCODER_PIN_B);

//   if (encoderState != encoderLastState)
//   {
//     if (encoderState == 0b11 && encoderLastState == 0b01)
//     {
//       encoderPos++;
//     }
//     else if (encoderState == 0b01 && encoderLastState == 0b11)
//     {
//       encoderPos--;
//     }
//   }

//   encoderLastState = encoderState;
// }

// void IRAM_ATTR handleEncoderButtonInterrupt()
// {
//   encoderButtonPressed = true;
// }

int gammaCorrection(int value)
{
  float normalizedValue = value / 240.0;                    // Нормализация значения от 0 до 1
  float correctedValue = pow(normalizedValue, 0.6) * 240.0; // Применение гамма-коррекции
  return round(correctedValue);                             // Округление и возврат значения
}

void setup()
{
  pinMode(DC_DC_EN, OUTPUT);
  digitalWrite(DC_DC_EN, LOW);

  pinMode(D6, INPUT_PULLUP);

  if (digitalRead(D6) == 0)
    telegram = true;

  Serial.begin(115200);
  Serial.println();
  Serial.println("Reset");

  strip.begin();
  strip.setBrightness(255);
  strip.setPixelColor(0, strip.Color(20, 0, 0));
  strip.setPixelColor(1, strip.Color(20, 0, 0));
  strip.show();

  // delay(2000);
  // показаны значения по умолчанию
  eb.setBtnLevel(LOW);
  eb.setClickTimeout(500);
  eb.setDebTimeout(50);
  eb.setHoldTimeout(600);
  eb.setStepTimeout(200);

  eb.setEncReverse(0);
  eb.setEncType(EB_STEP4_LOW);
  eb.setFastTimeout(30);

  // сбросить счётчик энкодера
  eb.counter = 0;

  // create the alarms, to trigger at specific times

  setup_wifi();

  if (timeStatus() == 0)
    Serial.println("timeNotSet");
  if (timeStatus() == 1)
    Serial.println("timeNeedsSync");
  if (timeStatus() == 2)
    Serial.println("timeSet");

  if (telegram)
    init_telegram();

  // Alarm.alarmRepeat(6, 00, 00, PowerON_1);   // POWER_ON_1
  // Alarm.alarmRepeat(7, 00, 00, PowerON_2);   // POWER_ON_2
  // Alarm.alarmRepeat(8, 30, 00, PowerOFF_1);  // POWER_OFF_1
  // Alarm.alarmRepeat(22, 05, 00, PowerOFF_2); // POWER_OFF_2

  // setTime(12, 00, 00, 1, 1, 24);            // set time to Saturday 8:29:00am Jan 1 2024
  Alarm.alarmRepeat(5, 30, 00, PowerON_1); // POWER_ON_1
  // Alarm.alarmRepeat(5, 30, 00, PowerON_2);   // POWER_ON_2
  Alarm.alarmRepeat(6, 20, 00, PowerOFF_1);  // POWER_OFF_1
  Alarm.alarmRepeat(22, 00, 00, PowerOFF_2); // POWER_OFF_2

  pinMode(pwmPin, OUTPUT);
  analogWriteFreq(pwmFrequency);
  analogWriteRange(pwmResolution);
  // analogWrite(pwmPin, 190);
  // digitalWrite(DC_DC_EN, HIGH);
}

void loop()
{
  if ((second() % 30 == 0) && (timeStatus() != 2))
  {
    uint32_t color0 = strip.getPixelColor(0);
    // uint32_t color1 = strip.getPixelColor(1);
    strip.setPixelColor(0, strip.Color(0, 0, 0));
    // strip.setPixelColor(1, strip.Color(0, 0, 0));
    strip.show();
    delay(900);
    // strip.setBrightness(0);
    strip.setPixelColor(0, color0);
    // strip.setPixelColor(1, color1);
    strip.show();
  }
#ifdef DEBUG
  if (second() % 10 == 0)
  {
    if (time_trigger == false)
    {
      time_trigger = true;
      // timeClient.update();
      // Serial.println(timeClient.getFormattedTime());
      // Serial.println(timeClient.getEpochTime());

      // Serial.println(timeStatus());

      digitalClockDisplay();
    }
  }
  else
    time_trigger = false;
#endif

  Alarm.service();

  eb.tick();

  if (eb.turn()) // rotating encoder
  {
#ifdef DEBUG
    Serial.print("turn: dir ");
    Serial.print(eb.dir());
    Serial.print(", fast ");
    Serial.print(eb.fast());
    Serial.print(", hold ");
    Serial.print(eb.pressing());
    Serial.print(", counter ");
    Serial.print(eb.counter);
    Serial.print(", clicks ");
    Serial.println(eb.getClicks());
#endif
    if (eb.counter >= 30)
      eb.counter = 30;
    if (eb.counter <= 0)
      eb.counter = 0;

    bright = 240 - eb.counter * 8;
    if (bright <= 0)
      bright = 0;
    if (bright >= 240)
      bright = 240;

    gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции

    if (gammaCorrectedValue >= 232) // reached minimum
    {
      State = STATE_OFF;
      digitalWrite(DC_DC_EN, 0); // DC-DC OFF
      strip.setPixelColor(0, strip.Color(5, 0, 0));
      strip.setPixelColor(1, strip.Color(5, 0, 0));

      // delay (10);
    }
    else if (State == STATE_OFF || State == STATE_OFF_AUTO || State == STATE_ON_AUTO || State == STATE_ON_AUTO_SLOW) // крутим с минимума
    {
      State = STATE_ON_MANUAL;
      analogWrite(pwmPin, 0);
      digitalWrite(DC_DC_EN, 1); // ВКЛ DC-DC
      // delay (1);

      strip.setPixelColor(0, strip.Color(0, 127, 0));
      strip.setPixelColor(1, strip.Color(0, 127, 0));
    }

    if (State == STATE_ON_MANUAL && gammaCorrectedValue < 9) // крутим от максимума вниз
    {
      strip.setPixelColor(0, strip.Color(0, 127, 0));
      strip.setPixelColor(1, strip.Color(0, 127, 0));
    }

    if (State == STATE_ON_MANUAL && gammaCorrectedValue < 2) // докрутили до максимума
    {
      strip.setPixelColor(0, strip.Color(127, 0, 127));
      strip.setPixelColor(1, strip.Color(127, 0, 127));
    }

    // strip.setBrightness(map(eb.counter, 0, 240, 50, 255)); // изменяем яркость индикаторов
    strip.show();

    if (State == STATE_OFF)
      analogWrite(pwmPin, 255);
    else
    {
      analogWrite(pwmPin, gammaCorrectedValue);
      Serial.println(gammaCorrectedValue);
    }
  }

  // кнопка
  if (eb.press())
  {
#ifdef DEBUG
    Serial.println("press");
#endif
  }
  if (eb.click())
  {
#ifdef DEBUG
    Serial.println("click");
#endif
    if (State == STATE_ON_AUTO || State == STATE_ON_AUTO_SLOW || State == STATE_ON_MANUAL)
    {
      State = STATE_OFF_AUTO; // start automatic shutdown
      strip.setPixelColor(0, strip.Color(127, 0, 0));
      strip.setPixelColor(1, strip.Color(127, 0, 0));
      strip.show();

      time_cur = millis();
      bright_temp = bright;
    }
    else
    {
      State = STATE_ON_AUTO;
      analogWrite(pwmPin, 0);
      digitalWrite(DC_DC_EN, 1); // ВКЛ DC-DC

      strip.setPixelColor(0, strip.Color(0, 100, 100));
      strip.setPixelColor(1, strip.Color(0, 100, 100));
      strip.show();

      time_cur2 = millis();
      bright_temp = bright;
    }

    //
  }
  if (eb.release())
  {
#ifdef DEBUG
    Serial.println("release");

    Serial.print("clicks: ");
    Serial.print(eb.getClicks());
    Serial.print(", steps: ");
    Serial.print(eb.getSteps());
    Serial.print(", press for: ");
    Serial.print(eb.pressFor());
    Serial.print(", hold for: ");
    Serial.print(eb.holdFor());
    Serial.print(", step for: ");
    Serial.println(eb.stepFor());
#endif
  }

  if (State == STATE_OFF_AUTO) //---------------------------------------------------------- AUTOMATIC SHUTDOWN ----------------------------
  {
    bright = bright_temp + (millis() - time_cur) / 50;

    if (bright <= 0)
      bright = 0;
    if (bright >= 240)
      bright = 240;

    gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции

    if (gammaCorrectedValue != bright_old_off)
    {
      bright_old_off = gammaCorrectedValue;
      analogWrite(pwmPin, gammaCorrectedValue);
      eb.counter = (240 - bright) / 8;
      Serial.println(gammaCorrectedValue);
    }
    // strip.setBrightness(map(eb.counter, 0, 240, 50, 255)); // изменяем яркость индикаторов

    if (gammaCorrectedValue >= 239)
    {
      State = STATE_OFF;
      digitalWrite(DC_DC_EN, 0); // ВЫКЛ DC-DC
      strip.setPixelColor(0, strip.Color(5, 0, 0));
      strip.setPixelColor(1, strip.Color(5, 0, 0));
      strip.show();
      analogWrite(pwmPin, 255);

      Serial.println("STATE_OFF");
    }
  }

  if (State == STATE_ON_AUTO) //---------------------------------------------------------- АВТОМАТИЧЕСКОЕ ВКЛЮЧЕНИЕ ----------------------------
  {
    bright = bright_temp - (millis() - time_cur2) / 40;

    if (bright <= 0)
      bright = 0;
    if (bright >= 240)
      bright = 240;

    gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции

    if (gammaCorrectedValue != bright_old_on)
    {
      bright_old_on = gammaCorrectedValue;
      analogWrite(pwmPin, gammaCorrectedValue);
      eb.counter = (240 - bright) / 8;
      Serial.println(gammaCorrectedValue);
    }
    // strip.setBrightness(map(eb.counter, 0, 240, 50, 255)); // изменяем яркость индикаторов

    if (gammaCorrectedValue <= MAX_BRIGHT_AUTO_ON) // яркость поднялась до середины 120
    {
      State = STATE_ON_MANUAL;
      strip.setPixelColor(0, strip.Color(0, 127, 0));
      strip.setPixelColor(1, strip.Color(0, 127, 0));
      strip.show();

      Serial.println("STATE_ON_MANUAL");
    }
  }

  if (State == STATE_ON_AUTO_SLOW) //---------------------------------------------------------- МЕДЛЕННОЕ АВТОМАТИЧЕСКОЕ ВКЛЮЧЕНИЕ (РАССВЕТ) ----------------------------
  {
    bright = bright_temp - (millis() - time_cur2) / 4000;

    if (bright <= 0)
      bright = 0;
    if (bright >= 240)
      bright = 240;

    gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции

    if (gammaCorrectedValue != bright_old_on_slow)
    {
      bright_old_on_slow = gammaCorrectedValue;
      analogWrite(pwmPin, gammaCorrectedValue);
      eb.counter = (240 - bright) / 8;
      Serial.println(gammaCorrectedValue);
    }
    // strip.setBrightness(map(eb.counter, 0, 240, 50, 255)); // изменяем яркость индикаторов

    if (gammaCorrectedValue <= MAX_BRIGHT_AUTO_ON_SLOW) // яркость поднялась до середины
    {
      State = STATE_ON_MANUAL;
      analogWrite(pwmPin, gammaCorrectedValue);
      strip.setPixelColor(0, strip.Color(0, 127, 0));
      strip.setPixelColor(1, strip.Color(0, 127, 0));
      strip.show();

      Serial.println("STATE_ON_MANUAL");
    }
  }

  if (telegram)
  {
    bot.tick(); // тикаем в луп
    if (restart)
    {
      Serial.println("Restart!");
      bot.tickManual();
      ESP.restart();
    }
  }
  // strip.show();
  // delay(10);
}

// functions to be called when an alarm triggers:
void PowerON_1()
{
  Serial.println("Alarm: - turn lights ON 1");
  // if (bright <= 200)
  //{
  State = STATE_ON_AUTO_SLOW;
  analogWrite(pwmPin, 0);
  digitalWrite(DC_DC_EN, 1); // ВКЛ DC-DC
  delay(1);
  gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции
  analogWrite(pwmPin, gammaCorrectedValue);
  eb.counter = (240 - bright) / 8;
  // delayMicroseconds(200);
  // analogWrite(pwmPin, 240);
  time_cur2 = millis();
  bright_temp = bright;
  strip.setPixelColor(0, strip.Color(0, 10, 10));
  strip.setPixelColor(1, strip.Color(0, 10, 10));
  strip.show();
  //}
}

void PowerON_2()
{
  Serial.println("Alarm: - turn lights ON 2");
  State = STATE_ON_AUTO_SLOW;
  analogWrite(pwmPin, 0);
  digitalWrite(DC_DC_EN, 1); // ВКЛ DC-DC
  delay(1);
  gammaCorrectedValue = gammaCorrection(bright); // Применение гамма-коррекции
  analogWrite(pwmPin, gammaCorrectedValue);
  eb.counter = (240 - bright) / 8;
  // delay(1);
  // analogWrite(pwmPin, 240);
  time_cur2 = millis();
  bright_temp = bright;
  strip.setPixelColor(0, strip.Color(0, 30, 30));
  strip.setPixelColor(1, strip.Color(0, 30, 30));
  strip.show();
}

void PowerOFF_1()
{
  Serial.println("Alarm: - turn lights OFF 1");
  State = STATE_OFF_AUTO;

  strip.setPixelColor(0, strip.Color(127, 0, 0));
  strip.setPixelColor(1, strip.Color(127, 0, 0));
  strip.show();

  time_cur = millis();
  bright_temp = bright;
}

void PowerOFF_2()
{
  Serial.println("Alarm: - turn lights OFF 2");
  State = STATE_OFF_AUTO;

  strip.setPixelColor(0, strip.Color(127, 0, 0));
  strip.setPixelColor(1, strip.Color(127, 0, 0));
  strip.show();

  time_cur = millis();
  bright_temp = bright;
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.println();
}

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// обработчик сообщений
void newMsg(FB_msg &msg)
{
  String chat_id = msg.chatID;
  String text = msg.text;
  String firstName = msg.first_name;
  String lastName = msg.last_name;
  String userID = msg.userID;
  String userName = msg.username;

  bot.notify(false);

  // выводим всю информацию о сообщении
  Serial.println(msg.toString());
  FB_Time t(msg.unix, 5);
  Serial.print(t.timeString());
  Serial.print(' ');
  Serial.println(t.dateString());

  if (chat_id == idAdmin1)
  {
    if (text == "/restart" && msg.chatID == idAdmin1)
      restart = 1;

    // обновление прошивки по воздуху из чата Telegram
    if (msg.OTA && msg.chatID == idAdmin1)
    {
      strip.setPixelColor(0, strip.Color(100, 0, 100));
      strip.setPixelColor(1, strip.Color(100, 0, 100));
      strip.show();
      bot.update();
      strip.setPixelColor(0, strip.Color(0, 100, 0));
      strip.setPixelColor(1, strip.Color(0, 100, 0));
      strip.show();
      delay(300);
    }
    if (text == "/ping")
    {
      strip.setPixelColor(0, strip.Color(100, 0, 100));
      strip.show();
      Serial.println("/ping");
      uptime = millis(); // + 259200000;
      int min = uptime / 1000 / 60;
      int hours = min / 60;
      int days = hours / 24;

      long rssi = WiFi.RSSI();
      int gasLevelPercent = 0; // map(gasLevel, GasLevelOffset, 1024, 0, 100);

      String time = "pong! Сообщение принято в ";
      time += t.timeString();
      time += ". Uptime: ";
      time += days;
      time += "d ";
      time += hours - days * 24;
      time += "h ";
      // time += min - days * 24 * 60 - (hours - days * 24) * 60;
      time += min - hours * 60;
      time += "m";

      time += ". RSSI: ";
      time += rssi;
      time += " dB";

      time += ". Gas: ";
      time += gasLevelPercent;
      time += " %";

      bot.sendMessage(time, chat_id);
      Serial.println("Message is sended");

      strip.setPixelColor(7, strip.Color(1, 0, 2));
      strip.show();
    }
    if (text == "/start" || text == "/start@dip16_GasSensor_bot")
    {
      strip.setPixelColor(7, strip.Color(100, 100, 0));
      strip.show();
      bot.showMenuText("Команды:", "\ping \t \restart", chat_id, false);
      delay(300);
      strip.setPixelColor(7, strip.Color(0, 0, 0));
      strip.show();
    }
  }
}

void setup_wifi()
{
  int counter_WiFi = 0;
  // delay(10);
  //  We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // secured_client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(400);
    Serial.print(".");
    // OnLEDBLUE2();
    strip.setPixelColor(0, strip.Color(0, 0, 127));
    strip.setPixelColor(1, strip.Color(0, 0, 127));
    strip.show(); // Update the LED strip
    delay(400);
    // OffLEDBLUE2();
    strip.setPixelColor(0, strip.Color(0, 0, 0));
    strip.setPixelColor(1, strip.Color(0, 0, 0));
    strip.show(); // Update the LED strip
    counter_WiFi++;
    if (counter_WiFi >= 20)
    {
      Online = false;
      break;
    }
  }

  if (Online == false)
  {
    Serial.println();
    Serial.println("ERROR! Unable connect to WiFi");
    strip.setPixelColor(0, strip.Color(200, 0, 0));
    strip.setPixelColor(1, strip.Color(200, 0, 0));
    strip.show(); // Update the LED strip

    setTime(12, 00, 00, 1, 1, 24); // set time to 12:00

    delay(2000);
    WiFi.mode(WIFI_OFF);
  }
  else // Online
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    timeClient.setTimeOffset(OffsetTime);
    timeClient.update();

    setTime(timeClient.getEpochTime());

    if (timeStatus() == 2)
    {
      strip.setPixelColor(0, strip.Color(0, 200, 0));
      strip.setPixelColor(1, strip.Color(0, 200, 0));
      strip.show(); // Update the LED strip
      delay(2000);
    }
    else
    {
      Serial.println();
      Serial.println("ERROR! Unable set Time!");
      strip.setPixelColor(0, strip.Color(100, 100, 0));
      strip.setPixelColor(1, strip.Color(100, 100, 0));
      strip.show();                  // Update the LED strip
      setTime(12, 00, 00, 1, 1, 24); // set time to Saturday 8:29:00am Jan 1 2024
    }

    strip.setPixelColor(0, strip.Color(5, 0, 0));
    strip.setPixelColor(1, strip.Color(5, 0, 0));
    strip.show();

    if (!telegram)
      WiFi.mode(WIFI_OFF);
  }
}

void init_telegram(void)
{
  bot.setChatID("");  // передай "" (пустую строку) чтобы отключить проверку
  bot.skipUpdates();  // пропустить непрочитанные сообщения
  bot.attach(newMsg); // обработчик новых сообщений

  bot.sendMessage("Lamp1 Restart OK", idAdmin1);
  Serial.println("Message to telegram was sended");
  strip.setPixelColor(0, strip.Color(127, 0, 127));
  strip.setPixelColor(1, strip.Color(127, 0, 127));

  bot.sendMessage(timeClient.getFormattedTime(), idAdmin1);
}

void OffLEDBLUE(void) // Turn off LED1 COM
{                     // turn the LED off (HIGH is the voltage level)
  digitalWrite(LED_BUILTIN, HIGH);
} // write 1 to blue LED port
void OnLEDBLUE(void) // Turn on LED1
{                    // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN, LOW);
} // запись 0 в порт синего светодиода

void OffLEDBLUE2(void) // Turn off LED2 WAKE
{                      // turn the LED off (HIGH is the voltage level)
  digitalWrite(LED_BUILTIN_AUX, HIGH);
} // write 1 to blue LED port
void OnLEDBLUE2(void) // Turn on LED2
{                     // turn the LED on by making the voltage LOW
  digitalWrite(LED_BUILTIN_AUX, LOW);
} // запись 0 в порт синего светодиода