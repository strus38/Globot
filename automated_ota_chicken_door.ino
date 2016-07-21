// Copyright (C) 2014 SISTEMAS O.R.P.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <LiquidCrystal.h>

#define SEARCH_AFTER_RESET "ready"
#define INTRO "\r\n"

#define AP_NORMAL "SISTEMASORP"
#define PWD_NORMAL "mypassword"
#define HOST_NORMAL "192.168.1.33"
#define PORT_NORMAL "49000"

#define AP_BOOTLOADER "SISTEMASORP"
#define PWD_BOOTLOADER "mypassword"
#define HOST_BOOTLOADER "192.168.1.33"
#define PORT_BOOTLOADER "50000"


boolean ok = false;
int counter = 0;
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  show: The string to be printed.
Returns: 
  Nothing.
Description: 
  It clears the LCD and print the string.
*/
void print_lcd(String show)
{
  lcd.clear();
  lcd.print(show);
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  command: The string to send.
  timeout: The maximum time in milliseconds the function can be running before a time out.
  wait_for1: The search string when the command succeeded.
  wait_for1: The search string when the command failed.
Returns: 
  The string contained in wait_for1, wait_for2 or the string TIMEOUT.
Description: 
  It sends the command trough the serial port and waits for one of the expected strings or exit with timeout
*/
String send(String command, int timeout, String wait_for1, String wait_for2)
{
  unsigned long time = millis();
  String received = "";
  
  Serial.print(command);
  Serial.print(INTRO);
  
  while(millis() < (time + timeout))
  {
    if(Serial.available() > 0)
    {
      received.concat(char(Serial.read()));
      if(received.indexOf(wait_for1) > -1)
      {
        return wait_for1;
      }
      else if(received.indexOf(wait_for2) > -1)
      {
        return wait_for2;
      }
    }
  }
  
  return "TIMEOUT";
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  wait_for: The search string.
  timeout: The maximum time in milliseconds the function can be running before a time out.
Returns: 
  True if the string was found, otherwise False.
Description: 
  It waits for the expected string.
*/
boolean look_for(String wait_for, int timeout)
{
  unsigned long time = millis();
  String received = "";
  
  while(millis() < (time + timeout))
  {
    if(Serial.available() > 0)
    {
      received.concat(Serial.readString());
      if(received.indexOf(wait_for) > -1)
      {
        return true;
      }
    }
  }
  
  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  command: The string to send.
  timeout: The maximum time in milliseconds the function can be running before a timeout.
  wait_for1: The search string when the command succeeded.
  wait_for1: The search string when the command failed.
Returns: 
  True if the wait_for1 string was found, otherwise False.
Description: 
  It sends the command trough the serial port and waits for one of the expected strings or exit with timeout.
*/
boolean send_command(String command, int timeout, String wait_for1, String wait_for2)
{
  String state;
  
  state = send(command, timeout, wait_for1, wait_for2);
  if(state == wait_for1)
  {
    return true;
  }
  else if(state == wait_for2)
  {
    // do something on error
  }
  else
  {
    // do something on timeout
  }
  
  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  Nothing
Returns: 
  True if all commands were successfully, otherwise False.
Description: 
  It initializes the module, joins to the access point and connects to the server.
*/
boolean init_commands()
{
  print_lcd("Changing mode");
  if(send_command("AT+CWMODE=1", 5000, "OK", "ERROR"))
  {
    print_lcd("Resetting module");
    if (send_command("AT+RST", 5000, SEARCH_AFTER_RESET, "ERROR"))
    {
      print_lcd("Joining AP");
  
      String cwjap = "AT+CWJAP=\"";
      cwjap += AP_NORMAL;
      cwjap += "\",\"";
      cwjap += PWD_NORMAL;
      cwjap += "\"";
      if (send_command(cwjap, 20000, "OK", "FAIL"))
        if (send_command("AT+CIPMUX=0", 2000, "OK", "ERROR"))
          if (send_command("AT+CIPMODE=0", 2000, "OK", "ERROR"))
          {
            print_lcd("Connecting host");
            
            String cipstart = "AT+CIPSTART=\"TCP\",\"";
            cipstart += HOST_NORMAL;
            cipstart += "\",";
            cipstart += PORT_NORMAL;
            if (send_command(cipstart, 5000, "OK", "ERROR"))
              return true;
          }
    }
  }
            
  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  Nothing
Returns: 
  True if all commands were successfully, otherwise False.
Description: 
  It initializes the module, joins to the access point, connects to the server and starts the protocol.
*/
boolean boot_commands()
{
  print_lcd("Joining AP");
  
  String cwjap = "AT+CWJAP=\"";
  cwjap += AP_BOOTLOADER;
  cwjap += "\",\"";
  cwjap += PWD_BOOTLOADER;
  cwjap += "\"";
  if (send_command(cwjap, 20000, "OK", "FAIL"))
    if (send_command("AT+CIPMUX=0", 2000, "OK", "ERROR"))
      if (send_command("AT+CIPMODE=1", 2000, "OK", "ERROR"))
      {
        print_lcd("Connecting host");
        
        String cipstart = "AT+CIPSTART=\"TCP\",\"";
        cipstart += HOST_BOOTLOADER;
        cipstart += "\",";
        cipstart += PORT_BOOTLOADER;
        if (send_command(cipstart, 5000, "OK", "ERROR"))
          if (send_command("AT+CIPSEND", 2000, ">", "ERROR"))
          {
            print_lcd("Init protocol");
            if (send_command("hello", 2000, "welcome", "error"))
              if (send_command("Arduino_remote_example.cpp.hex", 2000, "ok", "error"))
                return true;
          }
      }
            
  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
/*
Parameters:
  Nothing
Returns: 
  True if all commands were successfully, otherwise False.
Description: 
  It sends a string to the remote host and show it in the display.
*/
boolean test()
{
  String command = "AT+CIPSEND=";
  String to_send = "hello guys! ";
  to_send += counter;
  command += to_send.length() + 2;
  
  if (send_command(command, 2000, ">", "ERROR"))
    if (send_command(to_send + "\r\n", 2000, "OK", "ERROR"))
    {
      lcd.clear();
      lcd.print(to_send);
      counter++;
      return true;
    }
  
  return false;
}
//-----------------------------------------------------------------------------------------------------------------------------------
void setup()
{
  pinMode(13, OUTPUT);
  Serial.begin(115200);
  lcd.begin(16, 2);
  lcd.print("Init WI07C...");
  
  // Remove any garbage from the RX buffer
  delay(3000);
  while(Serial.available() > 0) Serial.read();
  
  ok = init_commands();
}
//-----------------------------------------------------------------------------------------------------------------------------------
void loop()
{
  if(ok)
  {
    digitalWrite(13, HIGH);
    ok = test();
    if(ok && look_for("reboot", 5000))
    {
      if(boot_commands())
      {
        pinMode(12, OUTPUT);
        digitalWrite(12, LOW);
        for(;;);
      }
      else
      {
        ok = false;
      }
    }
  }  
  else
  {
    digitalWrite(13, LOW);
    lcd.clear();
    lcd.print("Error sending");
    lcd.setCursor(0, 1);
    lcd.print("AT commands");
    for(;;);
  }
}
