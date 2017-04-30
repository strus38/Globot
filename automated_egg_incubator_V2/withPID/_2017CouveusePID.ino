//-------------------------------------------------------------------
// Couveuse 2017
// - Arduino Mega controlled temperature. 
// - It includes 2 temperatures ds18B20 and 1 heater as a lamp 
// for PID control.
// - 1 LCD, 1 computer fan, 1 motor to turn the eggs automatically
// every 3 hours, 1 DHT11 for humidity control
// - TODO: ethernet shield to send data to AWS DynamoDB.
//------------------------------------------------------------------

// PID Library
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>

// So we can measure the temperature
#include <OneWire.h>
#include <DallasTemperature.h>
// So we can measure the humidity
#include <DHT.h>
//#include <dht_nonblocking.h>

// So we can save and retrieve settings
#include <EEPROM.h>

// Library for the basic liquid crystal
#include "ssd1306_i2c.h"
#include "font.h"
#include "_2017CouveusePID.h"

// ************************************************
// Pin definitions
// ************************************************

#define PIN_TEMP1    8  // 1-wire bus
#define PIN_TEMP2    9  // 1-wire bus
#define PIN_DHT11   10 // DHT sensor
#define PIN_RELAY    7   // Channel 2
#define PIN_HEAT     6 // Channel 1
#define PIN_BUTTON   2 // Button
#define PIN_POTENT  A0 // Potentiometer

// Thermistor variable
#define NUMSAMPLES 5

// ************************************************
// PID Variables and constants
// ************************************************

//Define Variables we'll be connecting to
double Setpoint;
double Input;
double Output;

volatile long onTime = 0;

// pid tuning parameters
double Kp;
double Ki;
double Kd;

// EEPROM addresses for persisted data
const int SpAddress = 0;
const int KpAddress = 8;
const int KiAddress = 16;
const int KdAddress = 24;
const int EclAddress = 32;

//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

// 10 second Time Proportional Output window
int WindowSize = 10000; 
unsigned long windowStartTime;

// ************************************************
// Auto Tune Variables and constants
// ************************************************
byte ATuneModeRemember=2;

double aTuneStep=500;
double aTuneNoise=1;
unsigned int aTuneLookBack=20;

boolean tuning = false;

PID_ATune aTune(&Input, &Output);

// ************************************************
// Display Variables and constants
// ************************************************

const int logInterval = 10000; // log every 10 seconds
long lastLogTime = 0;

// ************************************************
// States for state machine
// ************************************************
enum operatingState { OFF = 0, SETP, RUN, TUNE_P, TUNE_I, TUNE_D, AUTO};
operatingState opState = OFF;

// Button handling
int buttonState;
int lastButtonState = LOW;
long lastButtonTime = 0;
long longPressTime = 500;
boolean shortButtonPressed = false;
boolean longButtonPressed = false;
boolean change = false;
long timeout = 10000;


//***********************************************************
// Init the OLED screen
//***********************************************************
#define SDA 20
#define SCL 21
#define I2C 0x3C
SSD1306 display(I2C, SDA, SCL);
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
//void drawFrame1(int x, int y);
//void (*frameCallbacks[1])(int x, int y) = {drawFrame1};
// how many frames are there?
int frameCount = 1;
// one frame is currently displayed
int currentFrame = 0;

//***********************************************************
// Init some variables for sensors
//***********************************************************
//DHT_nonblocking dht_sensor( PIN_DHT11, DHT_TYPE_11 );
DHT dht;
OneWire  oneWire(PIN_TEMP1);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

bool Dht_changed=false;
float Dht_temperature;
float Dht_humidity;
int RollTime = 9000;       // 8 seconds
const int RollEvery = 5;    //180; // turn every 3 hours
double Ecl;

// ************************************************
// Setup and diSplay initial screen
// ************************************************
void setup()
{
   // Initialize Relay Control:
   pinMode(PIN_HEAT, OUTPUT);    // Output mode to drive relay
   digitalWrite(PIN_HEAT, LOW);  // make sure it is off to start

   pinMode(PIN_RELAY, OUTPUT);    // Output mode to drive relay
   digitalWrite(PIN_RELAY, LOW);  // make sure it is off to start
   
   pinMode(PIN_BUTTON, INPUT_PULLUP);
   int found = 0;
   DeviceAddress insideThermometer;
   if (sensors.getDeviceCount() != 2)
    found=0;
   if (!sensors.getAddress(insideThermometer, 0)) 
    found=1;
   sensors.setResolution(insideThermometer, 12);
   if (!sensors.getAddress(insideThermometer, 1)) 
    found=2;
   sensors.setResolution(insideThermometer, 12);

   dht.setup(PIN_DHT11);
   
   display.init();
   display.flipScreenVertically();
   display.clear();
   display.displayOn();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse init "+String(found));
   display.drawString(1, 10, "p=xx t=xx i=xx");
   display.drawString(1, 20, "Input=xx SP=xx");
   display.drawString(1, 30, "T=xx.xC H=xx.x%");
   display.drawString(1, 40, "Eclosion: 21");
   display.drawString(1, 50, "Date: 02/01/2017");
   display.display();
   delay(3000);
  
   // Initialize the PID and related variables
   LoadParameters();
   myPID.SetTunings(Kp,Ki,Kd);

   myPID.SetSampleTime(1000);
   myPID.SetOutputLimits(0, WindowSize);

  // Run timer2 interrupt every 15 ms 
  TCCR2A = 0;
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1<<TOIE2;
}

// ************************************************
// Timer Interrupt Handler
// ************************************************
SIGNAL(TIMER2_OVF_vect) 
{
  if (opState == OFF)
  {
    digitalWrite(PIN_HEAT, LOW);  // make sure relay is off
  }
  else
  {
    DriveOutput();
  }
}

// ************************************************
// Main Control Loop
//
// All state changes pass through here
// ************************************************
void loop()
{
   //lcd.clear();
   resetbutton();
  
   switch (opState)
   {
   case OFF:
      Off();
      break;
   case SETP:
      Tune_Sp();
      break;
    case RUN:
      Run();
      break;
   case TUNE_P:
      TuneP();
      break;
   case TUNE_I:
      TuneI();
      break;
   case TUNE_D:
      TuneD();
      break;
   }
}

// ************************************************
// Initial State - long or short button to enter setpoint
// ************************************************
void Off()
{
   myPID.SetMode(MANUAL);
   digitalWrite(PIN_HEAT, LOW);  // make sure it is off
   display.clear();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse Off");
   display.display();
   
   while(!(shortButtonPressed || longButtonPressed))
   {
      button();
   }
   
   //turn the PID on
   myPID.SetMode(AUTOMATIC);
   windowStartTime = millis();
   opState = RUN; // start control
}

// ************************************************
// Setpoint Entry State
// Potentiometer to change setpoint
// Short for tuning parameters
// Long for changing value
// ************************************************
void Tune_Sp()
{
   display.clear();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse SET Sp");
   display.drawString(1, 10, "p="+String(Kp)+" d="+String(Kd)+ " i="+String(Ki));
   
   while(true)
   {
      button();
      if (change){
        Setpoint = map(analogRead(PIN_POTENT), 0, 1023, 99, 15);
        display.drawString(1, 20, "Setpoint=#");
        display.display();
        //lcd.setCursor(15, 1);
        //lcd.print("#");
      };        
      if (longButtonPressed)
      {
         change =!change;
         return;
      }
      if (shortButtonPressed)
      {
         opState = TUNE_P;
         change=false;
         return;
      }

      if ((millis() - lastButtonTime) > timeout)  // return to RUN after 3 seconds idle
      {
         opState = RUN;
         change=false;
         return;
      }
      
      display.drawString(1, 20, "Setpoint="+String(Setpoint));
      display.display();
      //lcd.setCursor(0,1);
      //lcd.print(Setpoint);
      //lcd.print(" ");
      DoControl();
   }
}

// ************************************************
// Proportional Tuning State
// Potentiometer to change Kp
// Short for Ki
// Long for changing value
// ************************************************
void TuneP()
{
   display.clear();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse SET Kp");
   display.drawString(1, 10, "p="+String(Kp)+" d="+String(Kd)+ " i="+String(Ki));
   while(true)
   {
      button();
      if (change){
        Kp = map(analogRead(PIN_POTENT), 0, 1023, 1000, 500);
        display.drawString(1, 20, "Kp=#");
        display.display();
        //lcd.setCursor(15, 1);
        //lcd.print("#");
      };        
      if (longButtonPressed)
      {
         change =!change;
         return;
      }
      if (shortButtonPressed)
      {
         opState = TUNE_I;
         change=false;
         return;
      }
      if ((millis() - lastButtonTime) > timeout)  // return to RUN after 3 seconds idle
      {
         opState = RUN;
         change=false;
         return;
      }
      display.drawString(1, 20, "Kp="+String(Kp));
      display.display();
      //lcd.setCursor(0,1);
      //lcd.print(Kp);
      //lcd.print(" ");
      DoControl();
   }
}


// ************************************************
// Integral Tuning State
// Potentiometer to change Ki
// Short for Kd
// Long for changing value
// ************************************************
void TuneI()
{
   display.clear();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse SET Ki");
   display.drawString(1, 10, "p="+String(Kp)+" d="+String(Kd)+ " i="+String(Ki));
   while(true)
   {
      button();

      if (change){
        Ki = map(analogRead(PIN_POTENT), 0, 1023, 1000, 100)/1000.0;
        display.drawString(1, 20, "Ki=#");
        display.display();
        //lcd.setCursor(15, 1);
        //lcd.print("#");
      };        
      if (longButtonPressed)
      {
         change =!change;
         return;
      }
      if (shortButtonPressed)
      {
         opState = TUNE_D;
         change=false;
         return;
      }
      if ((millis() - lastButtonTime) > timeout)  // return to RUN after 3 seconds idle
      {
         opState = RUN;
         change=false;
         return;
      }
      display.drawString(1, 20, "Ki="+String(Ki));
      display.display();
      //lcd.setCursor(0,1);
      //lcd.print(Ki);
      //lcd.print(" ");
      DoControl();
   }
}

// ************************************************
// Derivative Tuning State
// Potentiometer to change Kd
// Short for RUN
// Long for changing value
// ************************************************
void TuneD()
{
   display.clear();
   display.setFontScale2x2(false);
   display.drawString(1, 0, "Couveuse SET Kd");
   display.drawString(1, 10, "p="+String(Kp)+" d="+String(Kd)+ " i="+String(Ki));
   while(true)
   {
      button();

      if (change){
        Kd = map(analogRead(PIN_POTENT), 0, 1023, 200, 50)/1000.0;
        display.drawString(1, 20, "Kd=#");
        display.display();
        //lcd.setCursor(15, 1);
        //lcd.print("#");
      };        
      if (longButtonPressed)
      {
         change =!change;
         return;
      }
      if (shortButtonPressed)
      {
         opState = RUN;
         change=false;
         return;
      }
      if ((millis() - lastButtonTime) > timeout)  // return to RUN after 3 seconds idle
      {
         opState = RUN;
         change=false;
         return;
      }
      display.drawString(1, 20, "Kd="+String(Kd));
      display.display();
      //lcd.setCursor(0,1);
      //lcd.print(Kd);
      //lcd.print(" ");
      DoControl();
   }
}

// ************************************************
// PID Control State
// Long for autotune
// Short - off
// ************************************************
void Run()
{

   SaveParameters();
   myPID.SetTunings(Kp,Ki,Kd);
   while(true)
   {
      button();
      if (shortButtonPressed)
      {
//         StartAutoTune();
           opState = SETP;
           return;
      }
      else if (longButtonPressed && (abs(Input - Setpoint) < 0.5))  // Should be at steady-state
      {
         StartAutoTune();
         resetbutton();
      }
    
      DoControl();

      Dht_changed = measure_environment( &Dht_temperature, &Dht_humidity );
      display.clear();
      display.setFontScale2x2(false);
      display.drawString(1, 0, "Couveuse RUN");
      display.drawString(1, 10, String(Kp)+"-"+String(Ki)+ "-"+String(Kd));
      display.drawString(1, 20, "Input="+String(Input));
      display.drawString(1, 30, "SP="+String(Setpoint));
      display.drawString(1, 40, "H="+String(Dht_humidity)+"%");
      display.drawString(1, 50, "J-"+String(Ecl));
      display.display();
      //display.drawXbm(7, 7, temperature_width, temperature_height, temperature_bits);
     
      delay(100);
   }
}


// ************************************************
// Read temperature
// ************************************************
double measuretemp(){
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

static bool measure_environment( float *temperature, float *humidity )
{
  static unsigned long measurement_timestamp = millis( );

  /* Measure once every four seconds. */
  if( millis( ) - measurement_timestamp > 2000ul )
  {
    //if( dht_sensor.measure( temperature, humidity ) == true )
    //{
      measurement_timestamp = millis( );
      Dht_humidity = dht.getHumidity();
      //Dht_temperature = dht.getTemperature();
      return( true );
    //}
  }

  return( false );
}

// ************************************************
// Detect long and short button presses
// ************************************************
void button(){
  buttonState = digitalRead(PIN_BUTTON);
  if (buttonState != lastButtonState) {
    if (buttonState == LOW) {
      if (millis()-lastButtonTime < longPressTime){
        shortButtonPressed = true;
      }
      else{
        longButtonPressed = true;
      }
    } 
    lastButtonState = buttonState;
    lastButtonTime = millis();
  }
}

// ************************************************
// Cleans button presses after action is taken
// ************************************************
void resetbutton(){
  shortButtonPressed=false;
  longButtonPressed=false;
}

// ************************************************
// Execute the control loop
// ************************************************
void DoControl()
{
  // Read the input:
  Input = measuretemp();

  if (tuning) // run the auto-tuner
  {
     if (aTune.Runtime()) // returns 'true' when done
     {
        FinishAutoTune();
     }
  }
  else // Execute control algorithm
  {
     myPID.Compute();
  }
  
  // Time Proportional relay state is updated regularly via timer interrupt.
  onTime = Output;
}

// ************************************************
// Called by ISR every 15ms to drive the output
// ************************************************
void DriveOutput()
{  
  long now = millis();
  // Set the output
  // "on time" is proportional to the PID output
  if(now - windowStartTime>WindowSize)
  { //time to shift the Relay Window
     windowStartTime += WindowSize;
  }
  if((onTime > 100) && (onTime > (now - windowStartTime)))
  {
     digitalWrite(PIN_HEAT,HIGH);
  }
  else
  {
     digitalWrite(PIN_HEAT,LOW);
  }
}

// ************************************************
// Start the Auto-Tuning cycle
// ************************************************

void StartAutoTune()
{
   // REmember the mode we were in
   ATuneModeRemember = myPID.GetMode();

   // set up the auto-tune parameters
   aTune.SetNoiseBand(aTuneNoise);
   aTune.SetOutputStep(aTuneStep);
   aTune.SetLookbackSec((int)aTuneLookBack);
   tuning = true;
}

// ************************************************
// Return to normal control
// ************************************************
void FinishAutoTune()
{
   tuning = false;

   // Extract the auto-tune calculated parameters
   Kp = aTune.GetKp();
   Ki = aTune.GetKi();
   Kd = aTune.GetKd();

   // Re-tune the PID and revert to normal control mode
   myPID.SetTunings(Kp,Ki,Kd);
   myPID.SetMode(ATuneModeRemember);
   
   // Persist any changed parameters to EEPROM
   SaveParameters();
}

// ************************************************
// Save any parameter changes to EEPROM
// ************************************************
void SaveParameters()
{
   if (Setpoint != EEPROM_readDouble(SpAddress))
   {
      EEPROM_writeDouble(SpAddress, Setpoint);
   }
   if (Kp != EEPROM_readDouble(KpAddress))
   {
      EEPROM_writeDouble(KpAddress, Kp);
   }
   if (Ki != EEPROM_readDouble(KiAddress))
   {
      EEPROM_writeDouble(KiAddress, Ki);
   }
   if (Kd != EEPROM_readDouble(KdAddress))
   {
      EEPROM_writeDouble(KdAddress, Kd);
   }
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
   Setpoint = EEPROM_readDouble(SpAddress);
   Kp = EEPROM_readDouble(KpAddress);
   Ki = EEPROM_readDouble(KiAddress);
   Kd = EEPROM_readDouble(KdAddress);
   Ecl = EEPROM_readDouble(EclAddress);
   
   // Use defaults if EEPROM values are invalid
   if (isnan(Setpoint))
   {
     Setpoint = 38.0;
   }
   if (isnan(Kp))
   {
     Kp = 850;
   }
   if (isnan(Ki))
   {
     Ki = 0.5;
   }
   if (isnan(Kd))
   {
     Kd = 0.1;
   }
   if (isnan(Ecl))
   {
     Ecl = 21;
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

