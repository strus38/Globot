#include "WProgram.h"
#include <NewSoftSerial.h>
#include <AF_XPort.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <EEPROMI2C.h>
#include <LCDI2C.h>
#include <string.h>


/* List Arduino PINs used */
#define PIN_WATER_LEVEL 0 	/* Analog Pin 0 	*/
#define PIN_WATER_CFG   13	/* Digital Pin 13 */
#define TEMP_WIRE_BUS 	10  /* Digital Pin 10 */
#define PIN_BUCKPUCK_3  3 	/* Digital Pin 3 */
#define PIN_BUCKPUCK_9  9 	/* Digital Pin 9 */

/* List of I2C addresses */
const int I2C_clock_address  = 0x68;  /* I2C clock address */
const int I2C_LCD_address    = 0x4C;  /* I2C LCD address */
/* Not needed because it is already hard-coded in EEPROMI2C.h */
//const int I2C_eeprom_address  = 0x50; /* I2C EEPROM address */

/* Channels used on the Chester's Garage board for LED */
#define LED_WHITE  1
#define LED_BLUE   3

/* Tank depth between the Sonar sensor and the bottom of the Tank */
const float TANK_DEPTH	  = 90;
/* Distance between the Sonar Sensor and the water */
float cm = 0;

// initial brightness of the LCD screen
int BRIGHT = 128;

/* Date/Time Globals */
byte    Second; 	/* Store second value */
byte    Minute; 	/* Store minute value */
byte    Hour; 		/* Store hour value */
byte    Day; 		/* Store day value */
byte    Date; 		/* Store date value */
byte    Month; 		/* Store month value */
byte    Year; 		/* Store year value */
int			MilTime; 	/* create military time output [0000,2400]*/
int 		SaveMilTime = 0;
int     InitLevel = 0;

/* Config variables - if EEPROM not initialized */
int 		light_ON_MilTime  = 800;  /*Default value if conf file broken */
int 		light_OFF_MilTime = 2000; /*Default value if conf file broken */
int 		diming_MilTime    = 1900;   /*Default value if conf file broken */
int 		alarm_LIGHT       = 70;   /*Default value if conf file broken */
int 		alarm_AQUA        = 30;   /*Default value if conf file broken */
int     water_DELTA       = 40;   /*Default value if conf file broken */
int 		need_ATO          = 0; 		/*Default value if conf file broken */
int 		temp_LIGHT	  		= 1; 	 /*Default value if conf file broken */
int 		temp_AQUA	    		= 0; 	 /*Default value if conf file broken */

/* Alarm status: 0 = none, 1 = light, 2 = aqua */
byte 		alarm_STATUS      = 0;

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
int 		keypad_delay 	= 15;
LCDI2C	lcd = LCDI2C(4,20,I2C_LCD_address,1);
#define lenght 14.0

double percent=100.0;

unsigned char b;
unsigned int peace;
byte p1[8] = {
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10,
  0x10};

byte p2[8] = {
  0x18,
  0x18,
  0x18,
  0x18,
  0x18,
  0x18,
  0x18,
  0x18};

byte p3[8] = {
  0x1C,
  0x1C,
  0x1C,
  0x1C,
  0x1C,
  0x1C,
  0x1C,
  0x1C};

byte p4[8] = {
  0x1E,
  0x1E,
  0x1E,
  0x1E,
  0x1E,
  0x1E,
  0x1E,
  0x1E};

byte p5[8] = {
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F};

/* Config stored in EEPROM */
struct config
{
  int lightOn;
  int lightOff;
  int lightDimmer;
  int tempAqua;
  int tempLight;
  int alarmAqua;
  int alarmLight;
  int levelWater;
} 
config;

/* DS18B20 Globals */
OneWire tempWire(TEMP_WIRE_BUS);
DallasTemperature temp_sensors(&tempWire);
int ds18B20_count = 0; /* Number of DS sensors detected on the 1-wire bus */

float temps[3] 		= {
  0.0, 0.0, 0.0};

int loopNbr = 0; /* Tricks to have a more responsive keyboard */
int white_ON = 250;
int blue_ON = 250;

void setup(void) {
  TCCR1B = TCCR1B & 0b11111000 | 0x02;  // Sets the PWM freq to about 3900 Hz on pins 3 and 11
  TCCR2B = TCCR2B & 0b11111000 | 0x02;  // Sets the PWM freq to about 3900 Hz on pins 9 and 10

  /* pin modes */
  pinMode(PIN_WATER_CFG, OUTPUT);
  pinMode(PIN_BUCKPUCK_3, OUTPUT);
  pinMode(PIN_BUCKPUCK_9,OUTPUT);
  /* Initialization */
  Wire.begin(); /*initialize the I2C bus*/
  delay(50);
  Serial.begin(9600);

  lcd.init();
  lcd.set_backlight_brightness(BRIGHT);

  lcd.load_custom_character(1,p1);
  delay(50);
  lcd.load_custom_character(2,p2);
  delay(50);
  lcd.load_custom_character(3,p3);
  delay(50);
  lcd.load_custom_character(4,p4);
  delay(50);
  lcd.load_custom_character(5,p5);
  delay(50);

  lcd.clear();
  lcd.print("Glowbot - Init...");

  temp_sensors.begin(); 													/*Init temp sensors */
  ds18B20_count = temp_sensors.getDeviceCount();  /*Count sensors */

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

  delay(1000);
  lcd.clear();
  Serial.println("Setup");
}

void loop()
{
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
    BRIGHT += 10;
    lcd.set_backlight_brightness(BRIGHT);
  }
  else if (mode == 6) {
    BRIGHT -= 10;
    lcd.set_backlight_brightness(BRIGHT);
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
      //lcd.print(getLEDIntensity(LED_WHITE));
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
            setLEDIntensity(LED_WHITE, val);
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
              Serial.print("Modif config=");
              Serial.print(conf_id);
              Serial.print(" val=");
              Serial.println(val);
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
          Serial.println("Saving config");
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

  /* Process sensor data */
  process_info();


}

void process_info() {

  if (loopNbr > 4) loopNbr = 0;

  /*Read date/time */
  if (loopNbr == 1)
    getTime();

  if (loopNbr == 0)
    /*Read Temperatures */
    getTemp();

  if ((SaveMilTime == 0 && InitLevel == 0) || (MilTime - SaveMilTime >= 10)) {
    /*Read water level */
    Serial.print("Reading water level: ");
    getWaterLevel();
    SaveMilTime = MilTime;
    Serial.println(MilTime);
    InitLevel = 1;
  }

  //if (loopNbr == 2)
    setLight();

  /*Display new values on the LCD screen*/
  if (loopNbr == 3)
    displayLCD();

  loopNbr=loopNbr+1;
}

void setLight() {
  int bou = light_OFF_MilTime - diming_MilTime;
  int cur;
  if (MilTime >= light_ON_MilTime && MilTime < diming_MilTime) {
    setLEDIntensity(LED_BLUE,0);
    setLEDIntensity(LED_WHITE, white_ON);
  } 
  else {
    if (MilTime >= diming_MilTime && MilTime < light_OFF_MilTime) {
      float dimValue = ((float)light_OFF_MilTime - (float)MilTime) / (float)bou;
      //cur = map(dimValue,0,100,0,white_ON);
      cur = dimValue * white_ON;
      //Serial.println(dimValue);
      //Serial.println(cur);
      setLEDIntensity(LED_WHITE, cur);
      setLEDIntensity(LED_BLUE,0);
    } else {
      setLEDIntensity(LED_BLUE, 200);
      setLEDIntensity(LED_WHITE, 0);
    }
  }
}

void getTemp() {
  int i=0;
  temp_sensors.requestTemperatures();
  while (i<ds18B20_count) {
    temps[i] = temp_sensors.getTempCByIndex(i);
    i++;
  }
}

void displayLCD() {

  int i = 0;

  /* Display introduction */
  lcd.setCursor(0,0);
  lcd.print("Glowbot ");

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
  lcd.print("AQUA:");
  if (alarm_STATUS & 1) {
    lcd.print(" !");
  } 
  else {
    lcd.print("  ");
  }
  lcd.print(temps[temp_AQUA], 1);
  lcd.print("oC");
  lcd.print("  ATO=");
  lcd.print(need_ATO, DEC);

  /* Display LIGHT status */
  lcd.setCursor(2,0);
  lcd.print("LIGHT:");
  if (alarm_STATUS & 2) {
    lcd.print("!");
  } 
  else {
    lcd.print(" ");
  }
  lcd.print(temps[temp_LIGHT], 1);
  lcd.print("oC");

  /* Display WATER status */
  if (cm > TANK_DEPTH) {
    percent = 100.0;
  } 
  else {
    percent = cm/TANK_DEPTH*100.0;
  }
  double a=lenght/100*percent;

  lcd.setCursor(3,16);
  lcd.print((int)(TANK_DEPTH - cm), DEC);
  lcd.print("cm");
  //lcd.print((int)percent, DEC);
  lcd.setCursor(3,0);
  // drawing black rectangles on LCD
  //lcd.init_bargraph(LCDI2C_HORIZONTAL_BAR_GRAPH);
  //lcd.draw_horizontal_graph(3,0,25,50);


  if (a>=1) {
    for (int i=1;i<a;i++) {
      lcd.write(4);
      b=i;
    }
    a=a-b;
  }
  peace=a*5;
  // drawing charater's colums
  switch (peace) {
  case 0:
    break;
  case 1:
    lcd.write(0);
    break;
  case 2:
    lcd.write(1);
    break;
  case 3:
    lcd.write(2);
    break;
  case 4:
    lcd.write(3);
    break;
  }

  //clearing line
  for (int i =0;i<(lenght-b);i++) {
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
}

void setDefaultConfiguration() {
  Serial.println("Setting default Configuration");
  config.lightOn			= 800;
  config.lightOff 		= 2000;
  config.lightDimmer  = 1900;
  config.tempAqua 		= 0;
  config.tempLight  	= 1;
  config.alarmAqua  	= 28;
  config.alarmLight 	= 50;
  config.levelWater 	= 40;
  eeWrite(0,config);
  delay(30);
}

void loadConfiguration() {
  int val;
  Serial.print("Loading configuration... ");
  eeRead(0, config);
  delay(30);
  applyConfiguration();
  Serial.println("Done");
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

  // establish variables for duration of the ping, 
  // and the distance result in inches and centimeters:
  digitalWrite(PIN_WATER_CFG,HIGH);     //raise the reset pin high
  delay(120);                       		//start of calibration ring
  float sensorvalue = analogRead(PIN_WATER_LEVEL); //get analog sensor value from pin 0
  cm = 2.54 * (254.0/1024.0) *2.0* sensorvalue; //convert to inches
  /*delay(1000);    */  //optional delay 1 second
  digitalWrite(PIN_WATER_CFG,LOW);      //turn off Calibration ring and sensor

  need_ATO = 0;
  if (TANK_DEPTH - cm <= water_DELTA) {
    need_ATO = 1; 
  } 
  else {
    need_ATO = 0;
  }
}

void setLEDIntensity(int channel, int val) {
  int pin = PIN_BUCKPUCK_3;
  switch(channel) {
  case LED_WHITE:
    pin = PIN_BUCKPUCK_3;
    /*Serial.print("setLED pin=");
    Serial.print(pin);
    Serial.print(" Val=");
    Serial.println(val);
    */
    analogWrite(pin,val);
    break;
  case LED_BLUE:
    pin = PIN_BUCKPUCK_9;
    /*
    Serial.print("setLED pin=");
    Serial.print(pin);
    Serial.print(" Val=");
    Serial.println(val);
    */
    analogWrite(pin,val);
    break;
  }
}

