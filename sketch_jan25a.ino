#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "Adafruit_miniTFTWing.h"
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "BluefruitConfig.h"

#define ALIMENTAZIONE  5                             
#define LIGHT_OUT 13
#define HEAT_OUT 10
#define LIGHT_IN A0
#define HEAT_IN A1
#define TRIG 12
#define ECHO 11

#define ANAL_ON 255
#define ANAL_HIGH 192
#define ANAL_MEDIUM 128
#define ANAL_LOW 64
#define ANAL_OFF 0

#define LIGHT_MAXVALUE 1000

#define LIGHT_LOWMODE_MAXVALUE 0.7f
#define LIGHT_LOWMODE_MINVALUE 0.1f
#define LIGHT_MEDIUMMODE_MAXVALUE 0.8f
#define LIGHT_MEDIUMMODE_MINVALUE 0.2f
#define LIGHT_HIGHMODE_MAXVALUE 0.9f
#define LIGHT_HIGHMODE_MINVALUE 0.3f

#define DARK 0.1f
#define LOW_VALUE 0.3f
#define MEDIUM_VALUE 0.6f

#define HEAT_LOWMODE_MAXOFFSET 5
#define HEAT_MEDIUMMODE_MAXOFFSET 7
#define HEAT_HIGHMODE_MAXOFFSET 10

Adafruit_ST7735 tft = Adafruit_ST7735(5,  6, -1);

Adafruit_miniTFTWing ss;
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

unsigned long lastValueTime;

unsigned long lastBluetoothRoutine;

short displayStatus = 0;
short panelCursor = 0;
short displayValue = 0;

short lightValue = 0;
short lightOutput = 0;

short heatValue = 0;
short heatOutput = 0;

short lightMode = 0; //0 -> Auto_HIGH, 1 -> Auto_MEDIUM, 2 -> Auto_LOW, 3 -> Manu_ON, 4 -> Manu_HIGH, 5 -> Manu_LOW, 6 -> Manu_OFF
short heatMode = 0; //0 -> Auto_HIGH, 1 -> Auto_MEDIUM, 2 -> Auto_LOW, 3 -> Manu_ON, 4 -> Manu_HIGH, 5 -> Manu_LOW, 6 -> Manu_OFF
short settedValue = 25;
short settedTemp = 25;

boolean remoteController = true;

boolean buttonPressed = false;
boolean buttonAllowed = false;
boolean refresh = true;
boolean settingView = false;
boolean settingChanged = false;


void setup() {
  ss.begin();
  ss.setBacklight(0x0); //set the backlight fully on
  tft.initR(INITR_MINI160x80);   // initialize a ST7735S chip, mini display
  tft.setRotation(3);
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  initBluetooth();
  lastBluetoothRoutine = millis();
  
  
}

void loop() {
  if ((millis() - lastBluetoothRoutine) > 200){
    if (ble.isConnected()){
      if (remoteController)
        readBluetooth();
      printBluetooth();
      lastBluetoothRoutine = millis();
  }}
  readInputs();
  mainPanelRoutine();
  updateOutputValues();
  writeOutputs();
}

//--------------------------------------------BLUETOOTH---------------------------
void initBluetooth()
{
  ble.begin(VERBOSE_MODE);
}

void sendBluetooth(String data)
{
  ble.print("AT+BLEUARTTX=");
  ble.println(data);
}
void readBluetooth()
{
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if (strcmp(ble.buffer, "OK") == 0 || strcmp(ble.buffer, "ERROR") == 0)
    return;
  char* str = ble.buffer;
  int init_size = strlen(str);
  char delim[] = " ";
  char *ptr = strtok(str, delim);
  if (strcmp (ptr, "luce") == 0)
  {
    ptr = strtok(NULL, delim);
    if (strcmp (ptr, "A") == 0)
    {
      ptr = strtok(NULL, delim);
      if (strcmp (ptr, "alto") == 0)
        lightMode = 0;
      if (strcmp (ptr, "medio") == 0)
        lightMode = 1;
      if (strcmp (ptr, "basso") == 0)
        lightMode = 2;
    }
    else
    {
      lightMode = 7;
      float light = atof(ptr) / 100;
      lightOutput = light * 255;
    }
  }
  else if (strcmp (ptr, "temp") == 0)
  {
     ptr = strtok(NULL, delim);
     ptr = strtok(NULL, delim);
     if (strcmp(ptr, "alto") == 0)
      heatMode = 0;
     else if (strcmp(ptr, "medio") == 0)
      heatMode = 1;
     else 
      heatMode = 2;
     ptr = strtok(NULL, delim);
     settedTemp = atoi(ptr);
  }
}
void printBluetooth()
{
  String heatString = "temp ";
  heatString += heatValue;
  heatString += " ";
  heatString += settedTemp;
  heatString += " ";
  sendBluetooth(heatString);

  String lightString = "luce ";
  if (lightValue < LIGHT_MAXVALUE * DARK)
    lightString += "BUIO ";
  else if (lightValue < LIGHT_MAXVALUE * LOW_VALUE)
    lightString += "BASSO ";
  else if (lightValue < LIGHT_MAXVALUE * MEDIUM_VALUE)
    lightString += "MEDIO ";
  else
    lightString += "ALTO ";
  lightString += lightOutput;
  lightString += " ";
  sendBluetooth(lightString);
}

//--------------------------CODICE GESIONE SCHERMO----------------------

String Texts[][4] = {{"LIGHTS", "HEAT", "REMOTE"}, //MENU PRINCIPALE - 0
                    {"AUTOMATIC", "MANUAL", "LIGHT_V"}, {"HIGH", "MEDIUM", "LOW"}, {"FULL ON", "HIGH", "LOW", "FULL_OFF"}, {}, //LUCI
                    {"AUTOMATIC", "TEMPERATURE_V", "SET TEMP"}, {"HIGH", "MEDIUM", "LOW"}, {}, {}, //HEAT
                    {"REMOTE_ON", "REMOTE_OFF"}
};
String Titles[] = {"MAIN MENU", "LIGHT MENU", "AUTOMATIC", "MANUAL", "LIGHT VALUE", "HEAT MENU", "AUTOMATIC", "TEMPERATURE", "SET TEMP",  "REMOTE MENU"};
int Sizes[] = {3,3,3,4,0,3,3,0,0,2}; 

//--------------------GESTIONE PANNELLO-------------------------------------

void mainPanelRoutine()
{ int currentTime = millis();
  if (displayStatus == 4 || displayStatus == 8 || displayStatus == 11){
      if (refresh)
        printValuesMenu();
      else if (currentTime - lastValueTime > 300){
        updateValue();lastValueTime = currentTime;}}
  else if (refresh)
    if (settingView)
      printSettingView();
    else if (settingChanged)
      printSettedScreen();
    else
      printMenu();
  buttonsMainPanelRoutine();
}

void buttonsMainPanelRoutine() //--------------------Display buttons-----------------
{
  uint32_t buttons = ss.readButtons();
  buttonPressed=false;
  if (buttons == 3740) //joystik a riposo, 3740 è il codice a riposo
    buttonAllowed = true;
  
  if (buttonAllowed && (! (buttons & TFTWING_BUTTON_DOWN))) {
    if (displayStatus == 11)
      settedValue--;
    panelCursor = (panelCursor + 1)%Sizes[displayStatus];
    buttonPressed = true;
    buttonAllowed = false;
  }
  if (buttonAllowed && (! (buttons & TFTWING_BUTTON_UP))) {
    if (displayStatus == 11)
      settedValue++;
    panelCursor = panelCursor > 0 ? panelCursor - 1 : Sizes[displayStatus]-1;
    buttonPressed = true;
    buttonAllowed = false;
  }

  //qui se premo, controllo cosa devo cambiare o in che altro menù andare e metto il displayStatus
  if (buttonAllowed && (! (buttons & TFTWING_BUTTON_A))) {
    checkChanges();
    buttonAllowed = false;
  }
  if (buttonAllowed && (! (buttons & TFTWING_BUTTON_B))) {
    getBack();
    buttonAllowed = false;
  }
  if (buttonAllowed && (! (buttons & TFTWING_BUTTON_SELECT))) {
    settingView = true;
    refresh = true;
    buttonAllowed = false;
  }
  if (buttonPressed && displayStatus != 4 && displayStatus != 8 && displayStatus != 11)
      updateMenuList();
}
void updateMenuList()
{
    tft.setCursor(20, 0);
    tft.setTextColor(ST77XX_YELLOW);
    tft.println(Titles[displayStatus]);
    tft.setTextColor(ST77XX_WHITE);
    for (int i = 0; i<Sizes[displayStatus]; i++)
      tft.println(Texts[displayStatus][i]);
    tft.setCursor(0, (panelCursor+1)*16);
    tft.setTextColor(ST77XX_RED);
    tft.println(Texts[displayStatus][panelCursor]);
    refresh = false;
}
void printMenu()
{
  tft.fillScreen(ST77XX_BLACK);
  updateMenuList();
}

void checkChanges()  //-----------------changes-----------------
{
  if (settingView || settingChanged){
    settingView=false;
    settingChanged=false;
    refresh=true;
    return;
  }
  switch (displayStatus){
    case 0: //main manu
      switch (panelCursor){
        case 0: setDisplayStatus (1); return;
        case 1: setDisplayStatus (5); return;
        case 2: setDisplayStatus (9); return;
        case 3: setDisplayStatus (10);return;
      }
    case 1: //MAIN PANL LIGHT
      switch(panelCursor){
        case 0: setDisplayStatus (2); return; //automatico
        case 1: setDisplayStatus (3); return; //manuale
        case 2: setDisplayStatus (4); return; //valore temperatura
      } 

    case 2: //pannello automatico luci
      switch(panelCursor)
      {
        case 0: setLightMode(0); return; //luce automatica alta
        case 1: setLightMode(1); return; //luce automatica media
        case 2: setLightMode(2); return; //luce automatica bassa
      }
    case 3: //pannello manuale luci
      switch(panelCursor)
      {
        case 0: setLightMode(3);  return; //luce manuale ON
        case 1: setLightMode(4);  return; //luce manuale alta
        case 2: setLightMode(5);  return; //luce manuale bassa
        case 3: setLightMode(6);  return; //luce manuale OFF
      }
    case 5: //MAIN PANEL HEAT
        switch(panelCursor)
        {
          case 0: setDisplayStatus (6); return; //menu automatico
          case 1: setDisplayStatus (8); return; //valore temperatura
          case 2: setDisplayStatus (11); return; //valore temperatura
        }
    
    case 6: //pannello calore automatico
        switch(panelCursor)
        {
          case 0: setHeatMode(0); return; //calore automatica alta
          case 1: setHeatMode(1);  return; //calore automatica media
          case 2: setHeatMode(2);  return; //calore automatica bassa
        }
    case 9:
        switch(panelCursor)
        {
          case 0: setRemote(true); return; //abilito l'allarme
          case 1: setRemote(false); return; //disabilito l'allarme  
        }
    
    case 11: settedTemp = settedValue; setSettingChanged(); displayStatus = 5; return;  
    }
  
}
void setSettingChanged()
{
  settingChanged = true;
  refresh = true;
}
void setLightMode(int value)
{
  lightMode = value;
  setSettingChanged();
}
void setHeatMode(int value)
{
  heatMode = value;
  setSettingChanged();
}
void setRemote(boolean value)
{
  remoteController = value;
  setSettingChanged();
}

void setDisplayStatus(int dispStat)
{
  displayStatus = dispStat;
  refresh = true;
  panelCursor = 0;
}

void getBack()
{
  if (settingView || settingChanged){
    settingView=false;
    settingChanged=false;
    refresh=true;
    return;
  }
  switch (displayStatus){
    case 1: setDisplayStatus(0); return;
    case 2: setDisplayStatus(1); return;
    case 3: setDisplayStatus(1); return;
    case 4: setDisplayStatus(1); return;
    case 5: setDisplayStatus(0); return;
    case 6: setDisplayStatus(5); return;
    case 8: setDisplayStatus(5); return;
    case 9: setDisplayStatus(0); return;
    case 10: setDisplayStatus(0); return;
    case 11: setDisplayStatus(5);return;
  }
}

void printValuesMenu(){
  tft.fillScreen(ST77XX_BLACK);
  String Title = Titles[displayStatus];
  int value;
  if (displayStatus == 4)
    value = lightValue;
  else if (displayStatus == 8)
    value = heatValue;
  else
    value = settedValue;
  tft.setCursor(20, 0);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println(Title);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(5);
  tft.println(value); 
  tft.setTextSize(2);
  displayValue = value;
  refresh=false;
}
void updateValue()
{
  lastValueTime = millis();
  int value;
  if (displayStatus == 4)
    value = lightValue;
  else if (displayStatus == 8)
    value = heatValue;
  else
    value = settedValue;
  tft.setTextSize(5);
  tft.setCursor(0, 16);
  tft.setTextColor(ST77XX_BLACK);
  tft.println(displayValue);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 16);
  tft.println(value);
  tft.setTextSize(2);
  displayValue = value;
}
void printSettingView()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);

  String light = "L:", heat = "H:", temp="SET T: ";
  switch (lightMode)
  {
    case 0: light += "Auto_HIGH"; break;
    case 1: light += "Auto_MEDIUM";break;
    case 2: light += "Auto_LOW";break;
    case 3: light += "FULL_ON";break;
    case 4: light += "HIGH";break;
    case 5: light += "LOW";break;
    case 6: light += "FULL_OFF";break;
    case 7: light += "MANUAL "; 
  }
  switch (heatMode)
  {
    case 0: heat += "HIGH"; break;
    case 1: heat += "MEDIUM";break;
    case 2: heat += "LOW";break;
  }
  temp+=settedTemp;
  temp+=" C";
  tft.setTextColor(ST77XX_WHITE);
  tft.println(light);
  tft.println(heat);
  tft.println(temp);
  if (remoteController)
    {
      tft.setTextColor(ST77XX_GREEN);
      tft.println("REMOTE ON");
    }
  else
    {
      tft.setTextColor(ST77XX_RED);
      tft.println("REMOTE OFF");
    }
  refresh=false;
}
void printSettedScreen()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 10);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(" SETTING");
  tft.println(" CHANGED");
  tft.setTextSize(2);
  refresh = false;
}


//---------------------------------------CODICE LETTURA SENSORI----------------------------------------------

void readInputs()
{
   lightValue = analogRead(LIGHT_IN);
   int temp = analogRead(HEAT_IN);
   heatValue = (( 100.0 *  5 * temp) / 1024.0 * 3 / 5);
}
//-------------------------------------------CODICE AGGIORNAMENTO USCITE---------------------------------------

void updateOutputValues()
{
  updateLightOutput();
  updateHeatOutput();

}
void updateLightOutput()
{
  switch (lightMode)
  {
    case 0: refreshLight(0); break;
    case 1: refreshLight(1); break;
    case 2: refreshLight(2); break;
    case 3: lightOutput = ANAL_ON; break;
    case 4: lightOutput = ANAL_HIGH; break;
    case 5: lightOutput = ANAL_LOW; break;
    case 6: lightOutput = ANAL_OFF; break;
  }
}
void updateHeatOutput()
{
  switch (heatMode)
  {
    case 0: refreshHeat(0); break;
    case 1: refreshHeat(1); break;
    case 2: refreshHeat(2); break;
  }
}

void refreshLight(int mode)
{
  int darkness = LIGHT_MAXVALUE - lightValue;
  float maxValue, minValue;
  switch(mode)
  {
    case 0: 
            maxValue = LIGHT_MAXVALUE * LIGHT_LOWMODE_MAXVALUE;
            minValue = LIGHT_MAXVALUE * LIGHT_LOWMODE_MINVALUE;
            break;
    case 1: maxValue = LIGHT_MAXVALUE * LIGHT_MEDIUMMODE_MAXVALUE;
            minValue = LIGHT_MAXVALUE * LIGHT_MEDIUMMODE_MINVALUE;
            break;
    case 2: maxValue = LIGHT_MAXVALUE * LIGHT_HIGHMODE_MAXVALUE;
            minValue = LIGHT_MAXVALUE * LIGHT_HIGHMODE_MINVALUE;
            break;
  }
  if (darkness < minValue)
    lightOutput = 0;
  else if (darkness > maxValue)
    lightOutput = 255;
  else{
    lightOutput = ((darkness - minValue)/(maxValue - minValue)) * 255;
  }
}


void refreshHeat(int mode)
{
  int offset = settedTemp - heatValue;
  if (offset <= 0)
    heatOutput = 0;  
  else{
      
      float maxOffset;
      switch (mode)
      {
        case 0: maxOffset = HEAT_LOWMODE_MAXOFFSET; break;
        case 1: maxOffset = HEAT_MEDIUMMODE_MAXOFFSET; break;
        case 2: maxOffset = HEAT_HIGHMODE_MAXOFFSET; break;
      }
      if (offset >= maxOffset)
        heatOutput = 255;
      else
      {
        heatOutput = (offset / maxOffset) * 255;

      }
  }
}


//--------------------------------------------CODICE SCRITTURA USCITE-------------------------------------------

void writeOutputs()
{
  analogWrite(LIGHT_OUT, lightOutput);
  analogWrite (HEAT_OUT, heatOutput);
}
