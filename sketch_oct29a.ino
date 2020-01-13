#define TINY_GSM_MODEM_SIM800
#define ACC 3
#define IGN1 4
#define IGN2 5
#define STARTER 6
#define IMMO 7
#define TahometrPin 2
#define Brake A2
#include <avr/wdt.h>
#include <TinyGsmClient.h>
#include <SoftwareSerial.h>


SoftwareSerial SerialAT(11, 10);   //RX, TX
TinyGsm modem(SerialAT);

int interval = 1;
int stat = 0;
int rpc = 0;

unsigned long Check_time = 0;
float Taho_ChekTime = 0;

int Tahometr_impulse_count;
int RPM;
String carname = "Celsior";
String temp;
int count1 = 0;
float V_brake;

void setup() {
  pinMode(ACC, OUTPUT);
  pinMode(IGN1, OUTPUT);
  pinMode(IGN2, OUTPUT);
  pinMode(STARTER, OUTPUT);
  pinMode(IMMO, OUTPUT);
  pinMode(TahometrPin, INPUT_PULLUP);
  Check_time = millis();
  attachInterrupt(0, TahometrImpulse_on, RISING);
  Serial.begin(9600);
  delay(10);
  SerialAT.begin(9600);
  delay(3000);
  Serial.println("Initializing modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);

  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    // wdt_enable(WDTO_1S);
    while (true);
  }
  Serial.println("Network connected");
  wdt_enable(WDTO_8S);
  
}


float BrakeRead()
{
  float DCC = analogRead(Brake);
  return (DCC);
}

int Calc_RPM() {

  detachInterrupt(0); // запретили считать, на всякий случай, шоб не рыпался
  Taho_ChekTime = millis() - Check_time; // время между снятиями показаний счетчика

  if (Tahometr_impulse_count > 0) { // и наличия насчитанных импульсов (на нолик делить не желательно.. будут запредельные значения в RPM)
    RPM = (1000 / Taho_ChekTime) * (Tahometr_impulse_count * 30); // (1000/Taho_ChekTime) - вычисляем множитель (1 секунду делим на наше "замерочное" время) и умножаем на (Tahometr_impulse_count*30) - тут можно было написать *60/2 (* 60 секунд, т.к. у нас есть количество вспышек катушки в секунду и / на 2 т.к. на 1 оборот коленвала 2 искры для 4ёх цилиндрового дрыгателя), но к чему эти сложности
  } else {
    RPM = 0;            // если нет импульсов за время измерений  то  RPM=0
  }

  Tahometr_impulse_count = 0; //сбросили счетчик.
  Check_time = millis(); // новое время
  attachInterrupt(0, TahometrImpulse_on, RISING); // разрешили считать, RISING или FALLING - определяется на месте, с чем стабильней
  return (RPM);
}

void TahometrImpulse_on() {
  Tahometr_impulse_count++;
}

void enginestart()
{
  detection();
  if ((Calc_RPM() < 500) && (stat <= 0))
  {
    Voice(11);
    delay(1000);
    wdt_reset();
    Serial.println ("Запуск двигателя");
    digitalWrite(IMMO, HIGH), delay (1000);
    wdt_reset();
    digitalWrite(ACC, HIGH), delay (1000);
    digitalWrite(IGN1, HIGH), delay (1000);
    wdt_reset();
    digitalWrite(IGN2, HIGH), delay (1000);
    digitalWrite(STARTER, HIGH), delay (1000);
    digitalWrite(STARTER, LOW), delay (3000);
    wdt_reset();
    //detection();

    if (Calc_RPM() > 500) {
      Serial.println ("Двигатель запущен");
      Serial.print("Обороты двигателя: ");
      Serial.println(Calc_RPM());
      stat = 1;
      delay (200);
      Voice(5); // двигатель запущен
      wdt_reset();
      goto  end_loop;

    }
    if (Calc_RPM() < 500)
    {
      Serial.println ("Запуск не удался");
      Serial.print("Обороты двигателя: ");
      Serial.println(Calc_RPM());
      wdt_reset();
      digitalWrite(IGN2, LOW), delay (200);
      digitalWrite(IGN1, LOW), delay (200);
      digitalWrite(ACC, LOW), delay (200);
      digitalWrite(IMMO, LOW), delay (200);
      wdt_reset();
      stat = 0;
      Voice(6); // запуск не удался
      wdt_reset();
      goto  end_loop;
    }
    Serial.println ("Выход из запуска 1") ;

  }
  else if ((RPM > 500) && (stat = 1))
  {
    Serial.println ("Двигатель уже запущен");
    Serial.println ("Выход из запуска 2") ;
    wdt_reset();
    Voice(8); // двигатель уже запущен
    wdt_reset();
  }
end_loop:;
}

void enginestop() {
  if ((RPM > 500) && (stat >= 1))
  {
    Serial.println ("Остановка двигателя") ;
    Voice(10), delay (1000);
    wdt_reset();
    digitalWrite(IGN2, LOW), delay (200);
    digitalWrite(IGN1, LOW), delay (200);
    digitalWrite(ACC, LOW), delay (200);
    digitalWrite(IMMO, LOW), delay (200);
    stat = 0;

  }
  if (RPM < 600) {
    Serial.println ("Двигатель не запущен");
  }
}

void brake_check()
{
  if ((BrakeRead() > 900) && (stat >= 1))
  {
    enginestop();
  }
}

void detection()
{
  brake_check();
  rpc = Calc_RPM();
  if (rpc > 500)
  {
    Serial.print ("Обороты двигателя: ");
    Serial.println (rpc);
    stat = 1;
  }
  if (rpc < 500)
  {
    Serial.print ("Обороты двигателя: ");
    Serial.println (rpc);
    stat = 0;
  }

  Serial.print ("Статус двигателя: ");
  Serial.println (stat);

}

void loop()
{

  wdt_reset();
  detection();
  checkring();

}


String ReadGSM() {
  int c;
  String v;
  while (SerialAT.available()) {
    c = SerialAT.read();
    v += char(c);
    delay(10);
  }
  return v;
}

void checkring()
{

  if (SerialAT.find("RING")) { // если нашли RING
    detection();
    wdt_reset();
    Serial.println("RING!");
    SerialAT.println("AT+DDET=1"); // включаем DTMF
    delay(2000);
    SerialAT.println("ATA"); // поднимаем трубку
    delay(2000);
    SerialAT.println("AT+CREC=4,\"C:\\User\\1.amr\",0,90");
    while (1) {
      wdt_reset();
      temp = ReadGSM();
      delay(500);
      if (temp == "\r\n+DTMF: 1\r\n")
      {
        delay(500);
        detection();
        wdt_reset();
        enginestart();
        break;
      }
      else if (temp == "\r\n+DTMF: 2\r\n")
      {
        delay(500);
        detection();
        wdt_reset();
        enginestop();
        break;
      }
      else if (temp == "\r\nNO CARRIER\r\n") { // если пришел отбой -выходим из цикла
        wdt_reset();
        Serial.println("OK");
        break;
      }
    }
  }
}

void Voice(int Track) {
  SerialAT.print("AT+CREC=4,\"C:\\User\\"), SerialAT.print(Track), SerialAT.println(".amr\",0,95");
}
