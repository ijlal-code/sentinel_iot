#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>

// Helper Firebase
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Pin
#define TRIG 5
#define ECHO 18
#define BUZZER 23
#define RESET_PIN 0

// Firebase (SUDAH DISESUAIKAN)
#define FIREBASE_HOST "coba-b57da-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyBCpmeW6CkGOhhpOlKxBYqpkX_d4m45sFc"

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Firebase object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variabel
long duration;
float distance;
bool buzzerAllowed = true;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RESET_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("Setup WiFi...");

  WiFiManager wm;

  // Reset WiFi kalau tombol ditekan
  if (digitalRead(RESET_PIN) == LOW) {
    lcd.clear();
    lcd.print("Reset WiFi...");
    wm.resetSettings();
    delay(2000);
  }

  // Auto connect WiFi
  if (!wm.autoConnect("ESP32-PINTU")) {
    ESP.restart();
  }

  lcd.clear();
  lcd.print("WiFi Connected!");

  // Setup Firebase
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Terhubung");
  } else {
    Serial.printf("Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Default buzzer ON
  Firebase.RTDB.setString(&fbdo, "/monitoring/buzzer_state", "ON");
}

void loop() {
  // Baca sensor ultrasonik
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;

  String statusPintu = (distance < 50) ? "ADA ORANG" : "AMAN";

  // Kirim ke Firebase
  if (Firebase.ready()) {
    Firebase.RTDB.setFloat(&fbdo, "/monitoring/jarak", distance);
    Firebase.RTDB.setString(&fbdo, "/monitoring/status", statusPintu);

    // Ambil status buzzer dari web
    if (Firebase.RTDB.getString(&fbdo, "/monitoring/buzzer_state")) {
      String state = fbdo.stringData();
      buzzerAllowed = (state == "OFF") ? false : true;
    }
  }

  // Tampilkan ke LCD + buzzer
  lcd.clear();

  if (distance < 50) {
    lcd.setCursor(0,0);
    lcd.print("ADA ORANG!");

    if (buzzerAllowed) {
      for(int i = 0; i < 3; i++){
        digitalWrite(BUZZER, HIGH);
        delay(200);
        digitalWrite(BUZZER, LOW);
        delay(200);
      }
    } else {
      lcd.setCursor(0,1);
      lcd.print("BUZZER OFF");
      delay(800);
    }

  } else {
    lcd.setCursor(0,0);
    lcd.print("AMAN");
    digitalWrite(BUZZER, LOW);
  }

  delay(500); // lebih cepat realtime
}