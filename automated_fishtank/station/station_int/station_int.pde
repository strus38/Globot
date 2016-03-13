#include "WProgram.h"
#include <NewSoftSerial.h>
#include <AF_XPort.h>
#include <VirtualWire.h>

#define HTTPPATH "/arduino/xPortPost.php?"
#define WEB_HOST "XXXX.org"
#define IPADDR "XX.XX.XX.XX"
#define PORT 80

#define XPORT_RXPIN 2
#define XPORT_TXPIN 3
#define XPORT_RESETPIN 4
#define XPORT_DTRPIN 0
#define XPORT_CTSPIN 6
#define XPORT_RTSPIN 0

#define RX_PIN      8
#define LED_PIN     13
#define PIN_BUZZER  10

//*** Channel 1
//Baudrate 9600, I/F Mode 4C, Flow 00
//Port 10001
//Connect Mode : D4
//Send '+++' in Modem Mode enabled
//Show IP addr after 'RING' enabled
//Auto increment source port disabled
//Remote IP Adr: --- none ---, Port 00000
//Disconn Mode : 00
//Flush   Mode : 77

AF_XPort xPortSerial = AF_XPort(XPORT_RXPIN, XPORT_TXPIN, XPORT_RESETPIN, XPORT_DTRPIN, XPORT_RTSPIN, XPORT_CTSPIN);
char linebuffer[256];

uint8_t errno;

void setup(void) {

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(9600);
  Serial.println("Setup");

  //vw_set_ptt_inverted(true);
  vw_setup(2000);
  vw_set_rx_pin(RX_PIN);
  vw_rx_start();

  // set the data rate for the SoftwareSerial port
  xPortSerial.begin(57600);
  delay(1000);
}

void loop()
{
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  if (vw_get_message(buf, &buflen)) {
    digitalWrite(LED_PIN, HIGH);
    int i;
    for (i=0;i<buflen;i++) {
      Serial.print(buf[i]);
    }
    buf[i]='\0';
    Serial.println((char *)&buf);
    if (!strcmp("PIR",(char *)&buf)) {
      buzz(2500);
    } 
    else {
      Serial.println("");
      httpPost((char *)&buf);
    }
    digitalWrite(LED_PIN, LOW);
  }
}

void buzz(long frequency) {

  long delayValue = 1000000/frequency/2;
  long numCycles = frequency * 250/ 1000;
  for (long i=0; i < numCycles; i++){
    digitalWrite(PIN_BUZZER,HIGH); /* write the buzzer pin high to push out the diaphram */
    delayMicroseconds(delayValue); /*wait for the calculated delay value */
    digitalWrite(PIN_BUZZER,LOW); /* write the buzzer pin low to pull back the diaphram */
    delayMicroseconds(delayValue); /* wait againf or the calculated delay value */
  }
}

int httpPost(char* json)
{
  uint8_t ret;
  char *found=0;

  ret = xPortSerial.reset();
  switch (ret) {
  case  ERROR_TIMEDOUT: 
    { 
      Serial.println("Timed out on reset!"); 
      return 0;
    }
  case ERROR_BADRESP:  
    { 
      Serial.println("Bad respons on reset!");
      return 0;
    }
  case ERROR_NONE: 
    { 
      Serial.println("Reset OK!");
      break;
    }
  default:
    Serial.println("Unknown error"); 
    return 0;
  }

  // time to connect...
  ret = xPortSerial.connect(IPADDR, PORT);
  switch (ret) {
  case  ERROR_TIMEDOUT: 
    { 
      Serial.println("Timed out on connect"); 
      return 0;
    }
  case ERROR_BADRESP:  
    { 
      Serial.println("Failed to connect");
      return 0;
    }
  case ERROR_NONE: 
    { 
      Serial.println("Connected..."); 
      break;
    }
  default:
    Serial.println("Unknown error"); 
    return 0;
  }

  xPortSerial.print("GET "); 
  xPortSerial.print(HTTPPATH); 
  xPortSerial.print(json); 
  xPortSerial.println(" HTTP/1.1");
  Serial.print("GET "); 
  Serial.print(HTTPPATH); 
  Serial.print(json); 
  Serial.println(" HTTP/1.1");
  xPortSerial.print("Host: "); 
  xPortSerial.println(WEB_HOST);
  Serial.print("Host: "); 
  Serial.println(WEB_HOST);
  xPortSerial.println("");
  Serial.println("");

  while (1) {
    // read one line from the xport at a time
    ret = xPortSerial.readline_timeout(linebuffer, 255, 3000); // 3s timeout
    // if we're using flow control, we can actually dump the line at the same time!
    Serial.print(linebuffer);
    if (strstr(linebuffer, "HTTP/1.1 200 OK") == linebuffer)
      ret = 1;

    if (((errno == ERROR_TIMEDOUT) && xPortSerial.disconnected()) ||
      ((XPORT_DTRPIN == 0) &&
      (linebuffer[0] == 'D') && (linebuffer[1] == 0)))  {
      Serial.println("\nDisconnected...");
      return ret;
    }
  }
}

void base64encode(char *s, char *r) {
  char padstr[4];
  char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  uint8_t i, c;
  uint32_t n;

  i = 0;
  c = strlen(s) % 3;
  if (c > 0) { 
    for (i=0; c < 3; c++) { 
      padstr[i++] = '='; 
    } 
  }
  padstr[i]=0;

  i = 0;
  for (c=0; c < strlen(s); c+=3) { 
    // these three 8-bit (ASCII) characters become one 24-bit number
    n = s[c]; 
    n <<= 8;
    n += s[c+1]; 
    if (c+2 > strlen(s)) {
      n &= 0xff00;
    }
    n <<= 8;
    n += s[c+2];
    if (c+1 > strlen(s)) {
      n &= 0xffff00;
    }

    // this 24-bit number gets separated into four 6-bit numbers
    // those four 6-bit numbers are used as indices into the base64 character list
    r[i++] = base64chars[(n >> 18) & 63];
    r[i++] = base64chars[(n >> 12) & 63];
    r[i++] = base64chars[(n >> 6) & 63];
    r[i++] = base64chars[n & 63];
  }
  i -= strlen(padstr);
  for (c=0; c<strlen(padstr); c++) {
    r[i++] = padstr[c];  
  }
  r[i] = 0;
  Serial.println(r);
}



