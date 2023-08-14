#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "esp_rom_gpio.h"
#include "driver/gpio.h"
#include "TFT_eSPI.h"
#include <EEPROM.h>
#include <StreamUtils.h>

WiFiMulti wifiMulti;
HTTPClient http;
CookieJar cookieJar;
TFT_eSPI tft = TFT_eSPI();


JsonArray arr;

void setup() {
  String userId = "";

  // Start Serial
  Serial.begin(115200);
  Serial.printf("\n\n\n\n\n");
  
  // Start Screen
  tft.init();
  tft.setRotation(3);

  // Start EEPROM
  EEPROM.begin(1000);

  // Configure HTTP
  http.setCookieJar(&cookieJar);
  http.setReuse(true);

  // Define Networks here
  wifiMulti.addAP("Felix", "idkidkidk");
  WiFi.setHostname("classpill");

  tft.fillScreen(TFT_BLACK);
  tft.drawCentreString("Connecting Wifi...",64,64,1);
  Serial.printf("Connecting Wifi...\n");

  if(wifiMulti.run() == WL_CONNECTED) {
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Wifi Connected!",64,64,1);
    Serial.printf("WiFi connected!\n");
    Serial.printf("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("\n");

    // Auth with Compass API
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Authenticating...",64,64,1);
    http.begin("https://mullauna-vic.compass.education/services/admin.svc/AuthenticateUserCredentials");
    http.addHeader("Content-Type", "application/json");
    int httpCodeA = http.POST("{\"username\":\"cou0008\",\"password\":\"Lion.8664\"}");

    if(httpCodeA > 0) {
      if(httpCodeA == HTTP_CODE_OK) {
        // Convert response to JSON
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        // Get and print userId
        userId = doc["d"]["roles"][0]["userId"].as<String>();
        Serial.printf("User ID: %s\n\n",userId);
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
      http.begin("https://mullauna-vic.compass.education/Services/mobile.svc/GetScheduleLinesForDate");
      http.addHeader("Content-Type", "application/json");
      int httpCodeB = http.POST("{\"userId\":\""+userId+"\",\"date\":\"14/08/2023 - 10:00 AM\"}");

      if(httpCodeB > 0) {
        if(httpCodeB == HTTP_CODE_OK) {
        String payload = http.getString();
          // Filter and deserialise JSON
          StaticJsonDocument<64> filter;
          filter["d"]["data"][0]["topAndBottomLine"] = true;
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          arr = doc["d"]["data"].as<JsonArray>();
          // Print to serial
          for (JsonVariant period : arr) {
            String s = (String)period["topAndBottomLine"].as<const char*>();
            Serial.println(s);
          }
          // Save to memory
          EepromStream eepromStream(0, 1024);
          serializeJson(doc, eepromStream);
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

    DynamicJsonDocument doc(1024);
    EepromStream eepromStream(0, 1024);
    deserializeJson(doc, eepromStream);
    arr = doc["d"]["data"].as<JsonArray>();
    // Print to serial
    for (JsonVariant period : arr) {
      String s = (String)period["topAndBottomLine"].as<const char*>();
      Serial.println(s);
    }
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Fetched Cached Data",64,64,1);
  }
  tft.fillScreen(TFT_BLACK);
}

void loop(){
  tft.setTextWrap(true, true);
  if(arr.size() >0){
    for(JsonVariant period : arr) {
      tft.println(period["topAndBottomLine"].as<const char*>());
    }
  } else {
    tft.drawCentreString("No Period!",64,64,1);
  }
  while(1){}
}