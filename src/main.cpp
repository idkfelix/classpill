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
#include <string.h>

WiFiMulti wifiMulti;
HTTPClient http;
CookieJar cookieJar;
TFT_eSPI tft = TFT_eSPI();

JsonArray periods;
DynamicJsonDocument docSorted(2048);

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
  wifiMulti.addAP("DeathStar","coffeemarantz");
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
        DynamicJsonDocument doc(2048);
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
          DynamicJsonDocument doc(2048);
          deserializeJson(doc, payload, DeserializationOption::Filter(filter));
          JsonArray arr = doc["d"]["data"].as<JsonArray>();
          std::vector<String> values;
          // Print to serial
          for (JsonVariant period : arr) {
            String s = (String)period["topAndBottomLine"].as<const char*>();
            values.push_back(s);
          }
          // Sort periods
          std::sort(values.begin(), values.end(), [](const String& a, const String& b) {
            // Split each string at the '-' character and convert the second part to an integer
            int aVal = a.substring(a.indexOf('-') + 1).toInt();
            int bVal = b.substring(b.indexOf('-') + 1).toInt();

            // Compare the integer values
            return aVal < bVal;
          });
          // Save to new arr
          periods = docSorted.to<JsonArray>();
          for(const String& value : values) {
            String periodString = value;
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
          // Save to memory
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

    DynamicJsonDocument doc(2048);
    EepromStream eepromStream(0, 2048);
    deserializeJson(doc, eepromStream);
    periods = doc.as<JsonArray>();
    tft.fillScreen(TFT_BLACK);
    tft.drawCentreString("Fetched Cached Data",64,64,1);
  }
  tft.fillScreen(TFT_BLACK);
}

void loop(){
  if(periods.size() >0){
    for(JsonVariant period : periods) {
      tft.print(period[0].as<const char*>());
      tft.print(" ");
      tft.print(period[2].as<const char*>());
      tft.print(" ");
      tft.println(period[3].as<const char*>());
      delay(500);
      while(digitalRead(9) == HIGH){}
    }
  } else {
    tft.drawCentreString("No Periods!",64,64,1);
  }
}