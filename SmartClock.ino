/**The MIT License (MIT)

Copyright (c) 2016 by Rene-Martin Tudyka
Based on the SSD1306 OLED library Copyright (c) 2015 by Daniel Eichhorn (http://blog.squix.ch),
available at https://github.com/squix78/esp8266-oled-ssd1306

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/
#include <Wire.h>
#include <SPI.h>
#include "SH1106.h"
#include "SH1106Ui.h"
#include <ArduinoJson.h>
#include "time.h"
#include <NtpClientLib.h>
#include <ESP8266WiFi.h>


//Sensor de presencia
#ifdef ESP8266
  #define Presencia_GPIO D2
#else
  #define Presencia_GPIO 4
#endif

//Constantes de tiempo
#define TZ 3600  //1h timezone in sec
#define DST 0 //1h daylightOffset in sec

//Botones
#define BOTON_MODO D4
#define BOTON_UP D9

//Buzzer
#define BUZZER D6

// OLED
#define OLED_RESET  D1   // RESET
#define OLED_DC     D3   // Data/Command
#define OLED_CS     D8   // Chip select

//Flexor
#define FLEX_GPIO A0

//Variables OLED
SH1106 display(true, OLED_RESET, OLED_DC, OLED_CS);
SH1106Ui ui     ( &display );

//Variables de flexión
bool val = 0;
bool old_val = 0;

//Variables de clima
String APIKEY = "31b679f8aa9183a71cbd5706f41263a2";
String CityID = "6356734"; //Cáceres, España
char servername[]="api.openweathermap.org";  // remote server we will connect to
String result;
String weatherDescription ="";
String weatherLocation = "";
float Temperature;

String Llocation;
String Ltemperature;
String Lweather;
String Ldescription;
String LidString;
String LtimeS;
String Ltmax;
String Ltmin;
boolean night = false;

int TimeZone = 2; //GMT +2
WiFiClient client;

//Variables de Wifi
String YOUR_WIFI_SSID = "";
String YOUR_WIFI_PASSWD = "";
int tiempoEsperaWifi=0;

int remainingTimeBudget;

//Variables de tiempo
int horas=0;
int minutos=0;
int segundos=0;
bool alarma = false;
bool temp = false;
bool acostado = true;
time_t current_time, last_time;

//Variables de flexión
int flexposition;

//Método que dibuja en el display la información
bool drawFrame1(SH1106 *display, SH1106UiState* state, int x, int y) {
    
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  time(&current_time);
  display->clear();

  //Dibujo del display en la habitación
  if(YOUR_WIFI_SSID == "Habitacion" && (val || !esNoche())){
  display->drawString(0 + x, 5 + y, String(hour(current_time)) + ":" + String(minute(current_time)) + ':' + String(second(current_time)));
  display->drawString(55 + x, 5 + y, "Habitación");
  display->drawString(0 + x, 15 + y, String(day(current_time)) + ":" + String(month(current_time)) + ':' + String(year(current_time)));
  display->drawString(0 + x, 25 + y, "Actual: "+ String(Ltemperature.toInt()) + "º" );
  display->drawString(55 + x, 25 + y, Ldescription);
  if(alarma && acostado){
    display->drawString(0 + x, 45 + y, "Alarma: "+String(horas)+ ":" + String(minutos));
  } 
  if(Ltemperature.toInt()>=20){
    if(Ldescription == "lluvia ligera" ){
      display->drawString(0 + x, 35 + y, "Manga corta y paraguas");
    }else{
      display->drawString(0 + x, 35 + y, "Manga corta");
    }
  }
   else if(Ltemperature.toInt()<20 && Ltmax.toInt()>14){
      if(Ldescription == "lluvia ligera"){
        display->drawString(0 + x, 35 + y, "Manga larga y paraguas");
      }else{
        display->drawString(0 + x, 35 + y, "Manga larga");
      }
   }
    else if(Ltemperature.toInt()<14){
      if(Ldescription == "lluvia ligera"){
        display->drawString(0 + x, 35 + y, "Abrigo y paraguas");
      }else{
        display->drawString(0 + x, 35 + y, "Abrigo");
      }
    }
  }
  //Dibujo del display en la cocina
  else{ if(YOUR_WIFI_SSID == "Cocina"){
    display->drawString(0 + x, 5 + y, String(hour(current_time)) + ":" + String(minute(current_time)) + ':' + String(second(current_time)));
    display->drawString(55 + x, 5 + y, "Cocina");
    display->drawString(0 + x, 15 + y, String(day(current_time)) + ":" + String(month(current_time)) + ':' + String(year(current_time)));
    display->drawString(0 + x, 25 + y, "Actual: "+ String(Ltemperature.toInt()) + "º" );
    
    if(temp){
      display->drawString(0 + x, 35 + y, "Temporizador: " + String(segundos/60) + ":" + String(segundos%60));
    }  
  } 
  }

  return false;
}

//Número de Frames del display
int frameCount = 1;
//Incorporar los Frames al display
bool (*frames[])(SH1106 *display, SH1106UiState* state, int x, int y) = { drawFrame1};

//Módulo que incrementa los minutos
void incrementarMinutos(){
  minutos ++;
  minutos = minutos % 60; 
}
//Módulo que incrementa los segunds como si fueran minutos
void incrementarSegundosMinutos(){
  segundos = segundos + 60; 
}

//Módulo que incrementa las horas
void incrementarHoras(){
  horas ++;
  horas = horas % 24; 
}

//Módulo para comprobar si es de noche
bool esNoche(){
  if(String(hour(current_time))!="8" && String(hour(current_time))!="9" && String(hour(current_time))!="10" && String(hour(current_time))!="11" && String(hour(current_time))!="12" && String(hour(current_time))!="13" && String(hour(current_time))!="14" && String(hour(current_time))!="15" && String(hour(current_time))!="16" && String(hour(current_time))!="17" && String(hour(current_time))!="18" && String(hour(current_time))!="19" && String(hour(current_time))!="20"){
    return true;
  }
  return false;
}

//Módulo para introducir la alarma
void ponerAlarma(){
  int modo = 1;
  horas=0;
  minutos=0;
  while(modo!=3){
    Serial.print(alarma);
    switch(modo){
      case 1: 
        delay(1500);
        while(digitalRead(BOTON_MODO)!=LOW){
          delay(200);
          ui.nextFrame();
          ui.previousFrame();
          remainingTimeBudget = ui.update();
          if(digitalRead(BOTON_UP)==LOW){
            Serial.print("HORAS");
            incrementarHoras();
          }
        }
        modo =2;
      break;
      
    case 2: 
      delay(1500);
      while(digitalRead(BOTON_MODO)!=LOW){
        delay(200);
        ui.nextFrame();
        ui.previousFrame();
        remainingTimeBudget = ui.update();
        if(digitalRead(BOTON_UP)==LOW){
          Serial.print("Minutos");
          incrementarMinutos();
        }
      }
      modo = 3;
      break;
    }
  }
}

//Módulo para introducir el temporizador
void temporizador(){
  minutos=0;
  segundos=0;
    delay(2000);
    while(digitalRead(BOTON_MODO)!=LOW){
        delay(200);
        ui.nextFrame();
        ui.previousFrame();
        remainingTimeBudget = ui.update();
      if(digitalRead(BOTON_UP)==LOW){
        Serial.print("Minutos");
        incrementarSegundosMinutos();
      }
    }
}

//Módulo para detectar los valores del flexor
void detection_phase(){
  flexposition = analogRead(FLEX_GPIO);

    Serial.print(" mapped read flex sensor value: ");
    Serial.println(flexposition);


  if (flexposition>=740){
      acostado=true;
     Serial.println("Acostado!!!");
     
  }else{
    acostado=false;
  }
}

//Módulo para conectar a un punto WIFI
void conectarWIFI(){
  int tiempoEsperaWifi = 0;
  while(WiFi.status() != WL_CONNECTED){
    int n = WiFi.scanNetworks();
  
    if (n == 0)
      Serial.println("no hay redes en el rango de alcance");
    else{
      Serial.print(n);
      Serial.println(" redes en el rango del dispositivo");
    }
  
    for (int i = 0; i < n; ++i)
    {
      if(WiFi.SSID(i).equals("Habitacion")){
        Serial.println("ENTROOOO");
         YOUR_WIFI_SSID="Habitacion";
         YOUR_WIFI_PASSWD="12345678";
      }else if(WiFi.SSID(i).equals("Cocina")){
         YOUR_WIFI_SSID="Cocina";
         YOUR_WIFI_PASSWD="12345678";
      }
    }
  
    WiFi.begin(YOUR_WIFI_SSID.c_str(), YOUR_WIFI_PASSWD.c_str());

    while (WiFi.status() != WL_CONNECTED && tiempoEsperaWifi!=14) {
      Serial.print(".");
      tiempoEsperaWifi++;
      delay(500);
    }
    tiempoEsperaWifi=0;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode (Presencia_GPIO, INPUT);
  pinMode (BOTON_MODO, INPUT_PULLUP);
  pinMode (BOTON_UP, INPUT_PULLUP);
  pinMode (BUZZER, OUTPUT);
  pinMode(FLEX_GPIO, INPUT);
  
  old_val = 0;
  Serial.flush();
  
  WiFi.mode(WIFI_STA);
  
  conectarWIFI();
  configTime(TZ, DST, "es.pool.ntp.org", "pool.ntp.org", "time.nist.gov");

  while (!current_time) {
    time(&current_time);
    Serial.print("-");
    delay(100);
  }
  getWeatherData();
  display.clear();
  ui.disableAutoTransition();
  ui.setTimePerFrame(4000);
  ui.setTimePerTransition(0);
  ui.setFrames(frames, frameCount);
 
  // Inital UI takes care of initalising the display too.
  ui.init();
}

void loop() {
  time(&current_time);

  conectarWIFI();

  if ((current_time != last_time) && ((current_time % 3600) == 0)) {
    Serial.print(ctime(&current_time));
    getWeatherData();
    last_time = current_time;
  }
  
  delay(1000);
  
  if(YOUR_WIFI_SSID == "Habitacion"){
    if(esNoche()){
       val = digitalRead(Presencia_GPIO);
       if (old_val != val){
         if (val)
            Serial.println("DETECTADO CUERPO EN MOVIMIENTO");
         else
            Serial.println("TODO TRANQUILO");
       old_val = val;
      }
    }
    
    Serial.print(digitalRead(BOTON_MODO));
    
    if(digitalRead(BOTON_MODO)==LOW){
      alarma = true;
      acostado = true;
      ponerAlarma();
    }
    
    if(alarma){
      detection_phase();
      if(acostado){
        if((String(hour(current_time)) == String(horas)) && (String(minute(current_time)) == String(minutos))){
          delay(500);
          while(analogRead(FLEX_GPIO)>740){
            analogWrite(BUZZER, 255);
            delay(500);
            analogWrite(BUZZER, 0);
            ui.nextFrame();
            ui.previousFrame();
            remainingTimeBudget = ui.update();
          }
          alarma = false;
          analogWrite(BUZZER, 0);      
        }
      }else if((String(hour(current_time)) == String(horas)) && (String(minute(current_time)) == String(minutos))){
      alarma=false;  
    }
    }
  }else if(YOUR_WIFI_SSID == "Cocina"){
    
    if(digitalRead(BOTON_MODO)==LOW){
      temp=true;
      temporizador();
    }
    
    if(temp){
      ui.nextFrame();
      ui.previousFrame();
      remainingTimeBudget = ui.update();
      segundos--;
      if(segundos==0){
        while(digitalRead(BOTON_MODO)==HIGH){
          analogWrite(BUZZER, 255);
          delay(500);
          analogWrite(BUZZER, 0);
          ui.nextFrame();
          ui.previousFrame();
          remainingTimeBudget = ui.update();
        }
        temp=false;
        analogWrite(BUZZER, 0);   
      }
    }
  }
  ui.nextFrame();
  ui.previousFrame();
  remainingTimeBudget = ui.update();

}

//Módulo para obtener el tiempo en una ciudad
void getWeatherData() //client function to send/receive GET request data.
{
  Serial.println("Getting Weather Data");
  if (client.connect(servername, 80)) {  //starts client connection, checks for connection
    client.println("GET /data/2.5/forecast?id="+CityID+"&units=metric&cnt=1&lang=es&APPID="+APIKEY);
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
  } 
  else {
    Serial.println("connection failed"); //error message if no client connect
    Serial.println();
  }

  while(client.connected() && !client.available()) delay(1); //waits for data
 
    Serial.println("Waiting for data");

  while (client.connected() || client.available()) { //connected or data available
    char c = client.read(); //gets byte from ethernet buffer
      result = result+c;
    }

  client.stop(); //stop client
  result.replace('[', ' ');
  result.replace(']', ' ');
  Serial.println(result);

char jsonArray [result.length()+1];
result.toCharArray(jsonArray,sizeof(jsonArray));
jsonArray[result.length() + 1] = '\0';

StaticJsonBuffer<1024> json_buf;
JsonObject &root = json_buf.parseObject(jsonArray);
if (!root.success())
{
  Serial.println("parseObject() failed");
}

String location = root["city"]["name"];
String temperature = root["list"]["main"]["temp"];
String weather = root["list"]["weather"]["main"];
String description = root["list"]["weather"]["description"];
String idString = root["list"]["weather"]["id"];
String timeS = root["list"]["dt_txt"];
String tmax = root["list"]["main"]["temp_max"];
String tmin = root["list"]["main"]["temp_min"];


Llocation = location;
Ltemperature = temperature;
Lweather = weather;
Ldescription = description;
LidString = idString;
LtimeS = timeS;
Ltmax = tmax;
Ltmin = tmin;

int length = temperature.length();
if(length==5)
{
  temperature.remove(length-1);
}

Serial.println(Llocation);
Serial.println(Lweather);
Serial.println(Ltemperature);
Serial.println(Ldescription);
Serial.println(Ltemperature);
Serial.println(LtimeS);


}

//Módulo para convertir el tiempo
String convertGMTTimeToLocal(String timeS)
{
 int length = timeS.length();
 timeS = timeS.substring(length-8,length-6);
 int time = timeS.toInt();
 time = time+TimeZone;

 if(time > 21 ||  time<7)
 {
  night=true;
 }else
 {
  night = false;
 }
 timeS = String(time)+":00";
 return timeS;
}


