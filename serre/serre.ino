#include <DallasTemperature.h>
#include <Wire.h>

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
#define HEAT_RELAY_PIN  6 // Channel 2
#define DHT_SENSOR_TYPE DHT_TYPE_11
#define RELAY_ON        HIGH
#define RELAY_OFF       LOW
#define DELTA_T         20

DHT_nonblocking dht_sensor( DHT11_PIN, DHT_SENSOR_TYPE );
OneWire  oneWire(DS18B20_PIN_1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

const int TargetTemperature = 2500;       // 25.0 degree as target temp by default

volatile long t_onTime = 0;
unsigned long t_now;
bool          b_heat_relaystate = RELAY_OFF;
short int     si_nb_sensors = 0;
float         f_temp=-1;
float         f_humi =-1;
int           i_led=LOW;

void setup()
{
  /* Initialize the RELAY to be OFF by default! DO NOT CHANGE - SECURITY FIRST */
   // Configure the Relays
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);

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

  // Init the DS18B20 sensors
  sensors.begin();
  // Count the sensors (2 in the design currently since the f_temp of the DHT11 is ignored)
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

  if (!sensors.getAddress(insideThermometer, 1)) {
#if defined(DEVMODE)
    Serial.println(F("Unable to find address for Device 1"));
#endif
  }
  sensors.setResolution(insideThermometer, 11);

  // breath ... there is no hurry
  delay(1000);
}

void loop()
{
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);  // make sure it is off
  b_heat_relaystate = RELAY_OFF;

  run_control();
  delay(2000);
}

void drawScreen(int x, int y) {
  
  display.setFontScale2x2(false);
  
  display.drawString(1 + x, 1 + y, String(si_nb_sensors));
  
  if (b_heat_relaystate == RELAY_ON)
    display.drawString(22 + x, 1 + y, F("HEAT"));

  display.drawString(55 + x, 8 + y, F("RUN Mode"));
  
  display.drawXbm(x + 7, y + 7, temperature_width, temperature_height, temperature_bits);
  display.setFontScale2x2(true);
  if (f_temp == -1)
    display.drawString(34 + x, 20 + y, F("Error"));
  else
    display.drawString(34 + x, 20 + y, String(f_temp) + "C");

  display.setFontScale2x2(false);
  if (f_humi ==-1)
    display.drawString(44 + x, 40 + y, F("Error"));
  else
    display.drawString(44 + x, 40 + y, String(f_humi) + "H");

  display.drawString(70 + x, 56 + y, String(t_onTime));

}

void run_control()
{
  float temperature;
  int diff = 0;
  
  if( measure_environment( &temperature, &f_humi ) == true ) {
    temperature = getf_temperature();
    if (temperature ==127) 
      f_temp = -1;
    else 
      f_temp = temperature;
  }
  if (f_temp != -1) {
    diff = TargetTemperature - (f_temp * 100);
    check_relay_state(diff); 
  }

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
  Serial.println(diff);
#endif

}


float getf_temperature()
{
  sensors.requestTemperatures();
  float tempC1 = sensors.requestTemperaturesByIndex(0);
  float tempC2 = sensors.requestTemperaturesByIndex(1);
  
  if (abs(tempC1) == 127)
      if (abs(tempC2) != 127)
        return tempC2;
  if (abs(tempC2) == 127)
      if (abs(tempC1) != 127)
        return tempC1;
  
  return abs((tempC1 + tempC2) / 2);
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
