#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x38, 16, 2);

/*Podlaczenie enkodera*/

const int pinA = 3;                         // Pin obrotu enkodera nr1
const int pinB = 4;                         // Pin obrotu enkodera nr2
const int sw = 5;                           // Pin switcha enkodera

/*Konfiguracja pinow sterujacych: nadmuch, pomiar temperatury i zalaczanie grzania*/

const int lutownicaTemp = A0;               //Pin pomiaru napiecia termopary z lutownicy (po wzmocnieniu LM358)
const int hotairTemp = A1;                  //Pin pomiaru napiecia termopary z hotair (po wzmocnieniu LM358)
const int lutownicaGrzanie = 8;             //Sterowanie triakiem grzalki lutownicy
const int nadmuchPWM = 9;                   //Sterowanie PWM nadmuchu hotair
const int hotairGrzanie = 7;                //Sterowanie triakiem grzalki hotair

int lutEnkPozLicz = 0;                      //Wyzerowanie zmiennej licznika pozycji enkodera "lutownicy"
int hotEnkPozLicz = 0;                      //Wyzerowanie zmiennej licznika pozycji enkodera "hotair"

int nadmEnkPozLicz = 255;                   //Ustawienie zmiennej wypelnienia PWM "na enkoderze" nadmuchu na 100%!!!
int wartosc_pwm_nadm = 255 ;                //Ustawienie zmiennej sterowania wypelnieniem PWM nadmuchu na 100%!!!

int poprzedniPrzycisk = HIGH;               //Ustawienie ostatniego stanu przycisku enkodera na HIGH
int przyciskPozMenu = 0;                    //Ustawienie pozycji menu na 0 (poczatek)

int pinAPoprzedni;                          //Zmienna przechowujaca poprzedni stan pinuA enkodera
int aVal;                                   //Zmienna do porownania stanow pinow A i B
boolean bCW;                                //Zmienna sluzaca rozpoznaniu kierunku obrotow enkodera

int tempGrota = 0;                          //Zmienna przechowujaca odczyt adc termopary grota lutownicy
int outputValue = 0;                        //


int tempPowietrza = 0;                      //Zmienna przechowujaca odczyt adc temperatury hotair


const int P_coef = 4;                       //
const int displayRedrawCount = 100;         //
int cyclesCount = 0;                        //


void setup()
{

  /*Ustawienie stanow i kierunkow portow*/

  pinMode (pinA, INPUT_PULLUP);            //pinA enkodera, wejscie podciagniete do VCC
  pinMode (pinB, INPUT_PULLUP);            //pinB enkodera, wejscie podciagniete do VCC
  pinMode (sw, INPUT_PULLUP);              //przycisk enkodera, wejscie podciagniete do VCC
  pinMode (hotairGrzanie, OUTPUT);         //sterowanie grzaniem hotair, wyjscie
  pinMode (lutownicaGrzanie, OUTPUT);      //sterowanie grzaniem lutownicy, wyjscie


  /*Odczytaj stan pinA, w jakim stanie by nie byl, odzwierciedli ostatni stan pinA*/

  pinAPoprzedni = digitalRead(pinA);

  //   Serial.begin (115200);

  /*Inicjalizacja LCD wraz z ekranem powitalnym*/

  lcd.init();
  lcd.backlight();
  lcd.print("    STACJA      ");
  lcd.setCursor(0, 1);
  lcd.print("  LUTOWNICZA  v2");
  delay(1500);
  lcd.clear();

  /*Ustawienie sterowania triakami grzalek na LOW!!!*/

  digitalWrite(hotairGrzanie, LOW);
  digitalWrite(lutownicaGrzanie, LOW);

}

void loop()
{

  /*Kalibracja dwuetapowa:
    1) {Xi to temp ustawiona, Yi temperatura zmierzona zewnetrznym miernikiem} :   linear fit  {80,48},{100,62},{160,92},{200,123},{300,197}
    daje to nam rownanie:  0.672409 * x - 8.56477, [1]
    2) {Xi to temp ustawiona, Yi temperatura zmierzona zewnetrznym miernikiem} :   linear fit  {100,136},{150,205},{200,268},{222,300}
    daje to nam rownanie:  1.33191 * x + 3.48942,  [2]
    podstawiajac [1] do rownania [2]:  3.48942 + 1.33191 * (0.672409 * x-8.56477)  po uproszczeniu daje to: (0.895588 * x -7.91808) , [3]
  */
  tempGrota = analogRead(lutownicaTemp);                 //Odczytaj wartosc napiecia z termopary lutownicy
  tempGrota = (int)(0.895588 * tempGrota - 7.91808);     //Przelicz temp, uzyto rownania [3]

  /*{Xi to temp ustawiona, Yi temperatura zmierzona zewnetrznym miernikiem} :   linear fit {40,52},{80,95},{100,125},{160,195}
    daje to rownanie:  1.202 * x + 2.56 */
  tempPowietrza = analogRead(hotairTemp);                //Odczytaj wartosc napiecia z termopary hotair
  tempPowietrza = (int)(1.202 * tempPowietrza + 2.56);   //Przelicz temp

  int i;

  /*Termostaty sa skonfigurowane jako zalacz-wylacz*/

  /*Termostat 1*/

  i = lutEnkPozLicz - tempGrota;
  i = i > 0 ? i : 0;

  int sI_out;

  if (i)
    sI_out = HIGH;
  else
    sI_out = LOW;

  digitalWrite(lutownicaGrzanie, sI_out);               //zalacz lub wylacz grzanie lutownicy

  /*Termostat 2*/

  i = hotEnkPozLicz - tempPowietrza;
  i = i > 0 ? i : 0;

  int hG_out;

  if (i)
    hG_out = HIGH;
  else
    hG_out = LOW;

  digitalWrite(hotairGrzanie, hG_out);           //zalacz lub wylacz grzanie hotair

  /*Ustawianie predkosci nadmuchu*/

  i = nadmEnkPozLicz;
  i = i > 0 ? i : 0;
  i = i < 255 ? i : 255;

  wartosc_pwm_nadm = i;

  analogWrite(nadmuchPWM, wartosc_pwm_nadm);           //ustaw wypelnienie pwm nadmuchu


  /*Sprawdzanie enkodera*/

  aVal = digitalRead(pinA);                            //Nierownosc oznacza, ze enkoder jest obracany
  if (aVal != pinAPoprzedni)                           //Jesli sie obraca, nalezy rozpoznac kierunek obrotu
  {                                                    //Rozpoznamy to odczytujac pinB.
    if (digitalRead(pinB) != aVal)                     //Nierownosc oznacza ze, pinA zmienil swoj stan jako pierwszy - kierunek zgodny z obrotem wskazowek zegara
    {
      switch (przyciskPozMenu)
      {
        case 0:
          lutEnkPozLicz += 10;
          break;
        case 1:
          hotEnkPozLicz += 10;
          break;
        case 2:
          nadmEnkPozLicz += 10;
          break;
      }

      bCW = true;

    } else
    { // W przeciwnym razie pierwszy stan zmienil pinB - obroty przeciwne do wskazowek zegara

      bCW = false;

      switch (przyciskPozMenu)
      {
        case 0:
          lutEnkPozLicz -= 10;
          break;
        case 1:
          hotEnkPozLicz -= 10;
          break;
        case 2:
          nadmEnkPozLicz -= 10;
          break;
      }
    }
  }

  int stanPrzycisku = digitalRead(sw);

  if ( stanPrzycisku == LOW && stanPrzycisku != poprzedniPrzycisk )
  {

    //Serial.println ("Pushbutton");

    przyciskPozMenu = przyciskPozMenu == 2 ? 0 : przyciskPozMenu + 1 ;

  }

  poprzedniPrzycisk = stanPrzycisku;





  char str[16];

  char lutSeparator = '-';
  char hotSeparator = '-';
  char pwmSeparator = '=';

  //refresh display and serial output
  if (cyclesCount++ % displayRedrawCount == 0) {  // skips some cycles, otherwise output processing will block input processing

    Serial.print("sIenc = ");
    Serial.print(lutEnkPozLicz);
    Serial.print(" sIThermo = ");
    Serial.print(tempGrota);
    Serial.print(" sI_out = ");
    Serial.print(sI_out);
    Serial.print(" hG_out = ");
    Serial.print(hG_out);
    Serial.print(" hGenc = ");
    Serial.print(hotEnkPozLicz);
    Serial.print(" hGThermo = ");
    Serial.print(tempPowietrza);
    Serial.print(" fanEnc = ");
    Serial.print(nadmEnkPozLicz);
    Serial.print(" hgFan = ");
    Serial.print(wartosc_pwm_nadm);
    Serial.print(" buttonMenuPos = ");
    Serial.println(przyciskPozMenu);

    switch (przyciskPozMenu)
    {
      case 0:
        lutSeparator = '*'; 
        break;
      case 1:
        hotSeparator = '*'; 
        break;
      case 2:
        pwmSeparator = '*'; 
        break;
    }

    sprintf(str, "S=%3d%c%3d       ", tempGrota, lutSeparator, lutEnkPozLicz);
    lcd.setCursor(0, 0);
    lcd.print(str);
    sprintf(str, "H=%3d%c%3d F%c%3d%%", tempPowietrza, hotSeparator, hotEnkPozLicz, pwmSeparator, wartosc_pwm_nadm * 100 / 255);
    lcd.setCursor(0, 1);
    lcd.print(str);
  }


  pinAPoprzedni = aVal;


}
