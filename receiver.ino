#include <Arduino.h>

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "time.h"
#include "RTClib.h"

#include <TFT_eSPI.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

#include "temperature_img.h"
#include "water_drop.h"

#define SDA 27
#define SCL 22

#define DCF77_PIN 35

#define CYD_LED_BLUE 17
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16

const char* daysOfTheWeek[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
const char* months[] = {"Dez", "Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};

unsigned long gotTimeMillis;
bool print_time = false;
volatile unsigned long lastInt = 0;
volatile unsigned long long currentBuf = 0;
volatile byte bufCounter;

bool MEZ;
byte dcf77Year;
byte dcf77Month;
byte dcf77DayOfWeek;
byte dcf77DayOfMonth;
byte dcf77Hour;
byte dcf77Minute;
bool parityBitMinute;
bool parityBitHour;
bool parityBitDate;

bool print_temp;
int esp_now_id;
float esp_now_temp;
float esp_now_hum;
float esp_now_pres;

RTC_DS3231 rtc;

Adafruit_BMP280 bmp; // I2C
Adafruit_AHTX0 aht;

sensors_event_t humidity, temp;

// Define data structure
struct temp_struct {
  int id;
  float temp;
  float hum;
  float pres;
};
 
// Create structured data object
temp_struct tempData;

TFT_eSPI tft = TFT_eSPI();

bool IRAM_ATTR parity_even_bit(byte val){
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  val &= 0x01;
  return val;
}

unsigned int rawByteToInt(byte raw){
  return ((raw>>4)*10 + (raw &0x0F));
}

void IRAM_ATTR evaluateSequence(){
  print_time = true;

  dcf77Year = (currentBuf>>50) & 0xFF;    // year = bit 50-57
  dcf77Month = (currentBuf>>45) & 0x1F;       // month = bit 45-49
  dcf77DayOfWeek = (currentBuf>>42) & 0x07;   // day of the week = bit 42-44
  dcf77DayOfMonth = (currentBuf>>36) & 0x3F;  // day of the month = bit 36-41
  dcf77Hour = (currentBuf>>29) & 0x3F;       // hour = bit 29-34
  dcf77Minute = (currentBuf>>21) & 0x7F;     // minute = 21-27
  MEZ = (currentBuf>>18) & 1;                      // MEZ = 1 MESZ = 0
  parityBitMinute = (currentBuf>>28) & 1;
  parityBitHour = (currentBuf>>35) & 1;
  parityBitDate = (currentBuf>>58) & 1;

  Serial.println(currentBuf);
  
  if (rawByteToInt(dcf77Month) > 12  || rawByteToInt(dcf77DayOfWeek) > 7 || rawByteToInt(dcf77DayOfMonth) > 31 || rawByteToInt(dcf77Hour) > 23 || rawByteToInt(dcf77Minute) > 59) {
    print_time = false;
  }

  if(bufCounter != 59){
    print_time = false;
  }
  if((parity_even_bit(dcf77Minute)) != parityBitMinute){
    //Serial.println("Minute parity not OK"); 
    print_time = false;
  }
  if((parity_even_bit(dcf77Hour)) != parityBitHour){
    //Serial.println("Hour parity not OK");
    print_time = false;
  }
  if(((parity_even_bit(dcf77DayOfMonth) + parity_even_bit(dcf77DayOfWeek) 
  + parity_even_bit(dcf77Month) + parity_even_bit(dcf77Year))%2) != parityBitDate)
  {
    //Serial.println("Date parity not OK");
    print_time = false;
  }
  gotTimeMillis = millis();
}

void IRAM_ATTR DCF77_ISR(){
  unsigned long dur = 0;
  dur = millis() - lastInt;
  
  if(digitalRead(DCF77_PIN)){
    //Serial.println(dur);
    if(dur>1500){
      unsigned long highBuf = (currentBuf>>32) & 0x7FFFFFF;
      unsigned long lowBuf = (currentBuf & 0xFFFFFFFF);
      /*Serial.print("Signal, upper 4 bytes: "); 
      Serial.println(highBuf, BIN);
      Serial.print("Signal, lower 4 bytes: "); 
      Serial.println(lowBuf, BIN); */
      evaluateSequence();
      bufCounter = 0;
      currentBuf = 0;
    }
  }
  else{
    Serial.println(bufCounter);
    /*Serial.print(". ");
    Serial.print(dur);
    Serial.print("  /  ");*/
    if(dur>150){
      currentBuf |= ((unsigned long long)1<<bufCounter);
    }
    bufCounter++;
  }
  lastInt = millis();
}

void printLocalTime() {
  DateTime now = rtc.now();
  if (now.hour() == 0 && now.minute() == 0 && now.second() == 0) {
    tft.fillRect(0, 50, 320, 30, TFT_BLACK);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  char buf1[] = "hh:mm:ss";
  String timeStr = now.toString(buf1);
  String dateStr = String(daysOfTheWeek[now.dayOfTheWeek()]) + ". " + String(now.day(), DEC) + ". " + months[now.month()];

  tft.drawCentreString(timeStr,  320/2, 0, 6);

  tft.drawCentreString(dateStr,  320/2, 50, 4);
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) 
{
  esp_now_peer_info_t peer_info;
  // Get incoming data
  memcpy(&tempData, incomingData, sizeof(tempData));

  print_temp = true;
  esp_now_id = tempData.id;
  esp_now_temp = tempData.temp;
  esp_now_hum = tempData.hum;
  esp_now_pres = tempData.pres;
  
  // Print to Serial Monitor
/*  Serial.println();

  Serial.print("Temperature: ");
  Serial.println(esp_now_temp);
  Serial.print("Humidity: ");
  Serial.println(esp_now_hum);
  Serial.print("Pressure: ");
  Serial.println(esp_now_pres);

  Serial.print("ESP NOW ID: ");
  Serial.println(esp_now_id);*/
}



void setup() {
  Serial.begin(115200);

  pinMode(DCF77_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DCF77_PIN), DCF77_ISR, CHANGE);

  pinMode(CYD_LED_RED, OUTPUT);
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  digitalWrite(CYD_LED_RED, HIGH);
  digitalWrite(CYD_LED_GREEN, HIGH);
  digitalWrite(CYD_LED_BLUE, HIGH);

  Wire.begin(SDA, SCL);

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
      Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
      Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
      Adafruit_BMP280::FILTER_X16,      /* Filtering. */
      Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (!bmp.begin()) {
    Serial.println("Could not find BMP? Check wiring");
  }

  if (!aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
  }

  // Start the tft display and set it to black
  tft.init();
  //tft.invertDisplay(1); //If you have a CYD2USB - https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/cyd.md#my-cyd-has-two-usb-ports
  tft.setRotation(1); //This is the display in landscape
  
  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  tft.pushImage(0, 160, TEMPERATURE_IMG_WIDTH, TEMPERATURE_IMG_HEIGHT, temperature_img);

  tft.setSwapBytes(true);
  tft.pushImage(160, 160, TEMPERATURE_IMG_WIDTH, TEMPERATURE_IMG_HEIGHT, temperature_img);

  tft.pushImage(5, 120, WATER_DROP_WIDTH, WATER_DROP_HEIGHT, water_drop);

  tft.pushImage(165, 120, WATER_DROP_WIDTH, WATER_DROP_HEIGHT, water_drop);

  WiFi.mode(WIFI_AP_STA);

  esp_wifi_set_channel(3, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  tft.drawString(String(humidity.relative_humidity, 1) + " %", 205, 125, 4);
  //tft.drawString(String(temp.temperature, 1), 194, 180, 6);
  tft.drawString(String(bmp.readTemperature(), 1), 194, 180, 6);

  if (print_time) {
    Serial.println("Sleep MS:" + String(1000 - ((millis() - gotTimeMillis) % 1000)));
    delay(1000 - ((millis() - gotTimeMillis) % 1000));

    Serial.print("MEZ: "); Serial.println(MEZ);

    Serial.println("Hours: " + String(rawByteToInt(dcf77Hour)));
    Serial.println("Minutes: " + String(rawByteToInt(dcf77Minute)));
    Serial.println("Seconds: " + String((millis() - gotTimeMillis) / 1000));

    Serial.print(daysOfTheWeek[rawByteToInt(dcf77DayOfWeek)-1]);
    Serial.print(", ");
    Serial.print(String(rawByteToInt(dcf77DayOfMonth)) + ".");
    Serial.print(String(rawByteToInt(dcf77Month)) + ".");
    Serial.println(2000 + rawByteToInt(dcf77Year));

    rtc.adjust(DateTime(2000 + rawByteToInt(dcf77Year), rawByteToInt(dcf77Month), rawByteToInt(dcf77DayOfMonth), rawByteToInt(dcf77Hour), rawByteToInt(dcf77Minute), (millis() - gotTimeMillis) / 1000));

    print_time = false;
  }

  if (print_temp) {
    // Print to Serial Monitor
    Serial.println();

    Serial.print("Temperature: ");
    Serial.println(esp_now_temp);
    Serial.print("Humidity: ");
    Serial.println(esp_now_hum);
    Serial.print("Pressure: ");
    Serial.println(esp_now_pres);

    Serial.print("ESP NOW ID: ");
    Serial.println(esp_now_id);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    if (esp_now_id == 0) {
      tft.drawString(String(esp_now_temp, 1),  34, 180, 6);

      tft.drawString(String(esp_now_hum, 1) + "%",  40, 125, 4);
    }
    else if (esp_now_id == 1) {
      tft.drawCentreString( String(esp_now_temp, 1) + 'C',  320/2, 80, 4);
    }
    else {
      tft.drawString("Err",  34, 180, 6);

      tft.drawString("Error",  40, 125, 4);
    }
    print_temp = false;
  }

  for(int i=0; i<10; i++){
    printLocalTime();
    delay(1000);
  }
}
