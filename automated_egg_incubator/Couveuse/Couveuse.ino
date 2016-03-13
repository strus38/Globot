// Code source pour Couveuse automatique
// @author: mis@2016
//
// Logs:
//
// Projects used:
// ==============
// - DHT Lib: https://github.com/markruys/arduino-DHT
// - 1-Wire: http://forum.pjrc.com/threads/23939-Strange-behavior-on-the-Onewireslave-library?p=33608&viewfull=1#post33608
// - 1-Wire: https://github.com/PaulStoffregen/OneWire
// - DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
// - PID: http://playground.arduino.cc/Code/PIDLibraryRelayOutputExample
// - RELAY: https://github.com/John-NY/Arduino_Oven_Controller_PID/blob/master/tc_relay_control.pde
// - TIMER: http://www.doctormonk.com/2012/01/arduino-timer-library.html
//
//#include <SPI.h>
#include <Wire.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEVMODE 0
#define OLEDMODE 1

#if defined(OLEDMODE)
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
#include "ssd1306_i2c.h"
#include "font.h"
#include "Couveuse.h"

#define SDA 4
#define SCL 5
#define I2C 0x3C
SSD1306 display(I2C, SDA, SCL);
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
void drawFrame1(int x, int y);
void (*frameCallbacks[1])(int x, int y) = {drawFrame1};
// how many frames are there?
int frameCount = 1;
// on frame is currently displayed
int currentFrame = 0;
#endif

#define DS18B20_PIN_1       8  // 1-wire bus
#define DHT11_PIN           10 // DHT sensor
#define PID_RELAY_PIN       7 // Channel 2
#define ROLLER_RELAY_PIN    6 // Channel 1                                                                                                                   6 // Channel 1
#define PushPin   2 // Button
#define PotPin    A0 // Potentiometer

DHT dht;
OneWire  oneWire(DS18B20_PIN_1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

//#if defined(OLEDMODE)
// ************************************************
// Screen init
// ************************************************
//#define OLED_RESET 4
//Adafruit_SSD1306 oled(OLED_RESET);

//#define NUMFLAKES 10
//#define XPOS 0
//#define YPOS 1
//#define DELTAY 2

//#if (SSD1306_LCDHEIGHT != 64)
//#error("Height incorrect, please fix Adafruit_SSD1306.h!");
//#endif

//#endif

int SetPoint = 3780;       // 37.8 degree as target temp by default
int RollTime = 9000;       // 8 seconds
const int RollEvery = 180; // turn every 3 hours

double Minutes;
unsigned long StartTime;
boolean TimerOn = false;
boolean change = false;

volatile long onTime = 0;
short int i = 0;

enum operatingState { OFF = 0, SETT, SETR, RUN };
operatingState opState = OFF;

// Button handling
int buttonState;
int lastButtonState = HIGH;
unsigned long lastButtonTime = 0;
unsigned long longPressTime = 500;
boolean shortButtonPressed = false;
boolean longButtonPressed = false;

int lastRelayState = LOW;
float Temp=-1;
float Hum=-1;
int c = 0;

#define DAYS 1814400000

void setup()
{
  /* Initialize the RELAY to be OFF by default! DO NOT CHANGE - SECURITY FIRST */
  digitalWrite(PID_RELAY_PIN, LOW);
  digitalWrite(ROLLER_RELAY_PIN, LOW);


  //Serial.begin(9600);

  pinMode(PushPin, INPUT_PULLUP);

#if defined(OLEDMODE)
  display.init();
  display.flipScreenVertically();
  // set the drawing functions
  display.setFrameCallbacks(1, frameCallbacks);
  // how many ticks does a slide of frame take?
  display.setFrameTransitionTicks(10);

  display.clear();
  display.display();

  delay(500);
#endif

  dht.setup(DHT11_PIN);

  sensors.begin();
  c = sensors.getDeviceCount();
  
#if defined(DEVMODE)
  Serial.print(c, DEC);
  Serial.println(F(" devices."));
#endif

  if (!sensors.getAddress(insideThermometer, 0)) {
#if defined(DEVMODE)
    Serial.println(F("Unable to find address for Device 0"));
#endif
  }
  sensors.setResolution(insideThermometer, 12);

  if (!sensors.getAddress(insideThermometer, 1)) {
#if defined(DEVMODE)
    Serial.println(F("Unable to find address for Device 1"));
#endif
  }
  sensors.setResolution(insideThermometer, 12);

  pinMode(PID_RELAY_PIN, OUTPUT);
  pinMode(ROLLER_RELAY_PIN, OUTPUT);

  // Initialize the PID and related variables
  // LoadParameters();
  // Initialize LCD Display
#if defined(DEVMODE)
  Serial.print(F("Loading SetPoint: "));
  Serial.print(SetPoint);
  Serial.print(F(" RollTime: "));
  Serial.print(RollTime);
  Serial.println(F(" ... READY"));
#endif

  // Run Timer2 interrupt every 15 ms
  TCCR2A = 0;
  TCCR2B = 1 << CS22 | 1 << CS21 | 1 << CS20;

  // Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1 << TOIE2;
}

// ************************************************
// Timer Interrupt Handler
// ************************************************
SIGNAL(TIMER2_OVF_vect)
{
  if (opState == OFF)
  {
    digitalWrite(PID_RELAY_PIN, LOW);  // make sure relay is off
    digitalWrite(ROLLER_RELAY_PIN, LOW);  // make sure relay is off
  }
  else
  {
    DriveOutput();
  }
}


void loop()
{
#if defined(OLEDMODE)
  display.clear();
  display.nextFrameTick();
  display.display();
#endif
  resetbutton();

  switch (opState)
  {
    case OFF:
      Off();
      break;
    case SETT:
      SetT();
      break;
    case SETR:
      SetR();
      break;
    case RUN:
      Run();
      break;
  }
}

// capture button presses
void button() {
  buttonState = digitalRead(PushPin);
  if (buttonState != lastButtonState) {
    if (buttonState == HIGH) {
      if (millis() - lastButtonTime < longPressTime) {
        shortButtonPressed = true;
      }
      else {
        longButtonPressed = true;
      }
    }
    lastButtonState = buttonState;
    lastButtonTime = millis();
  }
}

void resetbutton() {
  shortButtonPressed = false;
  longButtonPressed = false;
}

void drawFrame1(int x, int y) {
  display.setFontScale2x2(false);
  display.drawString(1 + x, 1 + y, String(c));
  if (lastRelayState == HIGH)
    display.drawString(22 + x, 1 + y, F("HEAT"));
  
  switch (opState)
  {
    case OFF:
      display.drawString(55 + x, 8 + y, F("OFF"));
      break;
    case SETT:
      display.drawString(55 + x, 8 + y, F("SETT Mode"));
      break;
    case SETR:
      display.drawString(55 + x, 8 + y, F("SETR Mode"));
      break;
    case RUN:
      display.drawString(55 + x, 8 + y, F("RUN Mode"));
      break;
  }
  if (TimerOn)
    display.drawString(100 + x, 1 + y, F("ON"));
    
  display.drawXbm(x + 7, y + 7, temperature_width, temperature_height, temperature_bits);
  display.setFontScale2x2(true);
  if (Temp == -1)
    display.drawString(34 + x, 20 + y, "Error");
  else
    display.drawString(34 + x, 20 + y, String(Temp) + "C");

  display.setFontScale2x2(false);
  if (Hum ==-1)
    display.drawString(44 + x, 40 + y, "Error");
  else
    display.drawString(44 + x, 40 + y, String(Hum) + "H");
  
  display.drawString(1 + x, 56 + y, String((DAYS - millis())/(24 * 3600000)) + "j");
}


// ************************************************
// Initial State - long or short button to enter setpoint
// ************************************************
void Off()
{
  digitalWrite(PID_RELAY_PIN, LOW);  // make sure it is off
  digitalWrite(ROLLER_RELAY_PIN, LOW);  // make sure relay is off
  // Print status: OFF
#if defined(DEVMODE)
  Serial.println(F("Turning system OFF"));
#endif

  while (!(shortButtonPressed || longButtonPressed))
  {
    Temp = getTemperature();
    Hum = getHumidity();
    // Display Temp... and other info if needed
    display.clear();
    display.nextFrameTick();
    display.display();
  
    button();
  }
  StartTime = millis();
  opState = RUN; // start control
}

void SetT()
{
#if defined(DEVMODE)
  // Display: Set Temperature
  Serial.println(F("Setting Temperature"));
#endif

  while (true)
  {
    button();
    if (change) {
      SetPoint = map(analogRead(PotPin), 0, 1023, 15, 99);
      // Print #
    };
    if (longButtonPressed)
    {
      change = !change;
      return;
    }
    if (shortButtonPressed)
    {
      opState = SETR;
      change = false;
      return;
    }

    if (!change & (millis() - lastButtonTime) > 3000)  // return to RUN after 3 seconds idle
    {
      opState = RUN;
      change = false;
      return;
    }
    // Print SetPoint
  }
}

void SetR()
{
#if defined(DEVMODE)
  // Display: Set Roll time
  Serial.println(F("Set Rolling time"));
#else

#endif
  while (true)
  {
    button();
    if (change) {
      RollTime = map(analogRead(PotPin), 0, 1023, 15, 99);
      // Print #
    };
    if (longButtonPressed)
    {
      change = !change;
      return;
    }
    if (shortButtonPressed)
    {
      opState = RUN;
      change = false;
      return;
    }

    if (!change & (millis() - lastButtonTime) > 3000)  // return to RUN after 3 seconds idle
    {
      opState = RUN;
      change = false;
      return;
    }
    // Print SetPoint
  }
}

// ************************************************
// Long for Roll
// Short - off
// ************************************************
void Run()
{
#if defined(DEVMODE)
  Serial.println(F("System RUN"));
#endif

  while (true)
  {
    button();
    if (shortButtonPressed)
    {
      opState = SETT;
      return;
    }
    if (longButtonPressed)
    {
      opState = OFF;
      return;
    }

    display.clear();
    display.nextFrameTick();
    display.display();

    DoControl();

    if ((millis() - StartTime) / 60000 >= RollEvery) {
      TimerOn = true;
      if (TimerOn) {
        StartTime = millis();
      }
#if defined(DEVMODE)
      Serial.println(F("Timer started"));
#endif
    }
    if (TimerOn) {

      digitalWrite(ROLLER_RELAY_PIN, HIGH);
      if ((millis() - StartTime) >= RollTime)
      {
#if defined(DEVMODE)
        Serial.println(F("Timer stopped"));
#endif
        opState = RUN;
        TimerOn = false;
        return;
      }
    } else {
      digitalWrite(ROLLER_RELAY_PIN, LOW);
    }

    delay(100);
  }
}

void DoControl()
{
  // Read the input:
  Hum = getHumidity();
  Temp = getTemperature();

  // Time Proportional relay state is updated regularly via Timer interrupt.
  onTime = SetPoint - (Temp * 100);
#if defined(DEVMODE)
  Serial.print(F("Hum: "));
  Serial.print(Hum);
  Serial.print(F(" Temp: "));
  Serial.print(Temp);
  Serial.print(F(" onTime: "));
  Serial.println(onTime);
#endif

}


float getTemperature()
{
  sensors.requestTemperatures();
  float tempC1 = sensors.getTempCByIndex(0);
  float tempC2 = sensors.getTempCByIndex(1);
  if (tempC1 < 0) tempC1 = tempC2;
  if (tempC2 < 0) tempC2 = tempC1;
  
  return abs((tempC1 + tempC2) / 2);
}

float getHumidity()
{
  delay(dht.getMinimumSamplingPeriod());
  float humidity = dht.getHumidity();
  float temperature = dht.getTemperature();
  if (humidity <= 0)
    return -1;
  return humidity;
}


void DriveOutput()
{
  if (onTime > 20)
  {
    if (lastRelayState == LOW) {
      i++;
      if (i < 20)
        return;
    }
    i = 0;
    lastRelayState = HIGH;
    digitalWrite(PID_RELAY_PIN, HIGH);
  }
  else
  {
    if (lastRelayState == HIGH) {
      i++;
      if (i < 20)
        return;
    }
    i = 0;
    lastRelayState = LOW;
    digitalWrite(PID_RELAY_PIN, LOW);
  }
}

