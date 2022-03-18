#include <ModbusMaster.h>               //Library for using ModbusMaster
#include <PS2Keyboard.h>
#include <EEPROM.h>

#define DataPin    2
#define IRQpin     3
#define MAX485_RSE 6

ModbusMaster node;              //object node for class ModbusMaster
PS2Keyboard keyboard;

int32_t value1, value2; //уставки
int32_t a1, a2; //текущая цифра с клавиатуры
uint16_t dif, difOld; //разница уставок
uint8_t d0, d1; //старший и младший байты разницы для записи в EEPROM
int difAdr = 10; //адрес разницы в EEPROM
boolean f1; //флаг записи цифры в переменную
boolean f2; //флаг разрешения записи по клавише энтер
boolean f_zvezda = 0; //флаг нажатия звездочки, используется как точка
boolean fDif = 0; //флаг настройки разницы уставок
boolean fParol = 0; //флаг настройки разницы уставок
char c; //симфол с клавиатуры
int parolCount;
int zvCount = 0;
int zvCount2 = 0;
unsigned long t1,t2; //время для задания пароля

//Function for setting stste of Pins DE & RE of RS-485
void preTransmission() {
  digitalWrite(MAX485_RSE, 1);
}
void postTransmission() {
  digitalWrite(MAX485_RSE, 0);
}

//функция записи разницы уставок
void setDif() {
  parolCount = 0;
  fParol = 0;
  zvCount2 = 0;
  //  zvCount = 0;
  fDif = 1;
  f_zvezda = 0;
  f1 = 0;
  f2 = 0;
  difOld = dif;
  dif = 0;
  uint16_t diftemp = 0;
  while (fDif == 1) {
    if (keyboard.available()) {
      // read the next key
      c = keyboard.read();
      f1 = 1;
      //      f2 = 1;
    }
    if (c == PS2_DELETE) {
      dif = difOld;
      fDif = 0;
      zvCount2 = 0;
      f_zvezda = 0;
      c = 'a';
    }
    if (c == '*') {
      f_zvezda = 1;
      //      zvCount2++;
      c = 'a';
    }
    char  c2 = c;
    if ((isDigit(c2)) && (f1 == 1)) {
      a2 = c2 - '0'; //преобразование char в int
      if ((diftemp < 1000) && (zvCount2 < 1)) diftemp = diftemp * 10 + a2;
      if ((f_zvezda == 1) && (zvCount2 == 0)) zvCount2++;
      f1 = 0;
    }
    if (c == PS2_ENTER) {
      if (dif > 1000) dif = 1000;
      if (f_zvezda == 1) {
        dif = diftemp;
      } else {
        dif = diftemp * 10;
      }
      d0 = dif & 0xff;
      d1 = dif >> 8;
      EEPROM.update(10, d0);
      EEPROM.update(11, d1);
      fDif = 0;
      f1 = 0;
      zvCount2 = 0;
      f_zvezda = 0;
      c = 'a';
    }
    delay(20);
  }
}

void setup() {
  d0 = EEPROM.read(10);
  d1 = EEPROM.read(11);
  dif = (d1 << 8) + d0;
  keyboard.begin(DataPin, IRQpin);
  value1 = 0;

  pinMode(MAX485_RSE, OUTPUT);
  digitalWrite(MAX485_RSE, 0);

  Serial.begin(9600);             //Baud Rate as 9600

  node.begin(16, Serial);         //Slave ID as 16
  //Callback for configuring RS-485 Transreceiver correctly
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

void loop() {
  if (keyboard.available()) {
    // read the next key
    c = keyboard.read();
    f1 = 1;
    f2 = 1;
  }
  if (c==PS2_ESC) {
    node.writeSingleCoil(0x0002,1);
    c='a';
  }
  
  if (c == PS2_DELETE) {
    value1 = 0;
    value2 = 0;
    parolCount = 0;
    zvCount = 0;
    f2 = 1;
    f_zvezda = 0;
    
    
    //    c = '\0';
    c = 'a';
  }
  if (c == '*') {
    f_zvezda = 1;
    c = 'a';
  }
  if (c == '#') {
    if (parolCount==0) t1 = millis(); //
    parolCount++;
    c = 'a';
  }
  if (parolCount == 3) {
    t2=millis();                      //
    if ((t2 - t1) < 3000) fParol = 1; //
  }
  if (fParol == 1) setDif();
  char  c1 = c;
  if ((isDigit(c1)) && (f1 == 1)) {
    //  if ((c ~ = PS2_ENTER) && (c ~ = PS2_DELETE) && (f1 == 1)) {
    a1 = c1 - '0'; //преобразование char в int
    if ((value1 < 6000) && (zvCount < 1)) value1 = value1 * 10 + a1;
    if ((f_zvezda == 1) && (zvCount == 0)) zvCount++;
    f1 = 0;
  }

  if ((c == PS2_ENTER) && (f2 == 1)) {
    if (value1 > 6000) value1 = 6000;
    if (f_zvezda == 0) {
      value1 = value1 * 10;
      if (value1 > dif) {
        value2 = value1 - dif;
      } else {
        value2 = 0;
      }
    }
    if ((f_zvezda == 1) && (value1 > dif)) {
      value2 = value1 - dif;
    } else if (f_zvezda == 1) {
      value2 = 0;
    }

    //запись уставок
    // set word 0 of TX buffer to least-significant word of counter (bits 15..0)
    node.setTransmitBuffer(0, lowWord(value1));
    // set word 1 of TX buffer to most-significant word of counter (bits 31..16)
    node.setTransmitBuffer(1, highWord(value1));
    // slave: write TX buffer to (2) 16-bit registers starting at register 12
    node.writeMultipleRegisters(0x4000D, 2);
    //        node.writeSingleRegister(0x4000D,dif);   //Writes value to 0x40000 holding register
    delay(100);

    // set word 0 of TX buffer to least-significant word of counter (bits 15..0)
    node.setTransmitBuffer(0, lowWord(value2));
    // set word 1 of TX buffer to most-significant word of counter (bits 31..16)
    node.setTransmitBuffer(1, highWord(value2));
    // slave: write TX buffer to (2) 16-bit registers starting at register 14
    node.writeMultipleRegisters(0x4000F, 2);

    f2 = 0;
    f_zvezda = 0;
    value1 = 0;
    value2 = 0;
    parolCount = 0;
    zvCount = 0;
    c = 'a';
    //    node.writeSingleRegister(0x40000,value);   //Writes value to 0x40000 holding register
    //    node.writeSingleRegister(0x40002, 0);      //Writes 0 to 0x40001 holding register
  }
  delay(50);
}
