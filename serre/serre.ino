#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <StringSplitter.h>

#include "DS3231.h"
#include "dht_nonblocking.h"

#ifndef DEVMODE
#define DEVMODE 0
#endif

#ifndef OLEDMODE
#define OLEDMODE 1
#endif

#if defined(OLEDMODE)
    #include "ssd1306_i2c.h"
    #include "font.h"
    #include "serre.h"

    #define SDA 20
    #define SCL 21
    #define I2C 0x3C
    SSD1306 display(I2C, SDA, SCL);
    void drawScreen(int x, int y);
    void (*frameCallbacks[1])(int x, int y) = {drawScreen};
    int frameCount = 1;
    int currentFrame = 0;
#endif

#define DS18B20_PIN_1   8  // 1-wire bus
#define DHT11_PIN       10 // DHT sensor
#define HEAT_RELAY_PIN  6 // Channel 1
#define LIGHT_RELAY_PIN 7 // Channel 2
#define DHT_SENSOR_TYPE DHT_TYPE_11
#define RELAY_ON        HIGH
#define RELAY_OFF       LOW
#define DELTA_T         20

DS3231  rtc(SDA, SCL);
DHT_nonblocking dht_sensor( DHT11_PIN, DHT_SENSOR_TYPE );
OneWire  oneWire(DS18B20_PIN_1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

const int TargetTemperature = 2500;       // 25.0 degree as target temp by default

unsigned long t_now;
bool          b_heat_relaystate = RELAY_OFF;
bool          b_light_relaystate = RELAY_OFF;
short int     si_nb_sensors = 0;
float         f_temp=-1;
float         f_temp2=-1;
float         f_humi =-1;
int           i_led=LOW;
int           i_diff = 0;
String        str_now="";

void setup()
{
  /* Initialize the RELAY to be OFF by default! DO NOT CHANGE - SECURITY FIRST */
   // Configure the Relays
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  digitalWrite(LIGHT_RELAY_PIN, RELAY_OFF);

#if defined(DEVMODE)
  Serial.begin(9600);
#endif
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

  rtc.begin(); // Initialize the rtc object
  //rtc.setDOW(SUNDAY);     // Set Day-of-Week to SUNDAY
  //rtc.setTime(14, 0, 0);     // Set the time to 12:00:00 (24hr format)
  //rtc.setDate(20, 1, 2019);   // Set the date to January 1st, 2014
  // Init the DS18B20 sensors
  sensors.begin();
  // Count the sensors (1 in the design currently since the f_temp of the DHT11 is ignored)
  si_nb_sensors = sensors.getDeviceCount();

#if defined(DEVMODE)
  Serial.print(si_nb_sensors, DEC);
  Serial.println(F(" devices."));
#endif

  if (!sensors.getAddress(insideThermometer, 0)) {
#if defined(DEVMODE)
    Serial.println(F("Unable to find address for Device 0"));
#endif
  }
  sensors.setResolution(insideThermometer, 11);

  // breath ... there is no hurry
  delay(1000);
}

void loop()
{
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);  // make sure it is off
  digitalWrite(LIGHT_RELAY_PIN, RELAY_OFF);
  b_heat_relaystate = RELAY_OFF;
  b_light_relaystate = RELAY_OFF;
  while (true) {
    run_control();
    delay(30);
  }
}

void drawScreen(int x, int y) {
  
  display.setFontScale2x2(false);
  
  display.drawString(1 + x, 1 + y, String(si_nb_sensors));
  
  display.drawString(30 + x, 1 + y,rtc.getDateStr());
  display.drawString(30 + x, 10 + y, str_now);
  
  display.drawXbm(x + 6, y + 7, temperature_width, temperature_height, temperature_bits);
  display.setFontScale2x2(true);
  if (f_temp == -1)
    display.drawString(34 + x, 22 + y, F("Error"));
  else
    display.drawString(34 + x, 22 + y, String(f_temp) + "C");

  display.setFontScale2x2(false);
  if (f_humi ==-1)
    display.drawString(44 + x, 42 + y, F("Error"));
  else
    display.drawString(44 + x, 42 + y, String(f_humi) + "H");

  display.drawString(10 + x, 56 + y, String(f_temp2));
  display.drawString(70 + x, 56 + y, String(i_diff));
  if (b_heat_relaystate == RELAY_ON)
    display.drawString(100 + x, 56 + y, F("HEAT"));

}

static bool measure_environment( float *temperature, float *humidity )
{
  static unsigned long measurement_timestamp = millis( );

  /* Measure once every four seconds. */
  if( millis( ) - measurement_timestamp > 4000ul )
  {
    if( dht_sensor.measure( temperature, humidity ) == true )
    {
      measurement_timestamp = millis( );
      return( true );
    }
  }

  return( false );
}


void run_control()
{
  if (measure_environment( &f_temp, &f_humi ) == true) {
    i_diff = abs(TargetTemperature - (f_temp * 100));
    check_relay_state(i_diff); 
  }

  static unsigned long measurement_timestamp2 = millis( );
  /* Measure once every four seconds. */
  if( millis( ) - measurement_timestamp2 > 2000ul )
  {
    sensors.requestTemperatures();
    f_temp2 = sensors.getTempC(insideThermometer);
    if(f_temp2 == DEVICE_DISCONNECTED_C) 
    {
      f_temp2 = -1;
      return;
    }
    measurement_timestamp2 = millis( );
  }

  str_now = rtc.getTimeStr();
  check_light_relay(str_now);

#if defined(OLEDMODE)
  display.clear();
  display.nextFrameTick();
  display.display();
#endif
#if defined(DEVMODE)
  Serial.print(F("Hum: "));
  Serial.print(f_humi);
  Serial.print(F(" f_temp: "));
  Serial.print(f_temp);
  Serial.print(F(" d_temp: "));
  Serial.println(i_diff);
#endif

}

void check_relay_state(int delta)
{
  i_led = !i_led;
  digitalWrite(13,i_led);

  if (delta > DELTA_T)
  {
    if (b_heat_relaystate == RELAY_OFF) {
      b_heat_relaystate = RELAY_ON;
      digitalWrite(HEAT_RELAY_PIN, RELAY_ON);
    }
  }
  else
  {
    if (b_heat_relaystate == RELAY_ON) {
      b_heat_relaystate = RELAY_OFF;
      digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);
    }
  }
}

void check_light_relay(String strTime) {
  
  StringSplitter *splitter = new StringSplitter(strTime, ':', 2);
  String str_hour = splitter->getItemAtIndex(0);
  int hour = str_hour.toInt();
  if (hour >= 7 && hour <= 22) {
    if (b_light_relaystate == RELAY_OFF) {
      b_light_relaystate = RELAY_ON;
      digitalWrite(LIGHT_RELAY_PIN, RELAY_ON);
    }
  } else {
    if (b_heat_relaystate == RELAY_ON) {
      b_light_relaystate = RELAY_OFF;
      digitalWrite(LIGHT_RELAY_PIN, RELAY_OFF);
    }
  }
}
