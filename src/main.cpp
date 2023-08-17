#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <StreamUtils.h>
#include "rm67162.h"
#include <TFT_eSPI.h>
#include "ani.h"
#include "Free_Fonts.h" 

#define WIDTH  536
#define HEIGHT 240

String subDomain = "mullauna-vic";
String user = "cou0008";
String pass = "Lion.8664";
String networks[][2] = {
  {"Felix","idkidkidk"},
  {"DeathStar","coffeemarantz"}
};

WiFiMulti wifiMulti;

String date = "";
JsonArray periods;
DynamicJsonDocument docPeriods(2048);
DynamicJsonDocument userData(2048);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

void fetchData();

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

  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE);
  spr.drawCentreString("Loading...",(WIDTH/2),(HEIGHT/2),4);
  lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());

  fetchData();
}

int i = 0;
int n = 0;
void loop(){
  if(periods.size() >0){
    JsonVariant period = periods[i];
    spr.fillSprite(TFT_BLACK);

    spr.pushImage(55,65,102,160,ani[n]);

    spr.drawRoundRect(10,10,(WIDTH-20),40,5,TFT_WHITE);
    spr.drawRoundRect(10,60,150,(HEIGHT-70),5,TFT_WHITE);
    spr.drawRoundRect(170,60,(WIDTH-180),(HEIGHT-70),5,TFT_WHITE);

    spr.setTextColor(0x2C3C);
    spr.drawString((String)userData["reportName"].as<const char*>(),20,20,4);

    // if(WiFi.SSID()) spr.drawString(WiFi.SSID(),20,20,4);
    // else spr.drawString("No WiFi",20,20,4);

    spr.setTextColor(TFT_WHITE);
    spr.setFreeFont(FF23);
    spr.drawString((String)period[2].as<const char*>(),180,70,GFXFF);
    spr.drawRightString((String)period[1].as<const char*>(),(WIDTH-20),70,GFXFF);
    spr.setFreeFont(FF22);
    spr.drawString((String)period[0].as<const char*>(),180,110,GFXFF);

    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());

    delay(75);
    n++;
    if(n==45) n=0;
    spr.pushImage(55,65,102,160,ani[n]);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
    delay(75);

    if(digitalRead(21) == HIGH){
      if(i==0) i=(periods.size()-1);
      else i--;
    }
    if(digitalRead(0) == HIGH){
      if(i==(periods.size()-1)) i=0;
      else i++;
    }

    n++;
    if(n==45) n=0;
  } else {
    spr.fillSprite(TFT_BLACK);
    spr.drawCentreString("No Periods!",(WIDTH/2),(HEIGHT/2),4);
    lcd_PushColors(0, 0, WIDTH, HEIGHT, (uint16_t *)spr.getPointer());
  }
}

void fetchData(){
  String userId = "";

  HTTPClient http;
  CookieJar cookieJar;

  http.setCookieJar(&cookieJar);
  http.setReuse(true);

  if(wifiMulti.run() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
    // Auth with Compass API
    http.begin("https://"+subDomain+".compass.education/services/admin.svc/AuthenticateUserCredentials");
    http.addHeader("Content-Type", "application/json");
    const char* headerNames[] = { "date" };
    http.collectHeaders(headerNames, sizeof(headerNames)/sizeof(headerNames[0]));
    int httpCodeA = http.POST("{\"username\":\""+user+"\",\"password\":\""+pass+"\"}");

    if(httpCodeA > 0) {
      if(httpCodeA == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);
        userId = doc["d"]["roles"][0]["userId"].as<String>();
        Serial.printf("User ID: %s\n",userId);
        String indate = http.header("date");
        char s_month[5];
        int day, year;
        const String months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        if (indate.length() > 0) {
          int n = indate.length();
          char date_array[n+1];
          strcpy(date_array, indate.c_str());
          sscanf(date_array, "%*s %d %s %d %*s", &day, s_month, &year);
          int month = 0;
          for(int i=0; i<12; i++) {
            if(months[i] == String(s_month)) {
              month = i+1;
              break;
            }
          }

          char output[11];
          sprintf(output, "%02d/%02d/%04d", day, month, year);
          date = (String)output;
        }
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
          filter["d"]["data"]["formGroup"] = true;
          filter["d"]["data"]["school"]["Name"] = true;
          DynamicJsonDocument doc(2048);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          userData = doc["d"]["data"].as<JsonVariant>();

          EepromStream eepromStream(2048, 2048);
          serializeJson(periods, eepromStream);
          eepromStream.flush();
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
