#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "esp_rom_gpio.h"
#include "driver/gpio.h"
#include "TFT_eSPI.h"

WiFiMulti wifiMulti;
HTTPClient http;
CookieJar cookieJar;

TFT_eSPI tft = TFT_eSPI();

bool fetched = false;
String userId = "";
JsonArray arr;
String data = "";
String html = "";

void setup() {
  // Start Serial
  Serial.begin(115200);
  Serial.printf("\n\n\n\n\n");
  
  // Start Screen
  tft.init();
  tft.setRotation(3);

  // Configure HTTP
  http.setCookieJar(&cookieJar);
  http.setReuse(true);

  // Define Networks here
  wifiMulti.addAP("ssid", "pass");
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
    int httpCodeA = http.POST("{\"username\":\"\",\"password\":\"\"}");

    if(httpCodeA > 0) {
      if(httpCodeA == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
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
      int httpCodeB = http.POST("{\"userId\":\""+userId+"\",\"date\":\"11/08/2023 - 10:00 AM\"}");

      if(httpCodeB > 0) {
        if(httpCodeB == HTTP_CODE_OK) {
        String payload = http.getString();
          StaticJsonDocument<64> filter;
          filter["d"]["data"][0]["topAndBottomLine"] = true;
          DynamicJsonDocument doc(1024);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          arr = doc["d"]["data"].as<JsonArray>();
          for (JsonVariant period : arr) {
            String s = (String)period["topAndBottomLine"].as<const char*>();
            Serial.println(s);
          }
          serializeJson(doc["d"]["data"], data);
        } else {
          Serial.printf("%i\n",httpCodeB);
        }
      } else {
        Serial.printf("%s\n", http.errorToString(httpCodeB).c_str());
      }
      http.end();
    }
  } 
}

void loop(){
  for (JsonVariant period : arr) {
    String s = (String)period["topAndBottomLine"].as<const char*>();
    tft.drawCentreString(s,64,64,2);
    delay(1000);
  }
}