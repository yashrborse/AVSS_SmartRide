#include <esp_now.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- CREDENTIALS ---
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
#define CHAT_ID "YOUR_CHAT_ID_HERE"
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD"; 

// --- PINS ---
#define CONTROL_PIN 25    
#define EMERGENCY_PIN 15  
#define ONBOARD_LED 2     
#define RXD2 16 // GPS TX goes here
#define TXD2 17 // GPS RX goes here

// --- OBJECTS ---
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
TinyGPSPlus gps;
Adafruit_MPU6050 mpu;

typedef struct struct_message {
  int mq3_value;
  int ir_value;
} struct_message;

struct_message receivedData;
unsigned long lastTelegramTime = 0;
unsigned long lastReceiveTime = 0;
bool sensorConnected = false;

// 📡 ESP-NOW RECEIVER
void OnDataRecv(const esp_now_recv_info* info, const uint8_t* incomingData, int len) {
  lastReceiveTime = millis(); 
  if (!sensorConnected) {
    Serial.println("✅ ESP-NOW LINK CONNECTED!");
    sensorConnected = true;
  }
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  
  if (receivedData.mq3_value == 0 && receivedData.ir_value == 1) {
    digitalWrite(CONTROL_PIN, HIGH); 
    digitalWrite(ONBOARD_LED, HIGH); 
  } else {
    digitalWrite(CONTROL_PIN, LOW);  
    digitalWrite(ONBOARD_LED, LOW);  
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // Start GPS Serial
  
  pinMode(CONTROL_PIN, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(EMERGENCY_PIN, INPUT_PULLUP);
  digitalWrite(CONTROL_PIN, LOW); 
  digitalWrite(ONBOARD_LED, LOW); 

  // 1. INIT MPU6050
  if (!mpu.begin()) {
    Serial.println("❌ Failed to find MPU6050 chip! Check wiring.");
  } else {
    Serial.println("✅ MPU6050 Found!");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  }

  // 2. CONNECT TO WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  WiFi.setSleep(false); // Disable sleep for ESP-NOW stability
  Serial.println("\n✅ WIFI CONNECTED!");

  // 3. START ESP-NOW
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }
  
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  configTime(0, 0, "pool.ntp.org"); 
}

void sendEmergencyAlert(String reason) {
  unsigned long currentMillis = millis();
  if (currentMillis - lastTelegramTime > 15000) { // 15 sec cooldown
    Serial.println("🚨 SENDING TELEGRAM ALERT...");
    
    String message = "🚨 EMERGENCY ALERT: " + reason + "!\n";
    
    // Check if GPS has a lock
    if (gps.location.isValid()) {
      message += "📍 Live Location: https://maps.google.com/?q=";
      message += String(gps.location.lat(), 6) + ",";
      message += String(gps.location.lng(), 6);
    } else {
      message += "📍 Location: GPS is still searching for satellites...";
    }

    bot.sendMessage(CHAT_ID, message, "");
    lastTelegramTime = currentMillis;
  }
}

void loop() {
  // 1. READ GPS DATA IN BACKGROUND
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // 2. READ MPU6050 DATA
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // 3. CRASH DETECTION LOGIC
  // Standard gravity is 9.8 m/s^2. 
  // If X or Y acceleration exceeds 15 (hard impact or sudden fall)
  // OR if the Z axis (up/down) drops near 0 (meaning the bike is laying flat on its side)
  bool hardImpact = (abs(a.acceleration.x) > 15.0 || abs(a.acceleration.y) > 15.0);
  bool bikeFallen = (abs(a.acceleration.z) < 3.0); // Normal is ~9.8. < 3 means it's heavily tilted.

  if (hardImpact || bikeFallen) {
    sendEmergencyAlert("Accident / Fall Detected by Gyroscope");
  }

  // 4. MANUAL EMERGENCY PIN
  if (digitalRead(EMERGENCY_PIN) == LOW) {
    sendEmergencyAlert("Manual Emergency Button Pressed");
  }

  // 5. ESP-NOW DISCONNECT FAILSAFE
  if (sensorConnected && (millis() - lastReceiveTime > 3000)) {
    sensorConnected = false;
    digitalWrite(CONTROL_PIN, LOW); 
    digitalWrite(ONBOARD_LED, LOW); 
  }
}