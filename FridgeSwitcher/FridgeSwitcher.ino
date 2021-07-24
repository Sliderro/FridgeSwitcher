#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>

#define ENCA 2
#define ENCB 3
#define SWITCH 4
#define TEMP 5
#define RELAY 6

struct EEData {
  unsigned long freq;
  float tempMax;
  float tempRange;
};

volatile byte aFlag = 0;
volatile byte bFlag = 0;
volatile byte encoderPos = 127;
volatile byte reading = 0;
volatile byte buttonFlag = 1;


unsigned long tempTime;
float temp = 0.0;

const char * menuOpts[] = {
  "Czest. Pomiaru",
  "Maks. Temp.",
  "Zakres Aktyw.",
  "Reset"
};


EEData eedata;

bool inMenu = false;
unsigned long inMenuTime = 0;

bool inSubMenu = false;

int subMenu;

bool relayState = true;

OneWire oneWire(TEMP);
DallasTemperature sensors(&oneWire);

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  pinMode(ENCA, INPUT_PULLUP);
  pinMode(ENCB, INPUT_PULLUP);
  pinMode(SWITCH, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  attachInterrupt(0, IncEnc, RISING);
  attachInterrupt(1, DecEnc, RISING);

  EEPROM.get(0, eedata);

  Serial.begin(9600);
  sensors.begin();
  u8g2.begin();

  Serial.println(eedata.freq);
  tempTime = -eedata.freq;
  Serial.println(eedata.tempMax);
  Serial.println(eedata.tempRange);


}

void loop() {
  if (millis() - tempTime > eedata.freq) {
    measureTemp();
  }
  checkButton();
  inMenuTimeout();
  OLED();
  relaySwitcher();
  if (relayState) {
    digitalWrite(RELAY, HIGH);
  } else {
    digitalWrite(RELAY, LOW);
  }
}

void IncEnc() {
  cli();
  reading = PIND & 0xC;
  if (reading == B00001100 && aFlag) {
    encoderPos ++;
    bFlag = 0;
    aFlag = 0;
    if(!inMenu){
      encoderPos = 128;
    }
    inMenu = true;
    inMenuTime = millis();
  }
  else if (reading == B00000100) bFlag = 1;
  sei();
}

void DecEnc() {
  cli();
  reading = PIND & 0xC;
  if (reading == B00001100 && bFlag) {
    encoderPos --;
    bFlag = 0;
    aFlag = 0;
    if(!inMenu){
      encoderPos = 128;
    }
    inMenu = true;
    inMenuTime = millis();
  }
  else if (reading == B00001000) aFlag = 1;
  sei();
}

void checkButton() {
  if (buttonFlag != digitalRead(SWITCH) && buttonFlag == 1) {
    buttonFlag = digitalRead(SWITCH);
    Serial.println("Clicked");
    if(!inSubMenu){
      Serial.println("inSubMenu");
      if(inMenu){
        inSubMenu = true;
        subMenu = encoderPos % 4;
        encoderPos = subMenuToPos();
      }
    } else {
      saveEedata();
      inMenu = false;
      inSubMenu = false;
    }
  }
  buttonFlag = digitalRead(SWITCH);
}

void measureTemp() {
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);
  tempTime = millis();
}

void OLED() {
  if (!inMenu) {
    displayTemp();
  } else {
    if (!inSubMenu) {
      displayMenu();
    } else {
      displaySubMenu();
    }
  }
}

void inMenuTimeout(){
  if(millis() - inMenuTime > 10000){
     inMenu = false;
     inSubMenu = false;
  }
}

void displayTemp() {
  char currTemp[25];
  char maxTemp[25];
  char currTempValue[9];
  char maxTempValue[9];
  char state[25];
  char freq[25];
  strcpy(currTemp, "Temp.:");
  dtostrf(temp, 3, 1, currTempValue);
  strcat(currTemp, currTempValue);
  strcat(currTemp, "\xb0\bC");
  strcpy(maxTemp, "M.:");
  dtostrf(eedata.tempMax, 3, 1, maxTempValue);
  strcat(maxTemp, maxTempValue);
  strcat(maxTemp, "\xb1");
  dtostrf(eedata.tempRange, 3, 1, maxTempValue);
  strcat(maxTemp, maxTempValue);
  strcat(maxTemp, "\xb0\bC");
  if (relayState) {
    strcpy(state, "Wlacz.  ");
  } else {
    strcpy(state, "Wylacz. ");
  }
  strcat(state, "C.");
  sprintf(freq, "%s%ds", state, eedata.freq / 1000);
  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0,0,128,15);
    u8g2.setDrawColor(2);
    u8g2.setFontMode(1);
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(0, 10, "Przelacznik");
    u8g2.drawStr(0, 26, currTemp);
    u8g2.drawStr(0, 42, maxTemp);
    u8g2.drawStr(0, 58, freq);
  } while ( u8g2.nextPage() );
}

void displayMenu() {
  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 16 * (encoderPos%4), 128, 15);
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.setDrawColor(2);
    u8g2.setFontMode(1);
    for(int i=0; i<4; i++){
      u8g2.drawStr(0, 10 + (i * 16), menuOpts[i]);
    }
  } while ( u8g2.nextPage() );
}

void displaySubMenu() {
  u8g2.firstPage();
  if(subMenu != 3){
    char msg[25];
    char value[25];
    char buff[9];
    strcpy(value, "- ");
    if(subMenu == 0){
      strcpy(msg,"3-34\xb1\b1s");
      sprintf(buff, "%d", posToFreq()/1000);
      strcat(value, buff);
      strcat(value, "s");
    } else if (subMenu == 1){
      strcpy(msg,"0.0-25.5\xb1\b0.1\xb0\bC");
      dtostrf(posToTemp(), 3, 1, buff); 
      strcat(value, buff);
      strcat(value, "\xb0\bC");
    } else {
      strcpy(msg,"0.1-1.6\xb1\b0.1\xb0\bC");
      dtostrf(posToRange(), 3, 1, buff);
      strcat(value, buff);
      strcat(value, "\xb0\bC");
    }
    strcat(value, " +");
    do {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 0, 128, 15);
      u8g2.drawBox(0, 32, 128, 15);
      u8g2.setFont(u8g2_font_9x15_tf);
      u8g2.setDrawColor(2);
      u8g2.setFontMode(1);
      
      u8g2.drawStr(0, 10, menuOpts[subMenu]);
      u8g2.drawStr(0, 26, msg);
      u8g2.drawStr(0, 42, value);
      u8g2.drawStr(0, 58, "Nac., aby zap.");
    } while ( u8g2.nextPage() );
  } else {
    do {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, 0, 128, 15);
      u8g2.drawBox(((encoderPos % 6) / 3) * 64, 16 * ((encoderPos % 3) + 1), 64, 15);
      u8g2.setFont(u8g2_font_9x15_tf);
      u8g2.setDrawColor(2);
      u8g2.setFontMode(1);
      u8g2.drawStr(0, 10, menuOpts[subMenu]);
      u8g2.drawStr(0, 26, "  NIE    TAK  ");
      u8g2.drawStr(0, 42, "  NIE    NIE  ");
      u8g2.drawStr(0, 58, "  NIE    NIE  ");
    } while ( u8g2.nextPage() );
  }
}

byte subMenuToPos(){
  switch(subMenu){
    case 0:
      return (eedata.freq/1000) + 125;
    case 1:
      return (byte) round(eedata.tempMax * 10);
    case 2:
      return (byte) (round(eedata.tempRange * 10) + 127);
    default:
      return 132;
  }
}

unsigned long posToFreq(){
  return (((long) encoderPos % 32) + 3) * 1000;
}

float posToTemp(){
  return (float) encoderPos / 10.0;
}

float posToRange() {
  return (float) ((encoderPos % 16) + 1) / 10.0;
}

void saveEedata(){
  unsigned long tempFreq = eedata.freq;
  float tempTempMax = eedata.tempMax;
  float tempTempRange = eedata.tempRange;
  if(subMenu == 0){
    eedata.freq = posToFreq();
  } else if (subMenu == 1){
    eedata.tempMax = posToTemp();
  } else if (subMenu == 2){
    eedata.tempRange = posToRange();
  } else if (encoderPos % 6 == 3){
    eedata.freq = 6000;
    eedata.tempMax = 16.0;
    eedata.tempRange = 0.5;
  }
  if(eedata.freq != tempFreq || eedata.tempMax != tempTempMax || eedata.tempRange != tempTempRange){
    EEPROM.put(0, eedata);
  }
}

void relaySwitcher(){
  if(temp <= eedata.tempMax - eedata.tempRange){
    relayState = false;
  }
  if(temp >= eedata.tempMax + eedata.tempRange){
    relayState = true;
  }
}
