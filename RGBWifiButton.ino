#include <WiFi.h>
#include <EncButton.h>
Button btn(35);

IRAM_ATTR void isr() {
  btn.pressISR();
}
#include <WebServer.h>


const char* ssid = "xxxxxx";
const char* password = "xxxxxxx";

const byte DNS_PORT = 53;
const int red_pin = 26;   
const int green_pin = 27; 
const int blue_pin = 25;  

uint32_t btnTimer = 0;

// Setting PWM frequency, channels and bit resolution
const int frequency = 10000;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
const int resolution = 8;

bool flag = false;


WebServer webServer(80);

String webpage = ""
"<!DOCTYPE html><html><head><title>RGB control</title><meta name='mobile-web-app-capable' content='yes' />"
"<meta name='viewport' content='width=device-width' /></head><body style='margin: 0px; padding: 0px;'>"
//  "<form name='TimeForm'>"
//     "<table width='20%' bgcolor='A09F9F' align='center'>"
//         "<tr>"
//             "<td colspan=2>"
//                 "<center><font size=4><b>ESP32 Time Choose Page</b></font></center>"
//                 "<br>"
//             "</td>"
//             "<br>"
//             "<br>"
//         "</tr>"
//         "<tr>"
//              "<td>Hours:</td>"
//              "<td><input type='int' name='hours'><br></td>"
//         "</tr>"
//         "<br>"
//         "<br>"
//         "<tr>"
//             "<td>Mins:</td>"
//             "<td><input type='int' name='minutes'><br></td>"
//             "<br>"
//             "<br>"
//         "</tr>"
//         "<tr>"
//             "<td><input type='submit' 'onclick='check(this.form)' value='         TimeClick'></td>"
//         "</tr>"
//     "</table>"
// "</form>"
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


void setup() {
  ledcSetup(redChannel, frequency, resolution);
  ledcSetup(greenChannel, frequency, resolution);
  ledcSetup(blueChannel, frequency, resolution);
 
  ledcAttachPin(red_pin, redChannel);
  ledcAttachPin(green_pin, greenChannel);
  ledcAttachPin(blue_pin, blueChannel);

  attachInterrupt(0, isr, FALLING);


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
}

void loop() {
  btn.tick();   // опрос
  // читаем инвертированное значение для удобства
  if (btn.click() && !flag && millis() - btnTimer > 100) {  // обработчик нажатия
    flag = true;
    btnTimer = millis();
    Serial.println("click1");
  ledcWrite(redChannel, 606);
  ledcWrite(greenChannel, 86);
  ledcWrite(blueChannel, 15);
  }
  if (btn.click() && flag && millis() - btnTimer > 100) {  // обработчик отпускания
    flag = false;
        btnTimer = millis();
  ledcWrite(redChannel, 0);
  ledcWrite(greenChannel, 0);
  ledcWrite(blueChannel, 0);
    Serial.println("click2");
  }

webServer.handleClient();
}
