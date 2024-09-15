//**********************************************************************************************************************************
#define NTP_OFFSET   3*60*60        // В секундах часовой пояс МСК +3
#define NTP_INTERVAL 60 * 1000      // В миллисекундах
#define NTP_ADDRESS  "ntp3.ntp-servers.net"

#define DAWN_TIME 20    // продолжительность рассвета (в минутах) 
#define ALARM_TIMEOUT 15*60  // таймаут на автоотключение будильника, секунды  

#define DAWN_MIN 0    // начальная яркость лампы (0 - 255) (для сетевых матриц начало света примерно с 50)
#define DAWN_MAX 240  // максимальная яркость лампы (0 - 255)
#define RGBfunctions_D_MAX 160

#define LED_BRIGHT 200   // яркость светодиода индикатора (0 - 255)

#define EEPROM_SIZE 3

#define DNS_PORT = 53
// ************ ПИНЫ ******************************************************************************************************

#define R_PIN 26
#define G_PIN 27
#define B_PIN 25

#define LED_PIN 2          // светодиод индикатор HA ESP

// ***************** БИБЛИОТЕКИ *********************************************************************************
#include <IRremote.h>        // Подключаем библиотеку для работы с ИК-сигналом.
#define RECEIVE_PIN  5         // Сигнал с ИК-приемника поступает на 2-ой пин ардуино.
IRrecv irrecv(RECEIVE_PIN);
decode_results results;


#include <EncButton.h>
Button btn(35);

IRAM_ATTR void isr() {
  btn.pressISR();
}

#include <NTPClient.h>  //время
#include <TimeLib.h>
#include <WiFiUdp.h>  //время по вайфаю

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <EEPROM.h>

#include "GyverTimer.h"
//GTimer_ms timeoutTimer(15000);
GTimer_ms dutyTimer((long)DAWN_TIME * 60 * 1000 / (DAWN_MAX - DAWN_MIN));
GTimer_ms alarmTimeout((long)ALARM_TIMEOUT * 1000);

// ***************** ОБЪЕКТЫ И ПЕРЕМЕННЫЕ *********************************************************************************

const char* ssid = "xxxxxx";
const char* password = "xxxxxxx";

uint32_t btnTimer = 0;

// Setting PWM frequency, channels and bit resolution
const int frequency = 10000;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
const int resolution = 8;

boolean alarmFlag = true;  //minuteFlag deleted
int8_t hrs = 21, mins = 55, secs;
int8_t alm_hrs, alm_mins;
int8_t dwn_hrs, dwn_mins;
byte mode;

volatile boolean dawn_start = false;
volatile boolean dawn_start_IR = false;
volatile boolean LampF = false; 
volatile boolean alarmRun = false;
volatile boolean RGBlightsF = false; 
volatile int duty;




// ***************** wifi *************************************************************************
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// use first 3 channels of 16 channels (started from zero)
// #define LEDC_CHANNEL_0_R    5
// #define LEDC_CHANNEL_1_G    6
// #define LEDC_CHANNEL_2_B    7
// #define LEDC_CHANNEL_3_Br   8
#define LEDC_CHANNEL_4_LED  3
// use bit precission for LEDC timer
#define LEDC_TIMER  8
// use Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     10000
// ***************** wifi *************************************************************************







// ***************** webServer *********************************************************************************
WebServer webServer(80);

String webpage = ""
"<!DOCTYPE html><html><head><title>RGB control</title><meta name='mobile-web-app-capable' content='yes' />"
"<meta name='viewport' content='width=device-width' /></head><body style='margin: 0px; padding: 0px;'>"
"<canvas id='colorspace'></canvas></body>"
"<script type='text/javascript'>"
"(function () {"
" var canvas = document.getElementById('colorspace');"
" var ctx = canvas.getContext('2d');"
" function drawCanvas() {"
" var colours = ctx.createLinearGradient(0, 0, window.innerWidth, 0);"
" for(var i=0; i <= 360; i+=10) {"
" colours.addColorStop(i/360, 'hsl(' + i + ', 100%, 50%)');"
" }"
" ctx.fillStyle = colours;"
" ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);"
" var luminance = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);"
" luminance.addColorStop(0, '#ffffff');"
" luminance.addColorStop(0.05, '#ffffff');"
" luminance.addColorStop(0.5, 'rgba(0,0,0,0)');"
" luminance.addColorStop(0.95, '#000000');"
" luminance.addColorStop(1, '#000000');"
" ctx.fillStyle = luminance;"
" ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);"
" }"
" var eventLocked = false;"
" function handleEvent(clientX, clientY) {"
" if(eventLocked) {"
" return;"
" }"
" function colourCorrect(v) {"
" return Math.round(1023-(v*v)/64);"
" }"
" var data = ctx.getImageData(clientX, clientY, 1, 1).data;"
" var params = ["
" 'r=' + colourCorrect(data[0]),"
" 'g=' + colourCorrect(data[1]),"
" 'b=' + colourCorrect(data[2])"
" ].join('&');"
" var req = new XMLHttpRequest();"
" req.open('POST', '?' + params, true);"
" req.send();"
" eventLocked = true;"
" req.onreadystatechange = function() {"
" if(req.readyState == 4) {"
" eventLocked = false;"
" }"
" }"
" }"
" canvas.addEventListener('click', function(event) {"
" handleEvent(event.clientX, event.clientY, true);"
" }, false);"
" canvas.addEventListener('touchmove', function(event){"
" handleEvent(event.touches[0].clientX, event.touches[0].clientY);"
"}, false);"
" function resizeCanvas() {"
" canvas.width = window.innerWidth;"
" canvas.height = window.innerHeight;"
" drawCanvas();"
" }"
" window.addEventListener('resize', resizeCanvas, false);"
" resizeCanvas();"
" drawCanvas();"
" document.ontouchmove = function(e) {e.preventDefault()};"
" })();"
"</script></html>";
// ***************** webServer *********************************************************************************








void handleRoot() {
String red_pin = webServer.arg(0); 
String green_pin = webServer.arg(1);
String blue_pin = webServer.arg(2);

if((red_pin != "") && (green_pin != "") && (blue_pin != ""))
{ 
  ledcWrite(redChannel, 1023 - red_pin.toInt());
  ledcWrite(greenChannel, 1023 - green_pin.toInt());
  ledcWrite(blueChannel, 1023 - blue_pin.toInt());
}



Serial.print("Red: ");
Serial.println(red_pin.toInt()); 
Serial.print("Green: ");
Serial.println(green_pin.toInt()); 
Serial.print("Blue: ");
Serial.println(blue_pin.toInt()); 
Serial.println();

webServer.send(200, "text/html", webpage);
}



//______________________________________________________________Сетап_______________________________________________________


void setup() {
  ledcSetup(redChannel, frequency, resolution);
  ledcAttachPin(R_PIN, redChannel);
  ledcSetup(greenChannel, frequency, resolution);
  ledcAttachPin(G_PIN, greenChannel);
  ledcSetup(blueChannel, frequency, resolution);
  ledcAttachPin(B_PIN, blueChannel);


  // ledcSetup(LEDC_CHANNEL_0_R, LEDC_BASE_FREQ, LEDC_TIMER);
  // ledcAttachPin(R_PIN, LEDC_CHANNEL_0_R);
  // ledcSetup(LEDC_CHANNEL_1_G, LEDC_BASE_FREQ, LEDC_TIMER);
  // ledcAttachPin(G_PIN, LEDC_CHANNEL_1_G);
  // ledcSetup(LEDC_CHANNEL_2_B, LEDC_BASE_FREQ, LEDC_TIMER);
  // ledcAttachPin(B_PIN, LEDC_CHANNEL_2_B);

  ledcSetup(LEDC_CHANNEL_4_LED, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_4_LED);

  btn.setBtnLevel(LOW);

  attachInterrupt(0, isr, FALLING);

  irrecv.enableIRIn();       // Включить приемник.


  Serial.begin(115200);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());


  webServer.on("/", handleRoot);
  webServer.begin();

  EEPROM.begin(EEPROM_SIZE);  //Init EEPROM

  while (year() < 2021)
  {
    timeClient.update();
    String formattedTime = timeClient.getFormattedTime();
    long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
    setTime(unixtime);
    secs = second();
    mins = minute();
    hrs  = hour();
    int8_t numweek = ymdToWeekNumber(year(),month(),day());
  }

  timeClient.update();  //контрольный, да хоть и выше есть
  String formattedTime = timeClient.getFormattedTime();
  long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
  setTime(unixtime);
  secs = second();
  mins = minute();
  hrs  = hour();
  int8_t numweek = ymdToWeekNumber(year(),month(),day());

  alarmFlag = EEPROM.read(2);

  alm_hrs = constrain(alm_hrs, 0, 23);  //ОГРАНИЧЕНИЕ
  alm_mins = constrain(alm_mins, 0, 59);

  alarmFlag = constrain(alarmFlag, 0, 1);


 //  if((numweek % 2) == 0)   {alm_hrs = 8; alm_mins = 30;}  //четная неделя(зн)
 //     else                  {alm_hrs = 6; alm_mins = 30; } //нечетная неделя(ЧС)
 //    }
  if      (weekday() == 1) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 2) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 3) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 4) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 5) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 6) {alm_hrs = 8; alm_mins = 30;}
  else if (weekday() == 7) {alm_hrs = 8; alm_mins = 30;}

  EEPROM.write(0, alm_hrs);
  EEPROM.write(1, alm_mins);

  calculateDawn();  // расчёт времени рассвета

}
//----------------------------------------------сетап--------------------------------------------------------------------




//______ЛУП___________ЛУП________ЛУП______________ЛУП_____________ЛУП__________________ЛУП_______________ЛУП_____________

void loop() {

  btn.tick();   // опрос

  if (secs == 0)
  {
    timeClient.update();
    long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
    setTime(unixtime);
  }
  // else long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
  
  secs = second();
  mins = minute();
  hrs  = hour();

//  int8_t numweek = ymdToWeekNumber(year(),month(),day());
//  if((numweek % 2) == 0)   {alm_hrs = 6; alm_mins = 30;}  //четная неделя(зн)
//      else {                alm_hrs = 8; alm_mins = 30; } //нечетная неделя(ЧС)
//    }

  //обновление будильника на след день
  if (hrs == 19 && mins == 30) {
  if      (weekday() == 1) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 2) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 3) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 4) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 5) {alm_hrs = 7; alm_mins = 45;}
  else if (weekday() == 6) {alm_hrs = 8; alm_mins = 30;}
  else if (weekday() == 7) {alm_hrs = 8; alm_mins = 30;}
  EEPROM.write(0, alm_hrs);
  EEPROM.write(1, alm_mins);
  calculateDawn();        // расчёт времени рассвета
  }

  clockTick();    // считаем время
  alarmTick();    // обработка будильника

  if (alarmFlag) {
    ledcWrite(LEDC_CHANNEL_4_LED, LED_BRIGHT);
  } else {
    ledcWrite(LEDC_CHANNEL_4_LED, 0);
  }

  if (dawn_start || alarmRun) colorWheel();  // если рассвет или уже будильник управление лампой (dutytick)
  // if (!dawn_start && !alarmRun && !LampF)   {
  //   ledcWrite(redChannel, 0);
  //   ledcWrite(greenChannel, 0);
  //   ledcWrite(blueChannel, 0);  // если нерассвет или небудильник управление лампой (dutytick)
  // }
  

  EEPROM.commit();


  if (btn.hold()) {       // кнопка удержана
    //Serial.println("hold");   // будет постоянно возвращать true после удержания+++++++++++++++++
    if (dawn_start) {         // если удержана во время рассвета или будильника
      dawn_start = false;     // прекратить рассвет
      alarmRun = false;          // и будильник
      duty = 0;
      colorWheel();
      return;
    }
    if (!dawn_start) {   // кнопка удержана в режиме часов и сейчас не рассвет
      alarmFlag = !alarmFlag;     // переключаем будильник

      EEPROM.write(2, alarmFlag);
      //delay(1000);
    } 
    calculateDawn();
  }

  if (btn.click() && !LampF && millis() - btnTimer > 100) {  // обработчик нажатия
    LampF = true;
    btnTimer = millis();
    Serial.println("click1");
  ledcWrite(redChannel, 606);
  ledcWrite(greenChannel, 86);
  ledcWrite(blueChannel, 15);
  }
  if (btn.click() && LampF && millis() - btnTimer > 100) {  // обработчик отпускания
    LampF = false;
        btnTimer = millis();
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 0);
   // Serial.println("click2");
  }


 //____________________________rgb_functions________________________

  if (btn.releaseStep())
  { 
    RGBlightsF = !RGBlightsF;
    ledcWrite(redChannel, 0);
    ledcWrite(greenChannel, 0);
    ledcWrite(blueChannel, 0);
  }

  if (RGBlightsF)
  {
    // плавно мигаем яркостью
    static uint32_t tmr;
    if (millis() - tmr >= 1000) {
      tmr = millis();
      static int8_t dir = 1;
      static int val = 0;
      val += dir;
      if (val == 1530 || val == 0) dir = -dir;   // разворачиваем
      rgbWheel(val);
    }
  }








  if (dawn_start_IR)  {
      if (dutyTimer.isReady()) {    // поднимаем яркость по таймеру
        duty++;
      if (duty > DAWN_MAX) duty = DAWN_MAX;
    }
    colorWheel();
    
  }

 //____________________________IR_functions________________________

  if (irrecv.decode(&results)) {       // Если сигнал с пульта пришел
    if (results.value == 0x73FCAA47) {    // И если значение равно ВКЛ
      LampF = true;
      ledcWrite(redChannel, 606);
      ledcWrite(greenChannel, 86);
      ledcWrite(blueChannel, 15);
      
    }
    if (results.value == 0x42B17343) {    // Если значение равно ВЫКЛ

      RGBlightsF = false;

      LampF = false;
      ledcWrite(redChannel, 0);
      ledcWrite(greenChannel, 0);
      ledcWrite(blueChannel, 0);

      if (dawn_start) {         // если во время рассвета или будильника
        dawn_start = false;     // прекратить рассвет
        alarmRun = false;          // и будильник
        duty = 0;
        colorWheel();
        return;
      }
    }

    if (results.value == 0xE0E043BC) {    // И если значение равно Тусклее
      LampF = true;
      ledcWrite(redChannel, 7);
      ledcWrite(greenChannel, 2);
      ledcWrite(blueChannel, 0);
    }    

    if (results.value == 0xE0E0BF40) {    // И если значение равно Радуга
      RGBlightsF = !RGBlightsF;
    }    

    if (results.value == 0xE0E0A15E) {    // И если значение равно Рассвет
      dawn_start_IR = !dawn_start_IR;
    }
        

    if (results.value == 0x53DFD887) {    // И если значение равно Переключи будильник
      alarmFlag = !alarmFlag;     // переключаем будильник
      EEPROM.write(2, alarmFlag);
    }    




    irrecv.resume();                   // Готовность принять следующий сигнал.
  }
  
 //____________________________IR_functions________________________



 webServer.handleClient();
}





void colorWheel() {  //счетчик для установления нужного цвета и яркости
  if (duty <= 160) {
    setHSV(  10 + duty / 30, 255 - (2*duty / 6), duty/3 );
  }   else if (duty > 160 && duty <= 193) {  //макс наш свет
    setHSV(  15, 205, ( 3.54*duty - 514 )  );
  }  else if (duty > 193 && duty <= 250 ) {  //макс наш свет
    setHSV(  15, 205, ( 1.37*duty - 94  )  );
  }  else if (duty > 250) {  //макс наш свет
    setHSV(  15, 205, 255 );
  }
}

void rgbWheel(int16_t color) {
  int16_t _r, _g, _b;
  if (color <= 255) {                       // красный макс, зелёный растёт
    _r = 255;
    _g = color;
    _b = 0;
  }
  else if (color > 255 && color <= 510) {   // зелёный макс, падает красный
    _r = 510 - color;
    _g = 255;
    _b = 0;
  }
  else if (color > 510 && color <= 765) {   // зелёный макс, растёт синий
    _r = 0;
    _g = 255;
    _b = color - 510;
  }
  else if (color > 765 && color <= 1020) {  // синий макс, падает зелёный
    _r = 0;
    _g = 1020 - color;
    _b = 255;
  }
  else if (color > 1020 && color <= 1275) {   // синий макс, растёт красный
    _r = color - 1020;
    _g = 0;
    _b = 255;
  }
  else if (color > 1275 && color <= 1530) { // красный макс, падает синий
    _r = 255;
    _g = 0;
    _b = 1530 - color;
  }
  // analogWrite(R_PIN, 255 - _r); /255*1023
  // analogWrite(G_PIN, 255 - _g); /255*1023
  // analogWrite(B_PIN, 255 - _b);
  
  
  _r = constrain(_r, 0, RGBfunctions_D_MAX);  //ОГРАНИЧЕНИЕ
  _g = constrain(_g, 0, RGBfunctions_D_MAX);  //ОГРАНИЧЕНИЕ
  _b = constrain(_b, 0, RGBfunctions_D_MAX);  //ОГРАНИЧЕНИЕ

  ledcWrite(redChannel,   _r);
  ledcWrite(greenChannel, _g);
  ledcWrite(blueChannel,  _b);

  Serial.print("HSV Red:   ");
  Serial.println(_r); 
  Serial.print("HSV Green: ");
  Serial.println(_g); 
  Serial.print("HSV Blue:  ");
  Serial.println(_b); 
  Serial.println();
}

void calculateDawn() {
  // расчёт времени рассвета
  if (alm_mins > DAWN_TIME) {                // если минут во времени будильника больше продолжительности рассвета
    dwn_hrs = alm_hrs;                       // час рассвета равен часу будильника
    dwn_mins = alm_mins - DAWN_TIME;         // минуты рассвета = минуты будильника - продолж. рассвета
  } else {                                   // если минут во времени будильника меньше продолжительности рассвета
    dwn_hrs = alm_hrs - 1;                   // значит рассвет будет часом раньше
    if (dwn_hrs < 0) dwn_hrs = 23;           // защита от совсем поехавших
    dwn_mins = 60 - (DAWN_TIME - alm_mins);  // находим минуту рассвета в новом часе
  }
}

void clockTick() {
  if (dwn_hrs == hrs && dwn_mins == mins && alarmFlag && !dawn_start) {
    duty = DAWN_MIN;
    dawn_start = true;
  }
  if (alm_hrs == hrs && alm_mins == mins && alarmFlag && dawn_start && !alarmRun) {
    alarmRun = true;
    alarmTimeout.reset();
  }
}

void alarmTick() {
  if (dawn_start && alarmFlag) {
    if (dutyTimer.isReady()) {    // поднимаем яркость по таймеру
      duty++;
      if (duty > DAWN_MAX) duty = DAWN_MAX;
    }
  }
  if (alarmRun) {                    // настало время будильника
    if (alarmTimeout.isReady()) { // таймаут будильника
      dawn_start = false;         // прекратить рассвет
      alarmRun = false;              // и будильник
      duty = 0;
      ledcWrite(redChannel, 0);
      ledcWrite(greenChannel, 0);
      ledcWrite(blueChannel, 0);
    }
  }
}


// включить цвет НА ПИНАХ в HSV, принимает 0-255 по всем параметрам_________________________________________________________________
void setHSV(uint8_t h, uint8_t s, uint8_t v) {
  float r, g, b;
  uint8_t _r, _g, _b;


  float H = (float)h / 255;
  float S = (float)s / 255;
  float V = (float)v / 255;

  int i = int(H * 6);
  float f = H * 6 - i;
  float p = V * (1 - S);
  float q = V * (1 - f * S);
  float t = V * (1 - (1 - f) * S);

  switch (i % 6) {
    case 0: r = V, g = t, b = p; break;
    case 1: r = q, g = V, b = p; break;
    case 2: r = p, g = V, b = t; break;
    case 3: r = p, g = q, b = V; break;
    case 4: r = t, g = p, b = V; break;
    case 5: r = V, g = p, b = q; break;
  }
  _r = r * 255;
  _g = g * 255;
  _b = b * 255;

  _r = constrain(_r, 0, 255);  //ОГРАНИЧЕНИЕ
  _g = constrain(_g, 0, 255);  //ОГРАНИЧЕНИЕ
  _b = constrain(_b, 0, 255);  //ОГРАНИЧЕНИЕ

  ledcWrite(redChannel,   _r);
  ledcWrite(greenChannel, _g);
  ledcWrite(blueChannel,  _b);

  // Serial.print("HSV Red:   ");
  // Serial.println(_r); 
  // Serial.print("HSV Green: ");
  // Serial.println(_g); 
  // Serial.print("HSV Blue:  ");
  // Serial.println(_b); 
  // Serial.println();
}

int ymdToWeekNumber (int y, int m, int d) {
  // reject out-of-range input
  if ((y < 1965)||(y > 2099)) return 0;
  if ((m < 1)||(m > 12)) return 0;
  if ((d < 1)||(d > 31)) return 0;
  // compute correction for year   If Jan. 1 falls on: Mo Tu We Th Fr Sa Su
  // then the correction is:  0 +1 +2 +3 -3 -2 -1
  int corr = ((((y - 1965) * 5) / 4) % 7) - 3;
  // compute day of the year (in range 1-366)
  int doy = d;
  if (m > 1) doy += 31;
  if (m > 2) doy += (((y%4)==0) ? 29 : 28);
  if (m > 3) doy += 31;
  if (m > 4) doy += 30;
  if (m > 5) doy += 31;
  if (m > 6) doy += 30;
  if (m > 7) doy += 31;
  if (m > 8) doy += 31;
  if (m > 9) doy += 30;
  if (m > 10) doy += 31;
  if (m > 11) doy += 30;
  // compute corrected day number
  int cdn = corr + doy;
  // check for boundary conditions if our calculation would give us "week 53",
  // we need to find out whether week 53 really exists, or whether it is week 1 of the following year
  if (cdn > 364) {
    // check for year beginning on Thurs.
    if (corr==3) return 53;
    // check for leap year beginning on Wed.
    if (((y%4)==0) && (corr==2)) return 53;
    // otherwise, there is no week 53
    return 1;
  }
  // if our calculation would give us "week 0", then go to the previous year
  // and find out whether we are in week 52 or week 53
  if (cdn < 1) {
    // first, compute correction for the previous year
    corr = ((((y - 1966) * 5) / 4) % 7) - 3;
    // then, compute day of year with respect to that same previous year
    doy = d + (((y%4)==1)?366:365);
    // finally, re-compute the corrected day number
    cdn = corr + doy;
  }
  // compute number of weeks, rounding up to nearest whole week
  return ((cdn + 6) / 7);
}
