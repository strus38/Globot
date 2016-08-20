#include <LiquidCrystal.h>
#include "Menu.h"
#include <Wire.h>
#include "RTClib.h"

//rs,e,db7-db4
LiquidCrystal lcd(3, 4, 5, 6, 7, 8);
RTC_DS1307 rtc;
long backlight_cpt;
bool erreur = false;

uint8_t configData[6] = {0};
int capteur_haut = 10;
int capteur_bas = 9;
int enable = 13;
int in1 = 12;
int in2 = 11;
int lcdbacklight = 2;
int cellulePhoto = A3;
int buttons = A1;
int bpReset = A2;
int levelBatt = A7;
int ledBattLow = A6;


static const char* MAIN_MENU_ITEMS[] = {
  "Config Date/Heure",
  "Heure Levee",
  "Heure Couche",
  "Auto luminosite",
  "Gestion forcee"
};
static const Menu_t MAIN_MENU = {
  "Votre choix ?",
  MAIN_MENU_ITEMS,
  5,
  &doMainMenuAction
};

void setup()
{
  pinMode(capteur_haut, INPUT);
  pinMode(capteur_bas, INPUT);
  pinMode(cellulePhoto, INPUT);
  pinMode(buttons, INPUT);
  pinMode(bpReset, INPUT);
  pinMode(lcdbacklight, OUTPUT);
  digitalWrite(lcdbacklight, HIGH);

  pinMode(levelBatt, INPUT);
  pinMode(ledBattLow, OUTPUT);
  analogWrite(ledBattLow, 0);

  digitalWrite(enable, LOW);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  
  lcd.begin(16, 2);
  byte Haut[8] = {B00100,B00100,B00100,B00100,B10101,B01110,B00100,B00000};
  byte Bas[8] = {B00000,B00100,B01110,B10101,B00100,B00100,B00100,B00100};
  lcd.createChar(1,Haut);
  lcd.createChar(0,Bas);
  
  Wire.begin();
  rtc.begin();
  backlight_cpt = millis();
  
  //Serial.begin(9600);
  //rtc.adjust(DateTime(2015, 7, 13, 15, 54, 0));
  //rtc.writenvram(0,configData,6);
}

 
void loop()
{ 
  rtc.readnvram(configData, 6, 0);
  uint8_t luminosite = configData[4];
  uint8_t lum_auto = configData[5];

  //12V -0.7V diode -> pont diviseur 2.2k + 1k -> 3.53V
  //5V = 1023, 3.53V = 722
  //déchargé quand < 10.3V donc 3.218V donc 658
  int valBatt = analogRead(levelBatt);
  if ((levelBatt > 0) && (levelBatt < 658)){
    analogWrite(ledBattLow, 255);
  }
  else{
    analogWrite(ledBattLow, 0);
  }

  if(analogRead(bpReset) < 10){
    digitalWrite(lcdbacklight, HIGH);
    lcd.display();
    backlight_cpt = millis();
  }
    
  if (readPushButton() != BP_NONE)
  {
    digitalWrite(lcdbacklight, HIGH);
    lcd.display();
    displayMenu(MAIN_MENU);
    backlight_cpt = millis();
    erreur = false;
  }
  else
  {
    if ((backlight_cpt+10000) < millis())
    {
      digitalWrite(lcdbacklight, LOW);
      lcd.noDisplay();
    }
    
    DateTime now = rtc.now();
    DateTime DTHeureLeve = DateTime(now.year(), now.month(), now.day(), configData[0], configData[1], 0);
    DateTime DTHeureCouche = DateTime(now.year(), now.month(), now.day(), configData[2], configData[3], 0);
    
    String Hpoint = ":";
    if ((now.second() % 2) == 0){
      Hpoint = " ";
    }
    //lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(nombre2digit(now.hour()) + Hpoint + nombre2digit(now.minute()) + " " + nombre2digit(now.day()) + "/" + nombre2digit(now.month()) + "/" + nombre2digit(now.year()));
    lcd.setCursor(0,1);
    
    if (configData[4] == 0){
      lcd.print(String(nombre2digit(configData[0]) + ":" + nombre2digit(configData[1]) + "      " + nombre2digit(configData[2]) + ":" + nombre2digit(configData[3])));
    }
    else{
      lcd.print("auto: " + String(configData[5]) + " lum:" + String(analogRead(cellulePhoto)/4));
    }

    if (erreur){
      lcd.setCursor(0,1);
      lcd.print(" ERREUR PORTE ! ");
      analogWrite(ledBattLow, 300);
      delay(100);
      analogWrite(ledBattLow, 0);
      return;
    }
    
    if (configData[4] == 0){  //manuel
      //ouvrir porte
      if ((now.secondstime() >= DTHeureLeve.secondstime()) && (now.secondstime() < DTHeureCouche.secondstime()))
      {
	ouvrir_porte();
      }
      
      //fermer porte
      if (now.secondstime() >= DTHeureCouche.secondstime())
      {
        fermer_porte();
      }
    }
    else{  //auto
      //ouvrir porte
      if ((now.secondstime() >= DTHeureLeve.secondstime()) && (now.secondstime() < DTHeureCouche.secondstime()) && ((analogRead(cellulePhoto)/4) > configData[5]))
      {
        ouvrir_porte();
      }
      //fermer porte
      if (now.secondstime() >= DTHeureCouche.secondstime() && ((analogRead(cellulePhoto)/4) < configData[5]))
      {
        fermer_porte();
      }
    }
  }
  delay(100);
}
 
/** Fonction retournant le bouton appuyé (s’il y en a un). */
Button_t readPushButton(void)
{
  unsigned int val = analogRead(buttons);
  //Serial.println(val);
  if (val > 1000) return BP_NONE;
  if (val < 50) return BP_RIGHT;
  if (val < 150) return BP_UP; 
  if (val < 300) return BP_DOWN;
  if (val < 450) return BP_LEFT;
  if (val < 700) return BP_SELECT;
  //if (analogRead(bpReset) < 10) return BP_RST;
 
  /* Par défaut aucun bouton n'est appuyé */
  return BP_NONE;
}
 
/** Affiche le menu passé en argument */
void displayMenu(const Menu_t &menu) 
{ 
  /* Variable pour le menu */
  int selectedMenuItem = 0;   // Choix selectionné
  boolean shouldExitMenu = false; // Devient true quand l'utilisateur veut quitter le menu
  Button_t buttonPressed;      // Contient le bouton appuyé
  
  /* Tant que l'utilisateur ne veut pas quitter le menu */
  while(!shouldExitMenu)
  {
    unsigned long temps = millis();
  
    /* Affiche le menu */
    lcd.clear();
    lcd.print(menu.prompt);
    lcd.setCursor(0, 1);
    lcd.print(menu.items[selectedMenuItem]);
 
    /* Attend le relâchement du bouton */
    while(readPushButton() != BP_NONE);
 
    /* Attend l'appui sur un bouton */
    while((buttonPressed = readPushButton()) == BP_NONE)
    {
      if((millis() - temps) > 10000){
        return;
      }
    }
 
    /* Anti rebond pour le bouton */
    delay(30);
 
    /* Attend le relâchement du bouton */
    while(readPushButton() != BP_NONE);
 
    /* Gére l'appui sur le bouton */
    switch(buttonPressed) {
    case BP_UP:
      if(selectedMenuItem > 0) {
        selectedMenuItem--;
      }
      break;
 
    case BP_DOWN:
      if(selectedMenuItem < (menu.nbItems - 1)) {
        selectedMenuItem++;
      }
      break;
 
    case BP_LEFT: // Bouton RST = sorti du menu
      shouldExitMenu = true;
      break;
 
    case BP_SELECT:  // Bouton SELECT = validation du choix
      menu.callbackFnct(selectedMenuItem);
      break;
    }
  }
}
 
//action des menus
void doMainMenuAction(byte selectedMenuItem)
{
  if(selectedMenuItem == 0) { //configurer date heure
    lcd.clear();
    lcd.print(F("Config Date/heure"));
  
    DateTime now = rtc.now();
    uint8_t ladate[5] = {0};
    ladate[0] = now.hour();
    ladate[1] = now.minute();
    ladate[2] = now.day();
    ladate[3] = now.month();
    ladate[4] = now.year()-2000;
    
    lcd.setCursor(0,1);
    lcd.print(String(nombre2digit(ladate[0]) + ":" + nombre2digit(ladate[1]) + " " + nombre2digit(ladate[2]) + "/" + nombre2digit(ladate[3]) + "/" + nombre2digit(ladate[4])));
      
    uint8_t val_bp = menu_fleches(ladate, 0, 4);
      
    if (val_bp == 20) {
      rtc.adjust(DateTime(ladate[4]+2000, ladate[3], ladate[2], ladate[0], ladate[1], 0));
    }
  }
  if(selectedMenuItem == 1) //config levée
  {
    lcd.clear();
    lcd.print(F("Heure ouverture"));
    
    lcd.setCursor(0,1);
    lcd.print(String(nombre2digit(configData[0]) + ":" + nombre2digit(configData[1])));
    //hh:mm
    
    uint8_t val_bp = menu_fleches(configData, 0, 1);
      
    if (val_bp == 20){
      rtc.writenvram(0, configData, 6);
    }
  }
  if(selectedMenuItem == 2) //config couché
  {
    lcd.clear();
    lcd.print(F("Heure fermeture"));
    
    lcd.setCursor(6,1);
    lcd.print(String(nombre2digit(configData[2]) + ":" + nombre2digit(configData[3])));
    //__6___hh:mm
    
    uint8_t val_bp = menu_fleches(configData, 2, 3);
      
    if (val_bp == 20){
      rtc.writenvram(0, configData, 6);
    }
  }
  if(selectedMenuItem == 3) //config luminosité auto
  {
    lcd.clear();
    lcd.print(F("Config lum v="));
     
    Button_t buttonPressed;
    uint8_t valPhoto = 0;
    uint8_t val_bp = 0;
    while (val_bp < 20)
    {
      valPhoto = analogRead(cellulePhoto) / 4;
      lcd.setCursor(13,0);
      lcd.print(String(valPhoto) + "  ");
      lcd.setCursor(0,1);
      if (configData[4] == 0)
        lcd.print("NON             ");
      else
        lcd.print("OUI seuil:" + String(configData[5]) + "  ");
      //val de 0 à 255
    
      while((buttonPressed = readPushButton()) == BP_NONE);
      /* Anti rebond pour le bouton */
      delay(30);
      /* Attend le relâchement du bouton */
      //while(readPushButton() != BP_NONE);
      /* Gére l'appui sur le bouton */
      switch(buttonPressed) {
        case BP_UP:
          if (configData[5] < 255){
            configData[5]++;
            configData[4] = 1;
          }
          break;
        case BP_DOWN:
          if (configData[5] > 0){
            configData[5]--;
            if (configData[5] == 0) configData[4] = 0;
          }
          break;
        case BP_SELECT:
          val_bp = 20;
          break;
        case BP_LEFT:
          val_bp = 21;
          break;
      }
    }
    if (val_bp == 20)
    {
      rtc.writenvram(0, configData, 6);
    }
  }
  if(selectedMenuItem == 4) //gestion forcée
  {
    lcd.clear();
    //lcd.setCursor(0,0);
    lcd.print("Gestion forcee");
    digitalWrite(enable, HIGH);
         
    Button_t buttonPressed;
    uint8_t val_bp = 0;
    while (val_bp < 20)
    {
      lcd.setCursor(0,1);
      while((buttonPressed = readPushButton()) == BP_NONE)
      {
        lcd.setCursor(0,1);
        lcd.print("                ");
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
      }
      /* Anti rebond pour le bouton */
      delay(30);
      /* Attend le relâchement du bouton */
      //while(readPushButton() != BP_NONE);
      /* Gére l'appui sur le bouton */
      switch(buttonPressed) {
        case BP_UP:
          lcd.print("ouverture...    ");
          if (digitalRead(capteur_haut) == false){
            digitalWrite(in1, HIGH);
            digitalWrite(in2, LOW);
          }
          break;
        case BP_DOWN:
          lcd.print("fermeture...    ");
          if (digitalRead(capteur_bas) == false){
            digitalWrite(in1, LOW);
            digitalWrite(in2, HIGH);
          }
          break;
        case BP_SELECT:
          val_bp = 20;
          break;
        case BP_LEFT:
          val_bp = 21;
          break;
      }
    }
    digitalWrite(enable, LOW);
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    lcd.clear();
  }
}

uint8_t menu_fleches(uint8_t *tval, uint8_t minpos, uint8_t maxpos)
{
  uint8_t val_bp = 0;
  uint8_t indexpos = minpos;
  Button_t buttonPressed;
  
  lcd.setCursor(indexpos*3,1);
  lcd.cursor();
    
  while (val_bp < 20)
  {
    while((buttonPressed = readPushButton()) == BP_NONE);
    /* Anti rebond pour le bouton */
    delay(30);
    /* Attend le relâchement du bouton */
    //while(readPushButton() != BP_NONE);
    /* Gére l'appui sur le bouton */
    switch(buttonPressed) {
      case BP_UP:
        if ((indexpos == 2) && (tval[indexpos] < 31) && (maxpos == 4))
          tval[indexpos]++;
        else if ((indexpos == 3) && (tval[indexpos] < 12) && (maxpos == 4))
          tval[indexpos]++;
        else if (((indexpos == 0) || (indexpos == 2)) && (tval[indexpos] < 23))
          tval[indexpos]++;
        else if (((indexpos == 1) || (indexpos == 3)) && (tval[indexpos] < 59))
          tval[indexpos]++;
        else if ((indexpos == 4) && (tval[indexpos] < 255) && (maxpos == 4))
          tval[indexpos]++;
        break;
      
      case BP_DOWN:
        if (tval[indexpos] > 0) tval[indexpos]--;
        break;
      
      case BP_LEFT: 
        if (indexpos > minpos) indexpos--;
        break;
      
      case BP_RIGHT:
        if (indexpos < maxpos) indexpos++;
        break;
      
      case BP_SELECT:
          val_bp = 20;
          break;
      
      case BP_RST:
          val_bp = 21;
          break;
    }
    lcd.setCursor((indexpos*3),1);
    if (indexpos != 4)
      lcd.print(nombre2digit(tval[indexpos]));
    else
      lcd.print(nombre2digit(tval[indexpos]) + "  ");
      
    lcd.setCursor((indexpos*3),1);
    
    delay(100);
  }
  lcd.noCursor();
  
  return val_bp;
}

void fermer_porte()
{
  long timeout = millis() + 15000;
  long timeoutcapthaut = millis() + 2000;
  while ((millis() < timeout) && (digitalRead(capteur_bas) == false))
  {
    if ((millis() > timeoutcapthaut) && digitalRead(capteur_haut))
    {
      timeout = 1000;
    }
    
    DateTime now = rtc.now();
    lcd.setCursor(7,1);
    if ((now.second() % 2) == 0){
      lcd.write(byte(1));
    }
    else{
      lcd.write(" ");
    }
    digitalWrite(enable, HIGH);
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  digitalWrite(enable, LOW);
  digitalWrite(in2, LOW);

  if (digitalRead(capteur_bas) == false){
    erreur = true;
  }
}

void ouvrir_porte()
{
  long timeout = millis() + 15000;
  long timeoutcaptbas = millis() + 2000;
  while ((millis() < timeout) && (digitalRead(capteur_haut) == false))
  {
    if ((millis() > timeoutcaptbas) && digitalRead(capteur_bas))
    {
      timeout = 1000;
    }
    
    DateTime now = rtc.now();
    lcd.setCursor(7,1);
    if ((now.second() % 2) == 0){
      lcd.write(byte(0));
    }
    else{
      lcd.write(" ");
    }
    digitalWrite(enable, HIGH);
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  }
  digitalWrite(enable, LOW);
  digitalWrite(in1, LOW);

  if (digitalRead(capteur_haut) == false){
    erreur = true;
  }
}

String nombre2digit(uint16_t nb)
{
  String val = "";
  if (nb < 10)
    val = "0" + String(nb);
  else
    val = String(nb);
  
  return val;
}
