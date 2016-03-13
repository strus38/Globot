#include "WProgram.h"
#include <NewSoftSerial.h>#include <Time.h>
#include <AF_XPort.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <EEPROMI2C.h>
#include <LCDI2C.h>
#include <string.h>
#include <DHT.h>

// Define some Constants... either using #define or Const if we need it
//----------------------------------------------------------------------------//

#ifdef DEBUG
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINT(x) Serial.print(x)
#else
  #define DEBUG_PRINT(x)
#endif

//  Constants used for the LCD Screen: 20 rows
const int LCD_WIDTH = 20

// Constants used for the Arduino PINs
#define PIN_WATER_LEVEL 0   /* Analog Pin 0 	*/
#define PIN_TEMP_WIRE_BUS 	10  /* Digital Pin 10 */
#define PIN_BUCKPUCK_3  3   /* Digital Pin 3 */
#define PIN_BUCKPUCK_9  9   /* Digital Pin 9 */
#define PIN_TRIG        13  /* Water level, digital Pin 13 */
#define PIN_ECHO        12  /* Water level, digital Pin 12 */
#define PIN_DHT          2   /* DHT, digital Pin 2 */
#define PIN_TX  		 5	/* RF433 Transmission to RaspberryPI central node */

// Constants used for the DHT sensor
#define DHTTYPE DHT11
#define DHT_HUMI 0
#define DHT_TEMP 1


// Constants used for the I2C bus
//const int I2C_eeprom_address  = 0x50; /* I2C EEPROM address (EEPROMI2C.h) */
const int I2C_clock_address  = 0x68;  	/* I2C clock address */
const int I2C_LCD_address    = 0x4C;  	/* I2C LCD address */


/* Channels used on the Chester's Garage board for LED */
#define CHANNEL_LED_WHITE  1
#define CHANNEL_LED_BLUE   3
#define LED_WHITE  0 //used in arrays
#define LED_BLUE   1 //used in arrays
const int LED_ON 	= 250;
const int LED_OFF 	= 0;

/* Tank depth between the Sonar sensor and the bottom of the Tank */
const float TANK_DEPTH	  = 90;
/* Distance between the Sonar Sensor and the water */
float cm = 0;

/* LCD Globals */
/* Keypad layout */
/* 
 	1 2 3 A
 	4 5 6 B
 	7 8 9 C
 	* 0 # D
 
 	* = 104
 	# = 105
 */
const char keypad[17] = {
  -1,1,2,3,101,4,5,6,102,7,8,9,103,104,0,105,106};

// Now, define all the global variables.
//----------------------------------------------------------------------------//

// This structure contains all the sensors data that would be sent over RF433 to the central node (RPi)
// before it is pushed in the server DB
struct sensordata {
  int 	nodeID;
  int 	date;
  int 	time;
  float temps[3];
  float dht_vals[2];
  long  waterLevel;
  int	leds[2];
};
typedef struct sensordata SensorsData;

// initial brightness of the LCD screen
int LCD_BRIGHTNESSNESS = 128;

/* Date/Time Globals */
byte    Second; 	/* Store second value */
byte    Minute; 	/* Store minute value */
byte    Hour; 		/* Store hour value */
byte    Day; 		/* Store day value */
byte    Date; 		/* Store date value */
byte    Month; 		/* Store month value */
byte    Year; 		/* Store year value */
int		MilTime; 	/* create military time output [0000,2400]*/
int 	SaveMilTime = 0;
int     InitLevel   = 0;

/* Config variables - if EEPROM not initialized */
int 	light_ON_MilTime  = 800;  /*Default value if conf file broken */
int 	light_OFF_MilTime = 2000; /*Default value if conf file broken */
int 	diming_MilTime    = 1900; /*Default value if conf file broken */
int 	alarm_LIGHT       = 70;   /*Default value if conf file broken */
int 	alarm_AQUA        = 30;   /*Default value if conf file broken */
int     water_DELTA       = 40;   /*Default value if conf file broken */
int 	temp_LIGHT	  = 1; 	  /*Default value if conf file broken */
int 	temp_AQUA	  = 0; 	  /*Default value if conf file broken */
int 	temp_EXTRA	      = 2; 	  /*Default value if conf file broken */

/* Alarm status: 0 = none, 1 = light, 2 = aqua */
byte 	alarm_STATUS      = 0;

/* Config stored in the EEPROM */
struct config {
  int lightOn;
  int lightOff;
  int lightDimmer;
  int tempAqua;
  int tempLight;
  int alarmAqua;
  int alarmLight;
  int levelWater;
} config;

int ds18B20_count = 0; /* Number of DS sensors detected on the 1-wire bus */
int loopNbr = 0; /* Tricks to have a more responsive keyboard since we do not use interrupts yet */


DHT dht(PIN_DHT, DHTTYPE);
OneWire tempWire(PIN_TEMP_WIRE_BUS);
DallasTemperature temp_sensors(&tempWire);
LCDI2C	lcd = LCDI2C(4,20,I2C_LCD_address,1);
SensorsData data;

void setup(void) {
  
  DEBUG_PRINTLN("Setup: ENTER");
  setup_pins();
  
  Wire.begin(); /*initialize the I2C bus*/
  delay(50);
  Serial.begin(9600);
  
  delay(500);
  setup_lcd();

  delay(500);
  dht.begin();
 
  setup_sensors_data();
  DEBUG_PRINTLN("Setup: EXIT");
}

void loop()
{
  // Handle keyboard interraction
  process_keyboard();
  
  // Drive the light
  setLight();
  
  // Process sensor data sometimes...
  if (bcdToDec(Second) % 2 == 0)
	  process_info();


}

void setup_pins() {
  TCCR1B = TCCR1B & 0b11111000 | 0x02;  // Sets the PWM freq to about 3900 Hz on pins 3 and 11
  TCCR2B = TCCR2B & 0b11111000 | 0x02;  // Sets the PWM freq to about 3900 Hz on pins 9 and 10

  /* pin modes */
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  //pinMode(PIN_WATER_CFG, OUTPUT);
  pinMode(PIN_BUCKPUCK_3, OUTPUT);
  pinMode(PIN_BUCKPUCK_9,OUTPUT);
}

void setup_lcd() {
  lcd.init();
  lcd.set_backlight_brightness(LCD_BRIGHTNESS);
  lcd.clear();
  lcd.print("Glowbot - Init...");
  DEBUG_PRINTLN("Init...");
  temp_sensors.begin(); 													/*Init temp sensors */
  ds18B20_count = temp_sensors.getDeviceCount();  /*Count sensors */
  DEBUG_PRINTLN(ds18B20_count);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Glowbot - Init...");
  lcd.setCursor(1,0);
  lcd.print("Found DS18B20= ");
  lcd.print(ds18B20_count,DEC);
  lcd.setCursor(2,0);
  lcd.print("Loading config ...");
  loadConfiguration();
  lcd.setCursor(3,0);
  lcd.print("Ready!");
  lcd.clear();
}

void setup_sensors_data() {

  int i = 0;
  // Initialyzing all the sensor data, so at least if nothing works, we will get values
  data.nodeID = 1; // ID used to recognyze which module sends the values to the central node
  data.time = 0;
  for (i=0;i<ds18B20_count; i++) {
  	data.temps[i] = 0;
  }
  // Not beautiful but it works ... except if you have more than 2 colors LEDs
  for (i=0;i<2; i++) {
  	data.dht_vals[i] = 0.0;
	data.leds[i] = 0.0;
  }
  data.waterLevel = 0.0;
  // End of initialization
}

void process_keyboard() {

  int i = 0;
  int key;
  int set_time = 0;
  int not_set = 0;
  int val;
  int action;
  int mode = 100;
  if(keypad[lcd.keypad()] != -1) {
    mode = keypad[lcd.keypad()];
  }
  for (;;){
    if (keypad[lcd.keypad()] == -1) break;
  }
  if (mode == 1) {
    lcd.on();
  } 
  else if (mode == 2) {
    lcd.off();
  }
  else if (mode == 3) {
    LCD_BRIGHTNESS += 10;
    lcd.set_backlight_brightness(LCD_BRIGHTNESS);
  }
  else if (mode == 6) {
    LCD_BRIGHTNESS -= 10;
    lcd.set_backlight_brightness(LCD_BRIGHTNESS);
  } 
  else if (mode == 101) { /* A */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Glowbot - Config");
    lcd.setCursor(1,0);
    lcd.print("1-Set Date");
    lcd.setCursor(2,0);
    lcd.print("2-Set Light");
    lcd.setCursor(3,0);
    lcd.print("3-Exit");

    for (;;){
      key = keypad[lcd.keypad()];
      if (key != -1) break;
    }
    for (;;){
      if (keypad[lcd.keypad()] == -1) break;
    }

    if (key == 1) {
      while(set_time == 0){
        if (i == 0) {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Glowbot - Set Date");
          lcd.setCursor(1,0);
          lcd.print("Date DD/MM/YY HH:MM");
        }
        lcd.setCursor(2,i);
        if (i == 2 || i == 5) lcd.print("/");
        else if (i == 8) lcd.print(" ");
        else if (i == 11) lcd.print(":");
        else if (i > 13) {
          setTime();
          set_time = 1;
          lcd.clear();
        } 
        else {
          for (;;){
            key = keypad[lcd.keypad()];
            if (key != -1 && key >=0 && key <= 9) break;
          }
          for (;;){
            if (keypad[lcd.keypad()] == -1) break;
          }
          if (key == 105) break; /* # */
          if (i==6) Year=key*10;
          if (i==7) Year=Year+key;
          if (i==3) Month=key*10;
          if (i==4) Month=Month+key;
          if (i==0) Day=key*10;
          if (i==1) Day=Day+key;
          if (i==9) Hour=key*10;
          if (i==10) Hour=Hour+key;
          if (i==12) Minute=key*10;
          if (i==13) Minute=Minute+key;
          lcd.print(key);
        }
        i++;
      }
    } 
    else if (key == 2) {
      not_set = 0;
      val = -1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Glowbot - Set Light");
      lcd.setCursor(1,0);
      lcd.print("Intensity: X");
      //lcd.print(getLEDIntensity(CHANNEL_LED_WHITE));
      i=0;
      while(not_set == 0) {
        lcd.blink_on();
        lcd.setCursor(1,i+11);
        for (;;){
          key = keypad[lcd.keypad()];
          if (key != -1) break;
        }
        for (;;){
          if (keypad[lcd.keypad()] == -1) break;
        }
        if (key == 105) break; 	/* # */
        if (key == 104) { 	/* * */
          if (val != -1 && val <= 255) {
            setLEDIntensity(CHANNEL_LED_WHITE, val);
            lcd.blink_off();
            not_set = 1;
            lcd.clear();
          }
        } 
        else {
          lcd.print(key);
          if (val == -1)
            val = key;
          else {
            val = 10*val + key;
          }
        }
        i++;
      }
    } 
    else {
      lcd.clear();
    }
  }
  else if (mode == 102) { /* B */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Glowbot - Config");
    lcd.setCursor(1,0);
    lcd.print("1-Load Default Config");
    lcd.setCursor(2,0);
    lcd.print("2-Modify Config");
    lcd.setCursor(3,0);
    lcd.print("3-Exit");

    for (;;){
      key = keypad[lcd.keypad()];
      if (key != -1) break;
    }
    for (;;){
      if (keypad[lcd.keypad()] == -1) break;
    }
    if (key == 1) {
      setDefaultConfiguration();
      loadConfiguration();
      lcd.clear();
    } 
    else if (key == 2) {
      /* Display current configuration */
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ON=");
      lcd.print(config.lightOn);
      lcd.setCursor(0,9);
      lcd.print("OFF=");
      lcd.print(config.lightOff);
      lcd.setCursor(1,0);
      lcd.print("DIM=");
      lcd.print(config.lightDimmer);
      lcd.setCursor(1,9);
      lcd.print("Ta=");
      lcd.print(config.tempAqua);
      lcd.setCursor(2,0);
      lcd.print("Tl=");
      lcd.print(config.tempLight);
      lcd.setCursor(2,9);
      lcd.print("Aa=");
      lcd.print(config.alarmAqua);
      lcd.setCursor(3,0);
      lcd.print("Al=");
      lcd.print(config.alarmLight);
      lcd.setCursor(3,9);
      lcd.print("LW=");
      lcd.print(config.levelWater);
      /* Allow configuration modifications */
      int r=0;
      int c=0;
      for (int conf_id=1;conf_id<=8;conf_id++) {
        not_set = 0;
        val = -1;
        //while (not_set == 0) {
        if (conf_id==1) {
          //lcd.setCursor(0,3);
          r=0; 
          c=3;
        } 
        else if (conf_id==2) {
          //lcd.setCursor(0,13);
          r=0; 
          c=13;
        } 
        else if (conf_id==3) {
          //lcd.setCursor(1,3);
          r=1; 
          c=4;
        } 
        else if (conf_id==4) {
          //lcd.setCursor(1,13);
          r=1; 
          c=12;
        } 
        else if (conf_id==5) {
          //lcd.setCursor(2,3);
          r=2; 
          c=3;
        } 
        else if (conf_id==6) {
          //lcd.setCursor(2,13);
          r=2; 
          c=12;
        } 
        else if (conf_id==7) {
          //lcd.setCursor(3,3);
          r=3; 
          c=3;
        }
        else if (conf_id==8) {
          //lcd.setCursor(3,13);
          r=3; 
          c=12;
        }
        while (not_set == 0) {
          lcd.blink_on();
          lcd.setCursor(r,c);
          for (;;){
            key = keypad[lcd.keypad()];
            if (key != -1) break;
          }
          for (;;){
            if (keypad[lcd.keypad()] == -1) break;
          }
          if (key == 105) break;
          if (key == 104) {
            if (val != -1) {
              DEBUG_PRINT("Modif config=");
              DEBUG_PRINT(conf_id);
              DEBUG_PRINT(" val=");
              DEBUG_PRINTLN(val);
              switch(conf_id) {
              case 1:
                config.lightOn = val;
                break;
              case 2:
                config.lightOff = val;
                break;
              case 3:
                config.lightDimmer = val;
                break;
              case 4:
                config.tempAqua = val;
                break;
              case 5:
                config.tempLight = val;
                break;
              case 6:
                config.alarmAqua = val;
                break;
              case 7:
                config.alarmLight = val;
                break;
              case 8:
                config.levelWater = val;
                break;
              }
              lcd.blink_off();
              not_set = 1;
            }
          } 
          else {
            lcd.print(key);
            if (val == -1)
              val = key;
            else {
              val = 10*val + key;
            }
          }
          c++;
        }
        if (conf_id == 8) {
          lcd.blink_off();
          lcd.clear();
          DEBUG_PRINTLN("Saving config");
          eeWrite(0,config);
          delay(1000);
          loadConfiguration();
        }
      }
      if (key == 105) { /* # */
        lcd.clear();
      }
      lcd.clear();
    } 
    else {
      lcd.clear();
    }
  }
  else if (mode == 103) { /* C */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Glowbot - Config");
    lcd.setCursor(1,0);
    lcd.print("1-Show Logs");
    lcd.setCursor(2,0);
    lcd.print("2-Delete Logs");
    lcd.setCursor(3,0);
    lcd.print("3-Exit");

    for (;;){
      key = keypad[lcd.keypad()];
      if (key != -1) break;
    }
    for (;;){
      if (keypad[lcd.keypad()] == -1) break;
    }
    if (key == 1) {
      /*
      for (int j = 1; j <= db_data.nRecs(); j++) {
       db_data.read(j, DB_REC mydata);
       lcd.clear();
       lcd.setCursor(0,0);
       lcd.print("Glowbot - Show Logs");
       lcd.setCursor(1,0);
       lcd.print("Date: ");
       lcd.print(mydata.dateTime);
       lcd.setCursor(2,0);
       lcd.print("Name: ");
       lcd.print(mydata.id);
       lcd.setCursor(3,0);		 		 
       lcd.print("Value: ");
       lcd.print(mydata.value);
       for (;;){
       key = keypad[lcd.keypad()];
       if (key != -1) break;
       }
       for (;;){
       if (keypad[lcd.keypad()] == -1) break;
       }
       if (key == 104) continue;
       if (key == 105) { 
       break; 
       }
       }*/
      lcd.clear();
    } 
    else if (key == 2) {/*
      for (int j = 1; j <= db_data.nRecs(); j++) { 
     db_data.deleteRec(j);
     }*/
      lcd.clear();
    } 
    else {
      lcd.clear();
    }
  } 
  else if (mode == 106) { /* D */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Glowbot - NONE");
  }
}
void process_info() {

  if (loopNbr > 6) loopNbr = 0;


  if (loopNbr == 0)
    /*Read Temperatures */
    getTemp();
  
  
  /*Read date/time */
  if (loopNbr == 1)
    getTime();

  if (loopNbr == 2)
    getDHT();

  /*Display new values on the LCD screen*/
  if (loopNbr == 3)
    displayLCD();
  
  if (loopNbr == 4) {
    /*Read water level */
    DEBUG_PRINT("Reading water level: ");
    getWaterLevel();
    SaveMilTime = MilTime;
    DEBUG_PRINTLN(MilTime);
    InitLevel = 1;
  }
  
  if (loopNbr == 5)
  	loopNbr=loopNbr+1;
  
  
}

void setLight() {
  int bou = light_OFF_MilTime - diming_MilTime;
  int cur;
  if ((MilTime >= light_ON_MilTime) && (MilTime < diming_MilTime)) {
    setLEDIntensity(CHANNEL_LED_BLUE, LED_OFF);
    setLEDIntensity(CHANNEL_LED_WHITE, LED_ON);
  } else {
    if ((MilTime >= diming_MilTime) && (MilTime < light_OFF_MilTime)) {
      float dimValue = (float)MilTime - (float)diming_MilTime;
      cur = map(dimValue,0,bou,0,LED_ON);
      setLEDIntensity(CHANNEL_LED_WHITE, LED_ON - cur);
      setLEDIntensity(CHANNEL_LED_BLUE,cur);
    } 
    else {
      setLEDIntensity(CHANNEL_LED_BLUE, LED_ON);
      setLEDIntensity(CHANNEL_LED_WHITE, LED_OFF);
    }
  }
}

void getDHT() {
  
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    // check if returns are valid, if they are NaN (not a number) then something went wrong!
    if (isnan(t) || isnan(h)) {
        DEBUG_PRINTLN("Failed to read from DHT");
    } else {
        data.dht_vals[DHT_HUMI] = h;
        data.dht_vals[DHT_TEMP] = t;
        DEBUG_PRINT("DHT=");
        DEBUG_PRINTLN(h);
    } 
}

void getTemp() {
  int i=0;
  temp_sensors.requestTemperatures();
  while (i<ds18B20_count) {
    data.temps[i] = temp_sensors.getTempCByIndex(i);
    DEBUG_PRINT("Temp=");
    DEBUG_PRINTLN(data.temps[i]);
    i++;
  }
}

void displayLCD() {

  int i = 0;
  /* Display introduction */
  lcd.setCursor(0,0);
  lcd.print("Glowbot v1.0");

  /* Display Date/Time */
  lcd.setCursor(0,15);
  if(Hour < 10){
    lcd.print("0");
  }
  lcd.print(Hour, HEX);
  lcd.print(":");
  if(Minute < 10){
    lcd.print("0");
  }
  lcd.print(Minute, HEX);

  /* Display AQUA status */
  lcd.setCursor(1,0);
  lcd.print("T1=");
  if (alarm_STATUS & 1) {
    lcd.print(" !");
  } 
  else {
    lcd.print(" ");
  }
  lcd.print(data.temps[temp_AQUA], 1);
  lcd.print("oC");
  lcd.print(" LVL=");
  lcd.print(data.waterLevel, DEC);
  lcd.print("cm");
  
  /* Display LIGHT status */
  lcd.setCursor(2,0);
  lcd.print("T2=");
  if (alarm_STATUS & 2) {
    lcd.print("!");
  } 
  else {
    lcd.print(" ");
  }
  lcd.print(data.temps[temp_LIGHT], 1);
  lcd.print("oC W=");
  lcd.print(data.leds[LED_WHITE],DEC);
  lcd.print(" B=");
  lcd.print(data.leds[LED_BLUE],DEC);

  /* Display LIGHT status */
  lcd.setCursor(3,0);
  lcd.print("T3= ");
  if (data.temps[temp_EXTRA] != 0.0) {   
    lcd.print(data.temps[temp_EXTRA], 1);
  } else {
    lcd.print(data.dht_vals[DHT_TEMP], 1);
  }
  lcd.print("oC  oH=");
  lcd.print(data.dht_vals[DHT_HUMI], 1);

}

void clearlcd(int c) {
  //clearing line
  for (int i =c;i<LCD_WIDTH;i++) {
    lcd.print(" ");
  }
}

byte decToBcd(byte val)
{
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val)
{
  return ( (val/16*10) + (val%16) );
}

void setTime() {

  Wire.beginTransmission(I2C_clock_address);
  Wire.send(0);
  Wire.send(decToBcd(Second));
  Wire.send(decToBcd(Minute));
  Wire.send(decToBcd(Hour));
  Wire.send(decToBcd(Day));
  Wire.send(decToBcd(Date));
  Wire.send(decToBcd(Month));
  Wire.send(decToBcd(Year));
  Wire.endTransmission();
}

void getTime() {
  char tempchar [7];
  byte i = 0;
  Wire.beginTransmission(I2C_clock_address);
  Wire.send(0);
  Wire.endTransmission();

  Wire.requestFrom(I2C_clock_address, 7);

  while(Wire.available()) /* Check for data from slave */
  {
    tempchar [i] = Wire.receive(); /* receive a byte as character */
    i++;
  }
  Second = tempchar [0];
  Minute = tempchar [1];
  Hour   = tempchar [2];
  Day    = tempchar [3];
  Date   = tempchar [4];
  Month  = tempchar [5];
  Year   = tempchar [6];

  MilTime = (bcdToDec(Hour) * 100) + bcdToDec(Minute);
  data.time = MilTime;
  data.date = (bcdToDec(Month) * 100) + bcdToDec(Day);
}

void setDefaultConfiguration() {
  DEBUG_PRINTLN("Setting default Configuration");
  config.lightOn	= 800;
  config.lightOff 	= 2000;
  config.lightDimmer    = 1900;
  config.tempAqua 	= 0;
  config.tempLight  	= 1;
  config.alarmAqua  	= 28;
  config.alarmLight 	= 50;
  config.levelWater 	= 40;
  eeWrite(0,config);
  delay(30);
}

void loadConfiguration() {
  int val;
  DEBUG_PRINT("Loading configuration... ");
  eeRead(0, config);
  delay(30);
  applyConfiguration();
  DEBUG_PRINTLN("Done");
}

void applyConfiguration() {

  light_ON_MilTime = config.lightOn;
  light_OFF_MilTime = config.lightOff;
  diming_MilTime = config.lightDimmer;
  temp_AQUA = config.tempAqua;
  temp_LIGHT = config.tempLight;
  alarm_LIGHT = config.alarmAqua;
  alarm_AQUA = config.alarmLight;
  water_DELTA = config.levelWater;

}

void getWaterLevel() {
  
    long duration, distance;
    digitalWrite(PIN_TRIG, LOW);  // Added this line
    delayMicroseconds(2); // Added this line
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10); // Added this line
    digitalWrite(PIN_TRIG, LOW);
    duration = pulseIn(PIN_ECHO, HIGH);
    distance = (duration/2) / 29.1;
    if (distance >= 200 || distance <= 0){
      data.waterLevel=0;
    }
    else {
      data.waterLevel=distance;
    }
    DEBUG_PRINT("Water level=");
    DEBUG_PRINTLN(data.waterLevel);
}

void setLEDIntensity(int channel, int val) {
  int pin = PIN_BUCKPUCK_3;
  switch(channel) {
  case CHANNEL_LED_WHITE:
    pin = PIN_BUCKPUCK_3;
    DEBUG_PRINT("setLED pin=");
    DEBUG_PRINT(pin);
    DEBUG_PRINT(" Val=");
    DEBUG_PRINTLN(val);
    
    analogWrite(pin,val);
	data.leds[LED_WHITE] = val;
    break;
  case CHANNEL_LED_BLUE:
    pin = PIN_BUCKPUCK_9;
    
    DEBUG_PRINT("setLED pin=");
    DEBUG_PRINT(pin);
    DEBUG_PRINT(" Val=");
    DEBUG_PRINTLN(val);
    
    analogWrite(pin,val);
	data.leds[LED_BLUE] = val;
    break;
  }
}

void sendmsg_masternode() {
	// Convert the struct to a string so it can easily be sent over RF
	
}


