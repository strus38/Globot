#include <avr/interrupt.h>
#include "VirtualWire.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "Wire.h"
#include "LowPower.h"
#include "DHT.h"

#define TX_PIN  5
// Humidity sensor
#define DHTTYPE DHT11
#define DHT_PIN 6
// PIR sensor
#define PIR_PIN 2 //must be 2 or 3 to work with interruptions
//Light sensor
#define LED_PHOTORESISTANCE 5
// A led, always one ...
#define LED_PIN     13 //13 since it already has a resistor for led
//Temperature sensor
#define TEMP_WIRE_BUS 3 /* Digital pin of  the DS18B20s */
// Lets initialyze some stuff
OneWire tempWire(TEMP_WIRE_BUS);
DallasTemperature temp_sensors(&tempWire);
int ds18B20_count = 0;
float temps[1] 	= { 
  0.0 };
unsigned int SerialNumRead (byte);
// Internal Battery sensor value
int battVolts;   // made global for wider avaliblity throughout a sketch if needed, example a low voltage alarm, etc

volatile int interrupt = 0;
int last_i=0;
DHT dht(DHT_PIN, DHTTYPE);

void setup(void) {
  // Setup PINs so they can do what we need  
  //pinMode(TX_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  //vw_set_ptt_inverted(true);
  vw_setup(2400);
  vw_set_tx_pin(TX_PIN);
  
  /* Initialization */
  Serial.begin(9600);

  temp_sensors.begin(); /*Init temp sensors */
  ds18B20_count = temp_sensors.getDeviceCount(); /*Count sensors */

  //attachInterrupt(0, pin2Interrupt,RISING);
  // init of the Humidity sensor
  dht.begin();

  //Serial.println("Setup done");
  delay(5000); //some time for the PIR sensor to stabalyze
  //Serial.println("Ready");
}

void loop()
{

  wakeUp();
  //Serial.println("Going to sleep");
  //one value per minute
  rest();

}

void pin2Interrupt()
{
  /* This brings us back from sleep. */
  detachInterrupt(0);                 //disable interrupts while we wake up
  interrupt = 1;

}

/*
 SLEEP_MODE_IDLE        - least power savings
 SLEEP_MODE_ADC
 SLEEP_MODE_PWR_SAVE
 SLEEP_MODE_STANDBY
 SLEEP_MODE_PWR_DOWN    – most power savings
 */
void rest()
{

  attachInterrupt(0,pin2Interrupt,RISING);
  interrupt = 0;
  delay(75);
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  // some time for the arduino to wake up
  delay(75);
}

void wakeUp(void)
{
  char float_conv[6]; 
  char url[32];
  //Serial.print("Waking up...");
  if (interrupt) {
    Serial.println("interrupted");
    const char *msg2 = "PIR";
    digitalWrite(LED_PIN, HIGH);
    
    vw_send((uint8_t *)msg2,strlen(msg2));
    vw_wait_tx();
    digitalWrite(LED_PIN, LOW);

    interrupt = 0;
  } 
  else {
    detachInterrupt(0);
    if (last_i == 0 || last_i == 60) {
      if (last_i == 60) last_i = 1;
      last_i++;
      getTemp();
      delay(100);
      int light = analogRead( LED_PHOTORESISTANCE );
      delay(100);

      int vcc = getBandgap();
      delay(100);
      url[0] = '\0';
      sprintf(url, "vext=%d&t1=%s&light=%d", vcc, ftoa(float_conv, temps[0], 2),light);
      Serial.println(url);

      digitalWrite(LED_PIN, HIGH);
      vw_send((uint8_t *)url,strlen(url));
      vw_wait_tx();
      digitalWrite(LED_PIN, LOW);
      Serial.println("Transmitted");

    } 
    else if (last_i == 5) {
      last_i++;
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      if (isnan(t) || isnan(h)) {
        //Serial.println("Error while reading humidity");
      } 
      else {
        //Serial.print("Humidity (%): ");
        //Serial.println(h, 2);
        //Serial.print("Dew Point (oC): ");
        //Serial.println(dewPoint(t, h));
        sprintf(url, "hum=%s", ftoa(float_conv, h, 2));
        //Serial.println(url);

        digitalWrite(LED_PIN, HIGH);
        vw_send((uint8_t *)url,strlen(url));
        vw_wait_tx();
        digitalWrite(LED_PIN, LOW);
      }
    } 
    else {
      last_i++;   
    }
  }
}

void getTemp() {
  int i=0;
  temp_sensors.requestTemperatures();
  while (i<ds18B20_count) {
    temps[i] = temp_sensors.getTempCByIndex(i);
    ////Serial.print("temp: "); 
    ////Serial.println(temps[0]);
    i++;
  }
}

// Float support is hard on arduinos
// (http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1164927646)
char *ftoa(char *a, double f, int precision)
{
  long p[] = {
    0,10,100,1000,10000,100000,1000000,10000000,100000000                  };

  char *ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  return ret;
}

int getBandgap(void) // Returns actual value of Vcc (x 100)
{

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  // For mega boards
  const long InternalReferenceVoltage = 1115L;  // Adjust this value to your boards specific internal BG voltage x1000
  // REFS1 REFS0          --> 0 1, AVcc internal ref. -Selects AVcc reference
  // MUX4 MUX3 MUX2 MUX1 MUX0  --> 11110 1.1V (VBG)         -Selects channel 30, bandgap voltage, to measure
  ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR)| (0<<MUX5) | (1<<MUX4) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);

#else
  // For 168/328 boards
  const long InternalReferenceVoltage = 1056L;  // Adjust this value to your boards specific internal BG voltage x1000
  // REFS1 REFS0          --> 0 1, AVcc internal ref. -Selects AVcc external reference
  // MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG)         -Selects channel 14, bandgap voltage, to measure
  ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);

#endif
  delay(50);  // Let mux settle a little to get a more stable A/D conversion
  // Start a conversion  
  ADCSRA |= _BV( ADSC );
  // Wait for it to complete
  while( ( (ADCSRA & (1<<ADSC)) != 0 ) );
  // Scale the value
  int results = (((InternalReferenceVoltage * 1023L) / ADC) + 5L) / 10L; // calculates for straight line value 
  return results;

}

// dewPoint function NOAA
// reference: http://wahiduddin.net/calc/density_algorithms.htm
double dewPoint(double celsius, double humidity)
{
  double A0= 373.15/(273.15 + celsius);
  double SUM = -7.90298 * (A0-1);
  SUM += 5.02808 * log10(A0);
  SUM += -1.3816e-7 * (pow(10, (11.344*(1-1/A0)))-1) ;
  SUM += 8.1328e-3 * (pow(10,(-3.49149*(A0-1)))-1) ;
  SUM += log10(1013.246);
  double VP = pow(10, SUM-3) * humidity;
  double T = log(VP/0.61078);   // temp var
  return (241.88 * T) / (17.558-T);
}

// delta max = 0.6544 wrt dewPoint()
// 5x faster than dewPoint()
// reference: http://en.wikipedia.org/wiki/Dew_point
double dewPointFast(double celsius, double humidity)
{
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidity/100);
  double Td = (b * temp) / (a - temp);
  return Td;
}
