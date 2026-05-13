#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

#define MQ3_PIN 34
#define IR_PIN 35

// ⚠️ 1. PASTE THE MAC ADDRESS FROM THE MAIN BOARD HERE:
uint8_t mainBoardAddress[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; 

// ⚠️ 2. PASTE THE WIFI CHANNEL FROM THE MAIN BOARD HERE:
#define WIFI_CHANNEL 11 

typedef struct struct_message {
  int mq3_value;
  int ir_value;
} struct_message;

struct_message dataToSend;
unsigned long lastSendTime = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("✅ ESP-NOW LINK OK: Main Board received data.");
  } else {
    Serial.println("❌ ESP-NOW LINK FAILED: Main Board not found.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(MQ3_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  // 1. Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // 🔴 CRITICAL: Clear any saved router connections

  // 2. Force Radio Channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // 3. Verify Channel out loud so you can check it!
  Serial.print("👉 Sensor Radio is tuned to Channel: ");
  Serial.println(WiFi.channel());

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mainBoardAddress, 6);
  peerInfo.channel = 0;  // 🔴 CRITICAL: Setting to 0 uses the active hardware channel
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;
}

void loop() {
  if (millis() - lastSendTime >= 1000) {
    lastSendTime = millis();

    dataToSend.mq3_value = digitalRead(MQ3_PIN);
    dataToSend.ir_value = digitalRead(IR_PIN);

    Serial.printf("Sending -> MQ3: %d | IR: %d ... ", dataToSend.mq3_value, dataToSend.ir_value);
    esp_now_send(mainBoardAddress, (uint8_t *) &dataToSend, sizeof(dataToSend));
  }
}