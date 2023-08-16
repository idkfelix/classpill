#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <EEPROM.h>
#include <StreamUtils.h>

#include "esp_rom_gpio.h"
#include "driver/gpio.h"
#include "TFT_eSPI.h"

String subDomain = "mullauna-vic";
String user = "cou0008";
String pass = "Lion.8664";
String networks[][2] = {
  {"SSID","password"},
  {"Felix","idkidkidk"},
  {"DeathStar","coffeemarantz"}
};

WiFiMulti wifiMulti;

HTTPClient http;
CookieJar cookieJar;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

JsonArray periods;
DynamicJsonDocument docSorted(2048);
String date = "";

void setup() {
  String userId = "";

  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  EEPROM.begin(1000);
  http.setCookieJar(&cookieJar);
  http.setReuse(true);

  for(int i = 0; i< (sizeof networks / sizeof networks[0]); i++){
    wifiMulti.addAP(networks[0][0].c_str(),networks[0][1].c_str());
  }
  wifiMulti.addAP("Felix","idkidkidk");
  WiFi.setHostname("class.pill");

  tft.fillScreen(TFT_BLACK);
  tft.drawCentreString("Connecting Wifi...",64,64,1);

  if(wifiMulti.run() == WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Wifi Connected!",64,64,1);

    // Auth with Compass API
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Authenticating...",64,64,1);
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
          date = output;
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
      tft.fillScreen(TFT_BLACK);
      tft.drawCentreString("Fetching Schedule...",64,64,1);
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
          periods = docSorted.to<JsonArray>();

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
    }
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("No WiFi found!",64,64,1);

    WiFi.softAP("classpill", "password");

    EepromStream eepromStream(0, 2048);
    deserializeJson(docSorted, eepromStream);
    periods = docSorted.as<JsonArray>();
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Fetched Cached Data",64,64,1);
  }

  tft.fillScreen(TFT_BLACK);
  spr.createSprite(128,128);
}



void loop(){
  if(periods.size() >0){
    int i = 0;
    while(1) {
      JsonVariant period = periods[i];
      spr.fillScreen(TFT_BLACK);
      spr.setCursor(0, 0);

      spr.setTextColor(TFT_BLACK);
      spr.fillRect(0,0,128,18,TFT_WHITE);
      spr.drawRightString((String)period[1].as<const char*>(),123,5,1);
      if(WiFi.isConnected()){
        spr.drawString(WiFi.SSID(),5,5,1);
      } else {
        spr.drawString("No Wifi",5,5,1);
      }

      spr.setTextColor(TFT_WHITE);
      spr.drawCentreString((String)period[0].as<const char*>(),64,34,2);
      spr.drawCentreString((String)period[2].as<const char*>(),64,54,2);
      spr.drawCentreString((String)period[4].as<const char*>(),64,74,2);
      spr.drawCentreString((String)period[3].as<const char*>(),64,94,2);

      spr.pushSprite(0, 0);

      delay(500);
      while((digitalRead(9) == HIGH)&&(digitalRead(21) == HIGH)){} // Do Nothing until btn press
      if(digitalRead(9) != HIGH){
        if(i==(periods.size()-1)) i=0;
        else i++;
      }
      if(digitalRead(21) != HIGH){
        if(i==0) i=(periods.size()-1);
        else i--;
      }
    }
  } else {
    tft.drawCentreString("No Periods!",64,64,1);
  }
}