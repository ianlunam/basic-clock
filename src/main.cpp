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
#include <math.h>

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


uint8_t BL_MIN = 203;
uint8_t BL_MAX = 220;
uint8_t bl_pin = 25;
uint8_t backlight = BL_MIN;

uint8_t day = 0;
uint8_t gardenBin = -1;
uint8_t recycleBin = -1;
uint8_t landfillBin = -1;

#define THIS_BLACK 0x0
#define THIS_WHITE 0xFFFFFF
#define THIS_BLUE 0xFBE0
#define THIS_GREEN 0x7E0
#define THIS_RED 0xFF
#define THIS_YELLOW 0x7FF
#define THIS_GREY 0x39C4

void drawCircle(int16_t x, int16_t y, int16_t r, int16_t colour, bool fill) {
    if (fill) {
        tft.fillCircle(x, y, r-1, colour);
        tft.drawCircle(x, y, r, THIS_WHITE);
    } else {
        tft.drawCircle(x, y, r, colour);
    }
}

void setBacklight(int8_t value) {
    Serial.println("setBacklight");
    if (backlight != value) {
        backlight = value;
        dacWrite(bl_pin, backlight);
    }
}

int daysDiff() {
    struct tm tm1;
    getLocalTime(&tm1);
    struct tm tm2 = { 0 };

    /* date 2: 2024-1-3 - A landfill and garden bin day */
    tm2.tm_year = 2024 - 1900;
    tm2.tm_mon = 1 - 1;
    tm2.tm_mday = 3;
    tm2.tm_hour = tm2.tm_min = tm2.tm_sec = 0;
    tm2.tm_isdst = -1;

    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    double dt = difftime(t1, t2);
    return round(dt / 86400);   
}

void setup(void) {
    // Get WiFi creds from preferences storage
    Serial.begin(115200);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(THIS_BLACK);
    dacWrite(bl_pin, backlight);

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
        byte ypos = 40;

        if (ss==0 || initial) {
            initial = 0;
            tft.setTextColor(THIS_GREEN, THIS_BLACK);
            tft.setCursor (50, ypos + 60);

            char ptr[20];
            int rc = strftime(ptr, 20, "%a %e %b", &timeinfo);
            tft.print(ptr);
        }


        if (omm != mm) { // Only redraw every minute to minimise flicker
            // tft.setTextColor(THIS_GREY, THIS_BLACK);    // Leave a 7 segment ghost image, comment out next line!
            tft.setTextColor(THIS_BLACK, THIS_BLACK); // Set font colour to black to wipe image
            tft.drawString("88:88",xpos,ypos,7); // Overwrite the text to clear it
            tft.setTextColor(THIS_BLUE); // Blue
            omm = mm;

            if (hh<10) xpos+= tft.drawChar('0',xpos,ypos,7);
            xpos+= tft.drawNumber(hh,xpos,ypos,7);
            xcolon=xpos;
            xpos+= tft.drawChar(':',xpos,ypos,7);
            if (mm<10) xpos+= tft.drawChar('0',xpos,ypos,7);
            tft.drawNumber(mm,xpos,ypos,7);

            if ((hh >= 21 || hh <= 5)) {
                setBacklight(BL_MIN);
            } else {
                setBacklight(BL_MAX);
            }
        }

        if (ss%2) { // Flash the colon
            tft.setTextColor(THIS_GREY, THIS_BLACK);
            xpos+= tft.drawChar(':',xcolon,ypos,7);
        } else {
            tft.setTextColor(THIS_BLUE, THIS_BLACK);
            tft.drawChar(':',xcolon,ypos,7);

        }

        if (day != timeinfo.tm_mday) {
            day = timeinfo.tm_mday;
            int timelapse = daysDiff();
            Serial.print("Timelapse: ");
            Serial.println(timelapse);

            landfillBin = 14 - (timelapse%14);
            recycleBin = 14 - ((timelapse + 7)%14);
            gardenBin = 28 - (timelapse%28);

            int8_t binStart = 50;
            drawCircle(binStart, 15, 10, THIS_RED, (landfillBin<7));
            drawCircle(binStart + 25, 15, 10, THIS_YELLOW, (recycleBin<7));
            if (gardenBin<7) {
                drawCircle(binStart + 50, 15, 10, THIS_GREEN, true);
            } else {
                drawCircle(binStart + 50, 15, 10, THIS_GREEN, false);
                tft.setTextColor(THIS_GREEN, THIS_BLACK);
                tft.drawNumber((gardenBin - (gardenBin % 7)) / 7, binStart + 46, 7, 2);
            }
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Reconnecting WiFi");
        WiFi.begin(ssid.c_str(), pwd.c_str());
        delay(500);
    }
}


