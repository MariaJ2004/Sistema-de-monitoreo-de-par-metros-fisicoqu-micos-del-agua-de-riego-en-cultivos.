#include <Adafruit_I2CDevice.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include "RTClib.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2  // Temperatura -> D2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define SSpin 10
#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <math.h>

uint16_t adcTurbidez, adcTDS;
float voltajeTurbidez, voltajeTDS;
float NTU, PPM;
float tempC = NAN;

volatile bool flagADC = false;

const int chipSelect = 10; // Módulo SD (SPI -> D10 a D13)
LiquidCrystal_I2C lcd(0x27, 16, 2);
File dataFile;
RTC_DS1307 rtc; // RTC DS1307 (I2C) -> SDA A4 y SCL A5

void setup() {
  Serial.begin(9600);

  DDRC &= ~((1 << PC1) | (1 << PC0)); // Turbidez->A0; TDS->A1

  // Configuración del ADC
  ADMUX  = (1 << REFS0);
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

  // interrupción Temporizador (Timer1)
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (1 << CS10);
  OCR1A = 15624;
  TIMSK1 |= (1 << OCIE1A);
  interrupts();

  // Inicializar I2C, LCD, RTC, DS18B20 y SD
  Wire.begin();
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.print("Iniciando...");
  _delay_ms(800);
  lcd.clear();

  if (!rtc.begin()) {
    Serial.println("No se encuentra el modulo RTC DS1307");
    while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  sensors.begin();
  sensors.setResolution(10);
  if (sensors.getDeviceCount() == 0) {
    Serial.println(F("No se detecto ningun DS18B20."));
  }

  if (SD.begin(chipSelect)) {
    dataFile = SD.open("DATOS.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Tiempo(HH:MM -DD:MM:AA);Turbidez(NTU);TDS(PPM);Temperatura");
      dataFile.close();
      Serial.println("Tarjeta SD inicializada.");
    } else {
      Serial.println("Error al abrir DATOS.csv");
    }
  } else {
    Serial.println("Error al inicializar la tarjeta SD.");
  }

  flagADC = true;
}

void loop() {
  if (flagADC) {
    flagADC = false;

    // Leer sensor de turbidez y calcular NTU
    ADMUX = (ADMUX & 0b11110000) | 0;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    adcTurbidez = ADC;
    voltajeTurbidez = (adcTurbidez * 5.0f) / 1023.0f;
    NTU = (435.53f * powf(voltajeTurbidez, 2)) - (3517.3f * voltajeTurbidez) + 7105.4f;

    // Leer sensor de TDS y calcular PPM
    ADMUX = (ADMUX & 0b11110000) | 1;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    adcTDS = ADC;
    voltajeTDS = (adcTDS * 5.0f) / 1023.0f;
    PPM = (314.99f * powf(voltajeTDS, 2)) + (226.89f * voltajeTDS) + 257.56f;

    // Leer sensor de temperatura y calcula °C
    sensors.requestTemperatures();
    float tC = sensors.getTempCByIndex(0);
    if (tC != DEVICE_DISCONNECTED_C) tempC = tC;

    // Mostrar valores LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NTU: ");
    lcd.print((int)NTU);
    lcd.setCursor(0, 1);
    lcd.print("PPM: ");
    lcd.print((int)PPM);
    lcd.print(" T: ");
    if (isnan(tempC)) {
      lcd.print("--");
    } else {
      lcd.print((int)tempC);
    }
    lcd.print((char)223); 
    lcd.print("C");

    // Guardar datos en la SD (fecha, hora, NTU, PPM, °C)
    DateTime now = rtc.now();
    dataFile = SD.open("DATOS.csv", FILE_WRITE);
    if (dataFile) {
      if (now.hour() < 10) dataFile.print('0');
      dataFile.print(now.hour());   dataFile.print(':');
      if (now.minute() < 10) dataFile.print('0');
      dataFile.print(now.minute()); dataFile.print(" -");
      if (now.day() < 10) dataFile.print('0');
      dataFile.print(now.day());    dataFile.print(':');
      if (now.month() < 10) dataFile.print('0');
      dataFile.print(now.month());  dataFile.print(':');
      dataFile.print(now.year() % 100);
      dataFile.print(";");

      dataFile.print(NTU, 2); dataFile.print(";");
      dataFile.print(PPM, 2); dataFile.print(";");
      if (isnan(tempC)) dataFile.println("NaN");
      else dataFile.println(tempC, 2);
      dataFile.close();
    } else {
      Serial.println("No se pudo escribir en el archivo.");
    }

    // Mostrar valores en el monitor serial
    Serial.print("Fecha: ");
    Serial.print(now.day()); Serial.print('/');
    Serial.print(now.month()); Serial.print('/');
    Serial.print(now.year());
    Serial.print("  Hora: ");
    if (now.hour() < 10) Serial.print('0');
    Serial.print(now.hour()); Serial.print(':');
    if (now.minute() < 10) Serial.print('0');
    Serial.print(now.minute()); Serial.print(':');
    if (now.second() < 10) Serial.print('0');
    Serial.println(now.second());

    Serial.print("NTU: "); Serial.println((int)NTU);
    Serial.print("PPM: "); Serial.println((int)PPM);
    if (isnan(tempC)) {
      Serial.println(F("Temperatura: NaN"));
    } else {
      Serial.print(F("Temperatura: "));
      Serial.println((int)tempC);
    }
  }
}

// Interrupción del temporizador
ISR(TIMER1_COMPA_vect) {
  flagADC = true; // Activa la bandera de actualización
}//retorna interrupción
