#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "ThingSpeak.h"


const int TdsPin       = 34;
const int TurbidityPin = 35;
const int TempPin      = 4;
const int SD_CS        = 5;

OneWire oneWire(TempPin);
DallasTemperature sensors(&oneWire);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);


const float Vref = 3.3;
const int   ADCResolution = 4095;
float kValueTDS = 1.0;

bool sdOK = false;

WiFiClient client;
unsigned long myChannelNumber = 3165744;
const char* myApiKey = "1IAORBLS1W56D5DX";


float ultimoTDS  = 0;
float ultimoNTU  = 0;
float ultimaTemp = NAN;

unsigned long lastSerialLCD  = 0;
unsigned long lastThingSpeak = 0;

const unsigned long intervalSerialLCD  = 1000;   
const unsigned long intervalThingSpeak = 15000;  


void printFixedInt(LiquidCrystal_I2C &d, int value, uint8_t width) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", value);
  d.print(buf);
  uint8_t len = strlen(buf);
  while (len++ < width) d.print(' ');
}

void printFixedFloat1(LiquidCrystal_I2C &d, float value, uint8_t width) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", value);
  d.print(buf);
  uint8_t len = strlen(buf);
  while (len++ < width) d.print(' ');
}

void printFixedStr(LiquidCrystal_I2C &d, const char* s, uint8_t width) {
  d.print(s);
  uint8_t len = strlen(s);
  while (len++ < width) d.print(' ');
}

void setup() {
  Serial.begin(9600);   // Solo para debug / MATLAB. No es obligatorio para que el resto funcione.

  Wire.begin(21, 22);
  sensors.begin();

  if (!rtc.begin()) {
    if (Serial) Serial.println("No se encuentra el RTC");
  } else {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // comenta esta línea después de la primera programación
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Hydrosys...");
  delay(1000);
  lcd.clear();

  lcd.setCursor(0,0); lcd.print("PPM:");
  lcd.setCursor(8,0); lcd.print("NTU:");
  lcd.setCursor(0,1);
  lcd.print("T");
  lcd.write(223);
  lcd.print("C");

  if (!SD.begin(SD_CS)) {
    if (Serial) Serial.println("Fallo SD");
  } else {
    sdOK = true;
    File f = SD.open("/datos.csv", FILE_APPEND);
    if (f && f.size() == 0) {
      f.println("Fecha,Hora,ppm,NTU,TempC");
    }
    if (f) f.close();
  }

  // WiFi mediante portal de WiFiManager
  WiFiManager wifiManager;
  wifiManager.autoConnect("Hydrosys");

  ThingSpeak.begin(client);
}

void leerSensores() {

  sensors.requestTemperatures();
  ultimaTemp = sensors.getTempCByIndex(0);
  if (ultimaTemp == DEVICE_DISCONNECTED_C) ultimaTemp = NAN;
  float tempForTDS = isnan(ultimaTemp) ? 25.0 : ultimaTemp;

  
  int adcTDS = analogRead(TdsPin);
  float voltajeTDS = adcTDS * Vref / ADCResolution;
  float comp = 1.0 + 0.02 * (tempForTDS - 25.0);
  float voltCompTDS = voltajeTDS / comp;
  ultimoTDS = (314.99 * pow(voltCompTDS,2) + 226.89 * voltCompTDS + 257.56);
  if (ultimoTDS < 0) ultimoTDS = 0;

 
  int adcTur = analogRead(TurbidityPin);
  float voltTur = adcTur * Vref / ADCResolution;
  float voltInv = Vref - voltTur;
  ultimoNTU = 435.53 * pow(voltInv,2) - 3517.3 * voltInv + 7105.4;
  if (ultimoNTU < 0) ultimoNTU = 0;
}

void actualizarSerial() {
  if (!Serial) return;   

  DateTime now = rtc.now();

  Serial.print("Fecha: ");
  Serial.print(now.day());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.year());
  Serial.print(" Hora: ");
  if (now.hour() < 10)   Serial.print('0');
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print('0');
  Serial.print(now.minute());
  Serial.print(":");
  if (now.second() < 10) Serial.print('0');
  Serial.println(now.second());

  Serial.print("NTU: ");
  Serial.println((int)ultimoNTU);

  Serial.print("PPM: ");
  Serial.println((int)ultimoTDS);

  Serial.print("Temperatura: ");
  if (isnan(ultimaTemp)) Serial.println("NaN");
  else                   Serial.println((int)ultimaTemp);

  Serial.println();
}

void actualizarLCD() {
  lcd.setCursor(4, 0);
  printFixedInt(lcd, (int)ultimoTDS, 4);

  lcd.setCursor(12, 0);
  printFixedInt(lcd, (int)ultimoNTU, 4);

  lcd.setCursor(3, 1);
  if (isnan(ultimaTemp)) printFixedStr(lcd, "--.-", 4);
  else                   printFixedFloat1(lcd, ultimaTemp, 4);
}

void guardarEnSD() {
  if (!sdOK) return;

  DateTime now = rtc.now();

  File f = SD.open("/datos.csv", FILE_APPEND);
  if (f) {
    f.print("Fecha:");
    f.print(now.day()); f.print("/");
    f.print(now.month()); f.print("/");
    f.print(now.year());
    f.print(",");

    f.print("Hora:");
    f.print(now.hour()); f.print(":");
    f.print(now.minute());
    f.print(",");

    f.print("PPM:");  f.print(ultimoTDS, 2);  f.print(",");
    f.print("NTU:");  f.print(ultimoNTU, 2);  f.print(",");
    f.print("TempC:");
    if (isnan(ultimaTemp)) f.println("NaN");
    else                   f.println(ultimaTemp, 2);

    f.close();
  }
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSerialLCD >= intervalSerialLCD) {
    lastSerialLCD = currentMillis;

    leerSensores();
    actualizarLCD();
    guardarEnSD();
    actualizarSerial();
  }

 
  if (currentMillis - lastThingSpeak >= intervalThingSpeak) {
    lastThingSpeak = currentMillis;

    ThingSpeak.setField(1, ultimoNTU);  
    ThingSpeak.setField(2, ultimoTDS);   
    ThingSpeak.setField(3, ultimaTemp);  

    int status = ThingSpeak.writeFields(myChannelNumber, myApiKey);

    if (Serial) {
      if (status == 200) {
        Serial.println("Datos enviados a ThingSpeak");
      } else {
        Serial.println("Error enviando a ThingSpeak: " + String(status));
      }
      Serial.println("--------");
    }
  }
}
