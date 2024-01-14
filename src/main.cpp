/*
 An digital clock using a TFT LCD screen to show the time.

 Based on clock sketch by Gilchrist 6/2/2014 1.0
 Updated by Bodmer
 NTP added by ianlunam
 */

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <time.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

TFT_eSPI tft = TFT_eSPI();    // Invoke library, pins defined in User_Setup.h

uint32_t targetTime = 0;             // for next 1 second timeout

byte omm = 99;
bool initial = 1;
byte xcolon = 0;
unsigned int colour = 0;
struct tm timeinfo;
const char* tz = "NZST-12NZDT,M9.5.0,M4.1.0/3";
String ssid, pwd;

uint8_t hh, mm, ss;    // Get H, M, S from compile time

void setup(void) {
    // Get WiFi creds from preferences storage
    Serial.begin(115200);
    Preferences wifiCreds;
    wifiCreds.begin("wifiCreds", true);
    ssid = wifiCreds.getString("ssid");
    pwd = wifiCreds.getString("password");
    wifiCreds.end();

    WiFi.begin(ssid.c_str(), pwd.c_str());
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP()); 

    configTzTime(tz, "nz.pool.ntp.org");
    getLocalTime(&timeinfo);
    targetTime = millis() + 1000;

    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else    // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() {
            Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });


    byte mac[6];
    WiFi.macAddress(mac);
    char str[100] = "\0";
    strcat(str, "clock-");
    char buf[3];
    sprintf(buf, "%2X%2X%2X", mac[3], mac[4], mac[5]);
    strcat(str, buf);

    ArduinoOTA.setHostname(str);
    ArduinoOTA.begin();

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour
}

void loop() {
    getLocalTime(&timeinfo);
    ss = timeinfo.tm_sec;
    mm = timeinfo.tm_min;
    hh = timeinfo.tm_hour;
    
    if (targetTime < millis()) {
        targetTime = millis()+1000;

        // Update digital time
        byte xpos = 6;
        byte ypos = 30;

        if (ss==0 || initial) {
            initial = 0;
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor (50, ypos + 60);

            char ptr[20];
            int rc = strftime(ptr, 20, "%a %e %b", &timeinfo);
            tft.print(ptr);
        }


        if (omm != mm) { // Only redraw every minute to minimise flicker
            // Uncomment ONE of the next 2 lines, using the ghost image demonstrates text overlay as time is drawn over it
            tft.setTextColor(0x39C4, TFT_BLACK);    // Leave a 7 segment ghost image, comment out next line!
            //tft.setTextColor(TFT_BLACK, TFT_BLACK); // Set font colour to black to wipe image
            // Font 7 is to show a pseudo 7 segment display.
            // Font 7 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
            tft.drawString("88:88",xpos,ypos,7); // Overwrite the text to clear it
            // tft.setTextColor(0xFBE0); // Orange
            tft.setTextColor(0xFBE0); // BluelocalName
            omm = mm;

            if (hh<10) xpos+= tft.drawChar('0',xpos,ypos,7);
            xpos+= tft.drawNumber(hh,xpos,ypos,7);
            xcolon=xpos;
            xpos+= tft.drawChar(':',xpos,ypos,7);
            if (mm<10) xpos+= tft.drawChar('0',xpos,ypos,7);
            tft.drawNumber(mm,xpos,ypos,7);
        }

        if (ss%2) { // Flash the colon
            tft.setTextColor(0x39C4, TFT_BLACK);
            xpos+= tft.drawChar(':',xcolon,ypos,7);
        } else {
            tft.setTextColor(0xFBE0, TFT_BLACK);
            tft.drawChar(':',xcolon,ypos,7);
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnecting WiFi");
        WiFi.begin(ssid.c_str(), pwd.c_str());
        delay(500);
    }
}


