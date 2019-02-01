#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//#include "MemoryFree.h"
#include "DS3231.h"

#ifndef DEVMODE
#define DEVMODE 0
#endif

#ifndef OLEDMODE
#define OLEDMODE 1
#endif


#if defined(OLEDMODE)
    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>

    //#include "font.h"
    //#include "serre.h"

    #define SDA 20
    #define SCL 21
    #define I2C 0x3C
    #define SCREEN_WIDTH 128 // OLED display width, in pixels
    #define SCREEN_HEIGHT 64 // OLED display height, in pixels@@@@@@@@@@@@@@@@@@@@
    
    // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
    #define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

#define DS18B20_PIN_1   8  // 1-wire bus
#define DHTPIN       10 // DHT sensor
#define HEAT_RELAY_PIN  6 // Channel 1
#define LIGHT_RELAY_PIN 7 // Channel 2
#define DHTTYPE DHT11
#define RELAY_ON        HIGH
#define RELAY_OFF       LOW
#define DELTA_T         100

DS3231  rtc;
DHT dht(DHTPIN, DHTTYPE);
OneWire  oneWire(DS18B20_PIN_1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;
RTCDateTime dt;

const int     TargetTemperature = 2500;
bool          b_heat_relaystate = RELAY_OFF;
bool          b_light_relaystate = RELAY_OFF;
short int     si_nb_sensors = 0;
float         f_temp=-1;
float         f_temp2=-1;
float         f_humi =-1;
int           i_diff = 0;
char temp_buff[6]; 
char hum_buff[6];
float f=-1;
boolean b = true;

void setup()
{
  /* Initialize the RELAY to be OFF by default! DO NOT CHANGE - SECURITY FIRST */
   // Configure the Relays
  pinMode(HEAT_RELAY_PIN, OUTPUT);
  digitalWrite(HEAT_RELAY_PIN, RELAY_OFF);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  digitalWrite(LIGHT_RELAY_PIN, RELAY_OFF);

  pinMode(LED_BUILTIN, OUTPUT);
  
#if defined(DEVMODE)
  Serial.begin(9600);
#endif
#if defined(OLEDMODE)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
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
    //Serial.println(freeMemory());
    run_control();
    delay(4000);
  }
}

void run_control()
{
  dt = rtc.getDateTime();
  /* Measure once every four seconds. */

  digitalWrite(LED_BUILTIN, HIGH);
  f_temp = dht.readTemperature();
  f_humi = dht.readHumidity();
  if (isnan(f_temp) || isnan(f_humi)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  sensors.requestTemperatures();
  f_temp2 = sensors.getTempC(insideThermometer);
  if(f_temp2 == DEVICE_DISCONNECTED_C) 
  {
    f_temp2 = -1;
    return;
  } else {
    i_diff = TargetTemperature - (f_temp2 * 100);
  }
  dtostrf(f_temp2,5,1,temp_buff);
  dtostrf(f_humi,5,1,hum_buff);
  check_relay_state(i_diff);
  check_light_relay(dt.hour);
  digitalWrite(LED_BUILTIN, LOW);

#if defined(OLEDMODE)
  display.clearDisplay();
  display.setCursor(1,1);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  // Display date/time
  display.print(dt.day);
  display.print(F("/"));
  display.print(dt.month);
  display.print(F("/"));
  display.print(dt.year);
  display.setCursor(65,1);
  display.print(dt.hour); //text todisplay
  display.print(F(":"));
  display.print(dt.minute);
  f=(rtc.readTemperature());
  display.setCursor(105,1);
  display.print(f,0);
  display.println(F("C"));
  display.println(F(""));
  // Display Temp/Hum
  display.setTextSize(2);
  display.print( "T: " );
  display.print(temp_buff);
  display.println(F("C"));
  display.print( "H:" );
  display.print(hum_buff);
  display.print(F("%"));
  display.setTextSize(1);
  display.print(F("  "));
  // Display delta
  if (b_light_relaystate == RELAY_ON) {
    display.print(F("L"));
  }
  if (b_heat_relaystate == RELAY_ON) {
    display.print(F("H"));
  }
  display.println(F(""));
  display.println(F(""));
  display.println(F(""));
  display.print(F("Delta:"));
  display.print(i_diff);
  display.print(F("-"));
  display.print(si_nb_sensors);
  display.print(F("-"));
  dtostrf(f_temp,5,1,temp_buff);
  display.print(f_temp);
  display.print(F("C"));
  b = !b;
  if (b==true){
    display.print(F(".."));
  } else {
    display.print(F(""));
  }
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
  
  if (delta > DELTA_T)
  {
    if (b_heat_relaystate == RELAY_OFF) {
      b_heat_relaystate = RELAY_ON;
      digitalWrite(HEAT_RELAY_PIN, b_heat_relaystate);
    }
  }
  else
  {
    if (b_heat_relaystate == RELAY_ON) {
      b_heat_relaystate = RELAY_OFF;
      digitalWrite(HEAT_RELAY_PIN, b_heat_relaystate);
    }
  }
}

void check_light_relay(int hour) {
  
  if (hour >= 7 && hour < 22) {
    if (b_light_relaystate == RELAY_OFF) {
      b_light_relaystate = RELAY_ON;
      digitalWrite(LIGHT_RELAY_PIN, b_light_relaystate);
    }
  } else {
    if (b_light_relaystate == RELAY_ON) {
      b_light_relaystate = RELAY_OFF;
      digitalWrite(LIGHT_RELAY_PIN, b_light_relaystate);
    }
  }
}
