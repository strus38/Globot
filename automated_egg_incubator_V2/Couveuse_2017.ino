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
// - PID: https://github.com/milesburton/Arduino-Temperature-Control-Library
// - PID: http://playground.arduino.cc/Code/PIDLibraryRelayOutputExample
// - RELAY: https://github.com/John-NY/Arduino_Oven_Controller_PID/blob/master/tc_relay_control.pde
// - 
//

#include <Wire.h>
//#include <DHT.h>
#include <dht_nonblocking.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// So we can save and retrieve settings
#include <EEPROM.h>

#ifndef DEVMODE
#define DEVMODE 0
#endif

#ifndef OLEDMODE
#define OLEDMODE 1
#endif

#if defined(OLEDMODE)
    #include "ssd1306_i2c.h"
    #include "font.h"
    #include "Couveuse.h"

    #define SDA 20
    #define SCL 21
    #define I2C 0x3C
    SSD1306 display(I2C, SDA, SCL);
    // this array keeps function pointers to all frames
    // frames are the single views that slide from right to left
    void drawFrame1(int x, int y);
    void (*frameCallbacks[1])(int x, int y) = {drawFrame1};
    // how many frames are there?
    int frameCount = 1;
    // one frame is currently displayed
    int currentFrame = 0;
#endif

#define DS18B20_PIN_1       8  // 1-wire bus
#define DHT11_PIN           10 // DHT sensor
#define HEAT_RELAY_PIN       6 // Channel 2
#define ROLLER_RELAY_PIN    7 // Channel 1
#define PushPin   2 // Button
#define PotPin    A0 // Potentiometer

#define DHT_SENSOR_TYPE DHT_TYPE_11
DHT_nonblocking dht_sensor( DHT11_PIN, DHT_SENSOR_TYPE );
//DHT dht;
OneWire  oneWire(DS18B20_PIN_1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

const int EclAddress = 0;
int SetPoint = 3780;       // 37.8 degree as target temp by default
int RollTime = 5000;       // 8 seconds
const int RollEvery = 180;    //180; // turn every 3 hours

double Minutes;
unsigned long StartTime;
boolean TimerOn = false;
boolean change = false;

volatile long onTime = 0;
unsigned long Now;

enum operatingState { OFF = 0, RUN };
operatingState opState = OFF;

// Button handling
long buttonTimer = 0;
long longPressTime = 800;
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers


bool buttonActive = false;
bool longButtonPressed = false;
bool shortButtonPressed = false;

#define RELAY_ON HIGH
#define RELAY_OFF LOW
bool lastHeatRelayState = RELAY_OFF;
float Temp=-1;
float Humidity =-1;
short int c = 0;
int led=LOW;
double Ecl;

#define DAYS 1814400000 // 21 days

void setup()
{
  /* Initialize the RELAY to be OFF by default! DO NOT CHANGE - SECURITY FIRST */
   // Configure the Relays
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);
  
  pinMode(ROLLER_RELAY_PIN, OUTPUT);
  digitalWrite(ROLLER_RELAY_PIN, HIGH);

#if defined(DEVMODE)
  Serial.begin(9600);
#endif

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
#endif

  // Init the DHT sensor
  //dht.setup(DHT11_PIN);
  
  // Init the DS18B20 sensors
  sensors.begin();
  // Count the sensors (2 in the design currently since the Temp of the DHT11 is ignored)
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
  sensors.setResolution(insideThermometer, 11);

  if (!sensors.getAddress(insideThermometer, 1)) {
#if defined(DEVMODE)
    Serial.println(F("Unable to find address for Device 1"));
#endif
  }
  sensors.setResolution(insideThermometer, 11);

  LoadParameters();

  // Run timer2 interrupt every 15 ms 
  TCCR2A = 0;
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1<<TOIE2;

  //delay(2000);
}

SIGNAL(TIMER2_OVF_vect) 
{
  if (opState == OFF)
  {
    digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);  // make sure it is off
    digitalWrite(ROLLER_RELAY_PIN, HIGH);  // make sure relay is off
    lastHeatRelayState = RELAY_OFF;
  }
  else
  {
    DriveOutput();
  }
}

void loop()
{
  resetbutton();

  switch (opState)
  {
    case OFF:
      Off();
      break;
    case RUN:
      Run();
      break;
  }
  
}

// capture button presses
void button() {

  int reading = digitalRead(PushPin);
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        buttonTimer = millis();
      } else {
        if ((millis() - buttonTimer > longPressTime) && (longButtonPressed == false))
          longButtonPressed = true;
        else if ((millis() - buttonTimer) < longPressTime)
          shortButtonPressed = true;
      }
    }
  }
  lastButtonState = reading;
}

void resetbutton() {
  longButtonPressed = false;
  shortButtonPressed = false;
}

void drawFrame1(int x, int y) {
  display.setFontScale2x2(false);
  display.drawString(1 + x, 1 + y, String(c));
  if (lastHeatRelayState == RELAY_ON)
    display.drawString(22 + x, 1 + y, F("HEAT"));

  switch (opState)
  {
    case OFF:
      display.drawString(55 + x, 8 + y, F("OFF"));
      break;
    case RUN:
      display.drawString(55 + x, 8 + y, F("RUN Mode"));
      break;
  }
  if (TimerOn)
    display.drawString(100 + x, 1 + y, F("Roll"));
  else {
    display.drawString(100 + x, 1 + y, String(RollEvery - ((Now - StartTime) / 60000)));
  }

  display.drawXbm(x + 7, y + 7, temperature_width, temperature_height, temperature_bits);
  display.setFontScale2x2(true);
  if (Temp == -1)
    display.drawString(34 + x, 20 + y, F("Error"));
  else
    display.drawString(34 + x, 20 + y, String(Temp) + "C");

  display.setFontScale2x2(false);
  if (Humidity ==-1)
    display.drawString(44 + x, 40 + y, F("Error"));
  else
    display.drawString(44 + x, 40 + y, String(Humidity) + "H");

  display.drawString(1 + x, 56 + y, String(((DAYS - Now)/(24 * 3600000))+1) + "j");
  display.drawString(70 + x, 56 + y, String(onTime));
}


// ************************************************
// Initial State - long or short button to enter setpoint
// ************************************************
void Off()
{
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);  // make sure it is off
  digitalWrite(ROLLER_RELAY_PIN, HIGH);  // make sure relay is off
  lastHeatRelayState = RELAY_OFF;
  delay(1000);
  
  StartTime = millis();
  opState = RUN; // start control
}

// ************************************************
// Long for Roll
// Short - off
// ************************************************
void Run()
{

  while (true)
  {
    button();
    if (shortButtonPressed || longButtonPressed)
    {
      TimerOn = !TimerOn;
      StartTime = millis();
      opState = RUN;
      return;
    }

    Now = millis();
    DoControl();

    if ((Now - StartTime) / 60000 >= RollEvery) {
      TimerOn = true;
      if (TimerOn) {
        StartTime = Now;
      }
#if defined(DEVMODE)
      Serial.println(F("Timer started"));
#endif
    }
    if (TimerOn) {

      digitalWrite(ROLLER_RELAY_PIN, LOW);
      if ((Now - StartTime) >= RollTime)
      {
#if defined(DEVMODE)
        Serial.println(F("Timer stopped"));
#endif
        opState = RUN;
        TimerOn = false;
        SaveParameters();
        return;
      }
    } else {
      digitalWrite(ROLLER_RELAY_PIN, HIGH);
    }
    delay(15);
  }
}

void DoControl()
{
  float temperature;
  if( measure_environment( &temperature, &Humidity ) == true ) {
    temperature = getTemperature();
    if (temperature ==127) 
      Temp = -1;
    else 
      Temp = temperature;
  }
  if (Temp != -1)
    onTime = (SetPoint - (Temp * 100));

#if defined(OLEDMODE)
  display.clear();
  display.nextFrameTick();
  display.display();
#endif
#if defined(DEVMODE)
  Serial.print(F("Hum: "));
  Serial.print(Humidity);
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
  
  if (abs(tempC1) == 127)
      if (abs(tempC2) != 127)
        return tempC2;
  if (abs(tempC2) == 127)
      if (abs(tempC1) != 127)
        return tempC1;
  
  return abs((tempC1 + tempC2) / 2);
}

void DriveOutput()
{
  led = !led;
  digitalWrite(13,led);

  if (onTime > 20)
  {
    if (lastHeatRelayState == RELAY_OFF) {
      lastHeatRelayState = RELAY_ON;
      digitalWrite(HEAT_RELAY_PIN, RELAY_ON);
    }
  }
  else
  {
    if (lastHeatRelayState == RELAY_ON) {
      lastHeatRelayState = RELAY_OFF;
      digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);
    }
  }
}

static bool measure_environment( float *temperature, float *humidity )
{
  static unsigned long measurement_timestamp = millis( );

  /* Measure once every four seconds. */
  if( millis( ) - measurement_timestamp > 1400ul )
  {
    if( dht_sensor.measure( temperature, humidity ) == true )
    {
      measurement_timestamp = millis( );
      return( true );
    }
  }

  return( false );
}

// ************************************************
// Save any parameter changes to EEPROM
// ************************************************
void SaveParameters()
{
   if (Ecl != EEPROM_readDouble(EclAddress))
   {
      EEPROM_writeDouble(EclAddress, Ecl);
   }
}

// ************************************************
// Load parameters from EEPROM
// ************************************************
void LoadParameters()
{
  // Load from EEPROM
   Ecl = EEPROM_readDouble(EclAddress);
   
   if (isnan(Ecl))
   {
     Ecl = ((DAYS - Now)/(24 * 3600000))+1;
   }  
}


// ************************************************
// Write floating point values to EEPROM
// ************************************************
void EEPROM_writeDouble(int address, double value)
{
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      EEPROM.write(address++, *p++);
   }
}

// ************************************************
// Read floating point values from EEPROM
// ************************************************
double EEPROM_readDouble(int address)
{
   double value = 0.0;
   byte* p = (byte*)(void*)&value;
   for (int i = 0; i < sizeof(value); i++)
   {
      *p++ = EEPROM.read(address++);
   }
   return value;
}
