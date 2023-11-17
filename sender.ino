#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */
 
#define BME_SCL 17
#define BME_SDA 21

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;  // I2C

// Set the SLAVE MAC Address
uint8_t slaveAddress[] = {0x54, 0x32, 0x04, 0x46, 0x52, 0x08};
// Insert your SSID
constexpr char WIFI_SSID[] = "wlan-ssid";
// Structure to keep the temperature and humidity data from a DHT sensor
typedef struct temp_struct {
  float temp;
  float hum;
  float pres;
};
// Create a struct_message called myData
temp_struct tempData;
// Callback to have a track of sent messages
void OnSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nSend message status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sent Successfully" : "Sent Failed");
}
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
      for (uint8_t i=0; i<n; i++) {
          if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
              return WiFi.channel(i);
          }
      }
  }
  return 0;
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  pinMode(34, OUTPUT);
  digitalWrite(34, HIGH);

  Wire.begin(BME_SDA, BME_SCL);

  unsigned status;
  status = bme.begin(0x76, &Wire);
  if (!status) {
    esp_deep_sleep_start();
  }
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  int32_t channel = getWiFiChannel(WIFI_SSID);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("There was an error initializing ESP-NOW");
    esp_deep_sleep_start();
  }
  // We will register the callback function to respond to the event
  esp_now_register_send_cb(OnSent);
  
  // Register the slave
  esp_now_peer_info_t slaveInfo;
  memcpy(slaveInfo.peer_addr, slaveAddress, 6);
  slaveInfo.channel = 0;  
  slaveInfo.encrypt = false;
  
  // Add slave        
  if (esp_now_add_peer(&slaveInfo) != ESP_OK){
    Serial.println("There was an error registering the slave");
    esp_deep_sleep_start();
  }

  tempData.temp = bme.readTemperature();
  tempData.hum = bme.readHumidity();
  tempData.pres = bme.readPressure() / 100.0F;

    // Is time to send the messsage via ESP-NOW
  esp_err_t result = esp_now_send(slaveAddress, (uint8_t *) &tempData, sizeof(tempData));
   
  if (result == ESP_OK) {
    Serial.println("The message was sent sucessfully.");
  }
  else {
    Serial.println("There was an error sending the message.");
  }

  delay(50); // don't remove!!! this time is needed to send message
  Serial.println("Took " + String(millis()) + " ms to complete.");
  esp_deep_sleep_start();

}
void loop() {
}
