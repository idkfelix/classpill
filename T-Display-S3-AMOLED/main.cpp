#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "ESPAsyncWebServer.h"
#include "time.h"

#include <EEPROM.h>
#include <StreamUtils.h>

#include <TFT_eSPI.h>
#include "rm67162.h"
#include "Free_Fonts.h" 

#include <esp_sleep.h>

#define WIDTH  536
#define HEIGHT 240

#define ACCENT 0x2C3C

String subDomain = "mullauna-vic";
String user = "cou0008";
String pass = "Lion.8664";
String networks[][2] = {
  {"Felix","idkidkidk"},
  {"DeathStar","coffeemarantz"}
};

WiFiMulti wifiMulti;

JsonArray periods;
DynamicJsonDocument docPeriods(2048);
DynamicJsonDocument userData(2048);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

AsyncWebServer server(80);

void fetchData();
void drawHome(int i);
void drawLoad();

void setup(){
  Serial.begin(115200);
  
  pinMode(0,INPUT_PULLUP);
  pinMode(21,INPUT_PULLUP);

  EEPROM.begin(4096);

  rm67162_init();
  lcd_setRotation(1);
  spr.createSprite(WIDTH, HEIGHT);
  spr.setSwapBytes(1);

  for(int i = 0; i< (sizeof networks / sizeof networks[0]); i++){
    wifiMulti.addAP(networks[i][0].c_str(),networks[i][1].c_str());
  } 

  drawLoad();
  fetchData();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0,0);

  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  //   String html = "";
  //   html += "<form method='POST' action='/submit'>";
  //   html += "Username:<br><input type='text' name='user' value='"+user+"'><br>";
  //   html += "Password:<br><input type='text' name='pass' value='"+pass+"'><br>";
  //   html += "<input type='submit' value='Submit'>";
  //   html += "</form>";
  //   request->send(200, "text/html", html);
  // });

  // server.on("/submit", HTTP_POST, [](AsyncWebServerRequest *request){
  //   if(request->hasParam("user", true) && request->hasParam("pass", true)) {
  //       user = request->getParam("user", true)->value();
  //       pass = request->getParam("pass", true)->value();
  //   }
  //   request->send(200, "text/plain", "User and password updated");
  // });

  // server.begin();
}

int i = 0;
int n = 0;
void loop(){
  drawHome(i);

  delay(150);

  if(digitalRead(0) == LOW){
    int time = 0;
    while(digitalRead(0) == LOW){
      delay(10);
      time++;
    }
    if(time>300) {
      esp_deep_sleep_start();
    } else if(time>100){
      spr.fillSprite(TFT_BLACK);
      lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
      esp_light_sleep_start();
    } else {
      if(i==(periods.size()-1)) i=0;
      else i++;
      n=0;
    }
  }

  if(digitalRead(21) == LOW){
    int time = 0;
    while(digitalRead(21) == LOW){
      delay(10);
      time++;
    }
    if(time>100){
      drawLoad();
      fetchData();
    } else {
      if(i==0) i=(periods.size()-1);
      else i--;
      n=0;
    }
  }

  n++;
  if(n==(60000/200)){
    spr.fillSprite(TFT_BLACK);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    esp_light_sleep_start();
  }
}

void drawLoad(){
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.setFreeFont(FF24);
  spr.drawCentreString("CLASSPILL",(WIDTH/2),(HEIGHT/2-40),GFXFF);
  spr.setFreeFont(FF22);
  spr.drawCentreString("Loading...",(WIDTH/2),(HEIGHT/2+20),GFXFF);
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
}

void drawHome(int i){
  JsonVariant period = periods[i];
  spr.fillSprite(TFT_BLACK);

  // Boxes
  spr.drawRoundRect(0,0,(WIDTH),40,5,TFT_WHITE);
  spr.drawRoundRect(0,50,150,(HEIGHT-50),5,TFT_WHITE);
  spr.drawRoundRect(160,50,(WIDTH-160),(HEIGHT-50),5,TFT_WHITE);

  // Top Bar
  spr.setTextColor(TFT_WHITE);
  spr.setFreeFont(FF22);
  spr.drawString((String)userData["reportName"].as<const char*>(),10,10,GFXFF);
  spr.drawRightString((String)period[0].as<const char*>()+" - "+(String)userData["date"].as<const char*>(),(WIDTH-10),10,GFXFF);

  // Side Bar
  spr.setTextColor(ACCENT);
  spr.setFreeFont(FF22);
  spr.drawString("Network",10,60,GFXFF);
  spr.setTextColor(TFT_WHITE);
  spr.setFreeFont(FF18);
  if(WiFi.isConnected()) spr.drawString(WiFi.SSID(),10,85,GFXFF);
  else spr.drawString("No WiFi",10,85,GFXFF);

  spr.setTextColor(ACCENT);
  spr.setFreeFont(FF22);
  spr.drawString("IP Address",10,120,GFXFF);
  spr.setTextColor(TFT_WHITE);
  spr.setFreeFont(FF18);
  if(WiFi.isConnected()) spr.drawString(WiFi.localIP().toString(),10,145,GFXFF);
  else spr.drawString("No IP",10,145,GFXFF);

  spr.setTextColor(ACCENT);
  spr.setFreeFont(FF22);
  spr.drawString("User Code",10,180,GFXFF);
  spr.setTextColor(TFT_WHITE);
  spr.setFreeFont(FF18);
  spr.drawString((String)userData["displayCode"].as<const char*>(),10,205,GFXFF);

  // Main Content
  spr.setTextColor(ACCENT);
  spr.setFreeFont(FF24);
  if(periods.size() >0){
    spr.drawString((String)period[2].as<const char*>(),180,70,GFXFF);
    spr.setTextColor(TFT_WHITE);
    spr.drawRightString((String)period[1].as<const char*>(),(WIDTH-20),70,GFXFF);
    spr.setFreeFont(FF23);
    spr.drawString("Room: "+(String)period[3].as<const char*>(),180,130,GFXFF);
    spr.drawString("Teacher: "+(String)period[4].as<const char*>(),180,180,GFXFF);
  } else {
    spr.drawString("No Periods!",180,70,GFXFF);
  }

  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
}

void fetchData(){
  String userId = "";
  String date = "";

  HTTPClient http;
  CookieJar cookieJar;

  http.setCookieJar(&cookieJar);
  http.setReuse(true);

  if(wifiMulti.run() == WL_CONNECTED) {
    configTime(36000,0,"au.pool.ntp.org");
    struct tm timeinfo;
    char timeString[50];
    getLocalTime(&timeinfo);
    strftime(timeString, sizeof(timeString), "%d/%m/%Y", &timeinfo);
    date = timeString;
    date = "18/08/2023";
    
    // Auth with Compass API
    http.begin("https://"+subDomain+".compass.education/services/admin.svc/AuthenticateUserCredentials");
    http.addHeader("Content-Type", "application/json");
    int httpCodeA = http.POST("{\"username\":\""+user+"\",\"password\":\""+pass+"\"}");

    if(httpCodeA > 0) {
      if(httpCodeA == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        userId = doc["d"]["roles"][0]["userId"].as<String>();
        Serial.printf("User ID: %s\n",userId);
      } else {
        Serial.printf("%i\n",httpCodeA);
      }
    } else {
      Serial.printf("%s\n", http.errorToString(httpCodeA).c_str());
    }
    http.end();

    if(httpCodeA == HTTP_CODE_OK) {
      // Fetch Schedule Data
      http.begin("https://"+subDomain+".compass.education/Services/mobile.svc/GetScheduleLinesForDate");
      http.addHeader("Content-Type", "application/json");
      int httpCodeB = http.POST("{\"userId\":\""+userId+"\",\"date\":\""+date+" - 10:00 AM\"}");

      if(httpCodeB > 0) {
        if(httpCodeB == HTTP_CODE_OK) {
          String payload = http.getString();
          StaticJsonDocument<64> filter;
          filter["d"]["data"][0]["topAndBottomLine"] = true;
          DynamicJsonDocument doc(2048);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          JsonArray arr = doc["d"]["data"].as<JsonArray>();

          std::vector<String> values;
          for (JsonVariant period : arr) {
            String s = (String)period["topAndBottomLine"].as<const char*>();
            values.push_back(s);
          }
          std::sort(values.begin(), values.end(), [](const String& a, const String& b) {
            int aVal = a.substring(a.indexOf('-') + 1).toInt();
            int bVal = b.substring(b.indexOf('-') + 1).toInt();
            return aVal < bVal;
          });
          periods = docPeriods.to<JsonArray>();

          for(const String& value : values) {
            String periodString = value;
            periodString.replace("<s>", "(");
            periodString.replace("</s>", ")");
            periodString.replace("&nbsp;", "");
            String delimiter = " - ";
            DynamicJsonDocument docPeriod(2048);
            JsonArray period = docPeriod.to<JsonArray>();

            int delimiterIndex = periodString.indexOf(delimiter);
            while (delimiterIndex >= 0) {
              String substring = periodString.substring(0, delimiterIndex);
              period.add(substring);
              periodString = periodString.substring(delimiterIndex + delimiter.length());
              delimiterIndex = periodString.indexOf(delimiter);
            }
            period.add(periodString);
            periods.add(period);
          }

          EepromStream eepromStream(0, 2048);
          serializeJson(periods, eepromStream);
          eepromStream.flush();
        } else {
          Serial.printf("%i\n",httpCodeB);
        }
      } else {
        Serial.printf("%s\n", http.errorToString(httpCodeB).c_str());
      }
      http.end();

      // Fetch UserData
      http.begin("https://"+subDomain+".compass.education/services/mobile.svc/GetMobilePersonalDetails");
      http.addHeader("Content-Type", "application/json");
      int httpCodeC = http.POST("{\"userId\":\""+userId+"\"}");

      if(httpCodeC > 0) {
        if(httpCodeC == HTTP_CODE_OK) {
          String payload = http.getString();
          StaticJsonDocument<64> filter;
          filter["d"]["data"]["displayCode"] = true;
          filter["d"]["data"]["reportName"] = true;
          DynamicJsonDocument doc(2048);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          userData = doc["d"]["data"].as<JsonVariant>();
          userData["date"] = date;
          EepromStream eepromStreamA(2048, 2048);
          serializeJson(userData, eepromStreamA);
          eepromStreamA.flush();
        } else {
          Serial.printf("%i\n",httpCodeC);
        }
      } else {
        Serial.printf("%s\n", http.errorToString(httpCodeC).c_str());
      }
      http.end();
    }
  } else {
    EepromStream eepromStream(0, 2048);
    deserializeJson(docPeriods, eepromStream);
    periods = docPeriods.as<JsonArray>();

    EepromStream eepromStreamA(2048, 2048);
    deserializeJson(userData, eepromStreamA);
  }
}
