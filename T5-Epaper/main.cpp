#include <Arduino.h>

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time.h"

#include <EEPROM.h>
#include <StreamUtils.h>

#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h> 

#define LILYGO_T5_V213
#include "boards.h"

#include <Fonts/FreeMonoBold9pt7b.h>

#define WIDTH  122
#define HEIGHT 250

GxIO_Class io(SPI, EPD_CS, EPD_DC, EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);

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

void fetchData();

void setup(void)
{
    Serial.begin(115200);
    EEPROM.begin(4096);

    SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI);
    display.init(115200);

    for(int i = 0; i< (sizeof networks / sizeof networks[0]); i++){
        wifiMulti.addAP(networks[i][0].c_str(),networks[i][1].c_str());
    } 

    display.setRotation(4);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(5,(HEIGHT/2));
    display.print("Loading...");
    display.update();

    fetchData();
}

void loop(){
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(5,15);
    display.print(userData["date"].as<const char*>());
    
    display.setTextColor(GxEPD_WHITE);
    for(int i = 0; i< periods.size(); i++){
        JsonVariant period = periods[i];
        display.fillRoundRect(0,(HEIGHT/5*i+(25-i*5)),(WIDTH),(HEIGHT/5-10),10,GxEPD_BLACK);
        display.setCursor(5,((HEIGHT/5*i+15+(periods.size()*5-i*5))));
        display.print(period[2].as<const char*>());
        display.print(" ");
        display.print(period[3].as<const char*>());
        display.setCursor(5,((HEIGHT/5*i+35+(periods.size()*5-i*5))));
        display.print(period[0].as<const char*>());
        display.print(" ");
        display.print(period[1].as<const char*>());
    }

    display.update();
    while(1){}
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
