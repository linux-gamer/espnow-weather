#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_AHTX0.h>

#define ESP_NOW_ID 0

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */
 
#define SENSOR_POWER_PIN 12
#define pin 2

#define SCL 0
#define SDA 1

Adafruit_BMP280 bmp; // I2C
Adafruit_AHTX0 aht;

// Set the SLAVE MAC Address
uint8_t slaveAddress0[] = {0x54, 0x32, 0x04, 0x46, 0x52, 0x08};
uint8_t slaveAddress1[] = {0xD8, 0xBC, 0x38, 0xFD, 0x58, 0x28};
uint8_t slaveAddress2[] = {0xD8, 0xBF, 0xC0, 0x06, 0xC2, 0x15};

// Insert your SSID
constexpr char WIFI_SSID[] = "udmurtien";
// Structure to keep the temperature and humidity data from a BME sensor
/*typedef*/ struct temp_struct {
  int id;
  float temp;
  float hum;
  float pres;
};
// Create a struct_message called myData
temp_struct tempData;
// Callback to have a track of sent messages
void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);

  Serial.print("Message status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent Successfully" : "Sent Failed");
}

/*int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
            Serial.println(WiFi.RSSI(i));
              return WiFi.channel(i);
          }
      }
  }
  esp_deep_sleep_start();
}*/
 
void setup() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  // Turn sensor on
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);
  // Init Serial Monitor
  //Serial.begin(115200);
  //delay(2000);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  Wire.begin(SDA, SCL);

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  unsigned status;

  status = bmp.begin();
  if (!status) {
    //Serial.println("Could not find BMP? Check wiring");
    esp_deep_sleep_start();
  }

  if (!aht.begin()) {
    //Serial.println("Could not find AHT? Check wiring");
    esp_deep_sleep_start();
  }
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
//  int32_t channel = getWiFiChannel(WIFI_SSID);
  esp_wifi_set_channel(3, WIFI_SECOND_CHAN_NONE);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    //Serial.println("There was an error initializing ESP-NOW");
    esp_deep_sleep_start();
  }
  // We will register the callback function to respond to the event
  esp_now_register_send_cb(OnSent);
  
  // Register the slave
  esp_now_peer_info_t slaveInfo;
  memcpy(slaveInfo.peer_addr, slaveAddress0, 6);
  slaveInfo.channel = 0;  
  slaveInfo.encrypt = false;
  
  // Add slave        
  if (esp_now_add_peer(&slaveInfo) != ESP_OK){
    //Serial.println("There was an error registering the slave");
    esp_deep_sleep_start();
  }

  memcpy(slaveInfo.peer_addr, slaveAddress1, 6);

  if (esp_now_add_peer(&slaveInfo) != ESP_OK){
    //Serial.println("There was an error registering the slave");
    esp_deep_sleep_start();
  }

  memcpy(slaveInfo.peer_addr, slaveAddress2, 6);
  
  if (esp_now_add_peer(&slaveInfo) != ESP_OK){
    //Serial.println("There was an error registering the slave");
    esp_deep_sleep_start();
  }


  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

  tempData.id = ESP_NOW_ID;
  tempData.temp = bmp.readTemperature();
  tempData.hum = humidity.relative_humidity;
  tempData.pres = bmp.readPressure() / 100.0F;

  digitalWrite(SENSOR_POWER_PIN, LOW);

    // Is time to send the messsage via ESP-NOW
  esp_err_t result = esp_now_send(0, (uint8_t *) &tempData, sizeof(tempData));
   
  /*if (result == ESP_OK) {
    Serial.println("The message was sent sucessfully.");
    return;
  }
  else {
    Serial.println("There was an error sending the message.");
    return;
  }*/

  delay(50); // don't remove!!! this time is needed to send message
  //Serial.println("Took " + String(millis()) + " ms to complete.");
  digitalWrite(pin, LOW);
  esp_deep_sleep_start();

}
void loop() {
}
