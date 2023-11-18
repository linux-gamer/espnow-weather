#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <NTP.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

#include <Adafruit_GFX.h>  // Include core graphics library
#include <Adafruit_ST7735.h>  // Include Adafruit_ST7735 library to drive the display
#include <SPI.h>

#include "DHT.h"

#define DHTPIN 11     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321

// Declare pins for the display:
#define TFT_MOSI  3
#define TFT_SCLK  2
#define TFT_CS    7   // Chip select control pin
#define TFT_DC    6   // Data Command control pin
#define TFT_RST  10   // Reset pin (could connect to RST pin)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

unsigned long last_milli;
unsigned long last_sec;

const char* ssid = "wlan_ssid";
const char* password = "wlan_password";

WiFiUDP wifiUdp;
NTP ntp(wifiUdp);

const char * WriteAPIKey = "9PKFHUZOHA4JX84F";
 
// Define data structure
typedef struct temp_struct {
  float temp;
  float hum;
  float pres;
};
 
// Create structured data object
temp_struct tempData;

DHT dht(DHTPIN, DHTTYPE);

// Callback function
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) 
{
  // Get incoming data
  memcpy(&tempData, incomingData, sizeof(tempData));

  float temp = tempData.temp - 0.15;
  float hum = tempData.hum;
  float pres = tempData.pres + 11;
  
  // Print to Serial Monitor
  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(hum);
  Serial.print("Pressure: ");
  Serial.println(pres);
  String tempStr = String(temp, 2);
  String humStr = String(hum, 1);
  String presStr = String(pres, 2);

  tft.setTextColor(ST7735_ORANGE, ST7735_BLACK);  // Set color of text. First is the color of text and after is color of background
  tft.setTextSize(1);  // Set text size. Goes from 0 (the smallest) to 20 (very big)

  tft.setCursor(115, 40);  // Set position (x,y)
  tft.println(tempStr + " C");
  tft.setCursor(120, 50);  // Set position (x,y)
  tft.println(humStr + " %");
  tft.setCursor(90, 60);  // Set position (x,y)
  tft.println(presStr + " hPa");
}

void setup() {
  // Set up Serial Monitor
  Serial.begin(115200);

  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(1);
  tft.setTextWrap(false);
  
  tft.setTextColor(ST7735_ORANGE);  // Set color of text. First is the color of text and after is color of background
  tft.setTextSize(1);  // Set text size. Goes from 0 (the smallest) to 20 (very big)

  tft.setCursor(0, 30);  // Set position (x,y)
  tft.println("Aussen: ");
  tft.setCursor(8, 40);
  tft.println("Temperatur:");
  tft.setCursor(8, 50);
  tft.println("Luftfeuchtigkeit:");
  tft.setCursor(8, 60);
  tft.println("Luftdruck:");
  tft.setCursor(0, 70);
  tft.println("Innen: ");
  tft.setCursor(8, 80);
  tft.println("Temperatur:");
  tft.setCursor(8, 90);
  tft.println("Luftfeuchtigkeit:");
 
  // Start ESP32 in Station mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    ++count;
    if (count > 50) {
      ESP.restart();
    }
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  /*
    Zeitzone
    CEST: Central European Summertime
    Beginn europäische Sommerzeit letzter Sonntag im März 2 Uhr GMT + 2 Stunden
  */
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120);
  // CET: Central European Time
  // Beginn Normalzeit letzter Sonntag im Oktober 3 Uhr GMT + 1 Stunde
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60);
  // ntp starten
  ntp.begin();
 
  dht.begin();

  // Initalize ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
   
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);
  last_milli = millis();
  last_sec = millis();
}
 
void loop() {
  if (last_milli + 1000 < millis()) {
    last_milli = millis(); 
    ntp.update();

    tft.setCursor(8, 0);  // Set position (x,y)
    tft.setTextColor(ST7735_ORANGE, ST7735_BLACK);  // Set color of text. First is the color of text and after is color of background
    tft.setTextSize(3);  // Set text size. Goes from 0 (the smallest) to 20 (very big)

    String hours = (ntp.hours() < 10 ? "0" : "") + String(ntp.hours());
    String minutes = (ntp.minutes() < 10 ? "0" : "") + String(ntp.minutes());
    String seconds = (ntp.seconds() < 10 ? "0" : "") + String(ntp.seconds());

    tft.println(hours + ":" + minutes + ":" + seconds);

    if (last_sec + 10000 < millis()) {
      last_sec = millis();
      tft.setTextSize(1);
//      tft.fillRect(80, 96, 160, 128, ST7735_BLACK);
      tft.setTextColor(ST7735_ORANGE, ST7735_BLACK);

      tft.setCursor(115, 80);  // Set position (x,y)
      tft.println(String(dht.readTemperature() - 1.0, 2) + " C");
      tft.setCursor(120, 90);  // Set position (x,y)
      tft.println(String(dht.readHumidity(), 1) + " %");
    }
  }
}
