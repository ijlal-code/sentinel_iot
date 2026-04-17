#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <time.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Pin
#define TRIG 5
#define ECHO 18
#define BUZZER 23

#define FIREBASE_HOST "coba-b57da-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyBCpmeW6CkGOhhpOlKxBYqpkX_d4m45sFc"

LiquidCrystal_I2C lcd(0x27, 16, 2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

long duration;
float distance;

bool buzzerAllowed = true;

// 🔥 TEXT PER KONDISI
String textNear = "";
String textFar = "";

// NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;

String getTimeNow() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "-";

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  // 🔥 LCD STATUS WIFI
  lcd.setCursor(0,0);
  lcd.print("Menghubungkan");
  lcd.setCursor(0,1);
  lcd.print("WiFi...");

  WiFiManager wm;
  if (!wm.autoConnect("ESP32-PINTU")) {
    ESP.restart();
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");

  configTime(gmtOffset_sec, 0, ntpServer);

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {

  // SENSOR
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;

  String statusPintu = (distance < 50) ? "ADA ORANG" : "AMAN";

  // 🔥 AMBIL DATA DARI WEB
  Firebase.RTDB.getString(&fbdo, "/monitoring/buzzer_state");
  buzzerAllowed = fbdo.stringData() == "ON";

  Firebase.RTDB.getString(&fbdo, "/monitoring/text_near");
  textNear = fbdo.stringData();

  Firebase.RTDB.getString(&fbdo, "/monitoring/text_far");
  textFar = fbdo.stringData();

  // 🔥 REALTIME
  Firebase.RTDB.setFloat(&fbdo, "/monitoring/realtime/jarak", distance);
  Firebase.RTDB.setString(&fbdo, "/monitoring/realtime/status", statusPintu);

  // 🔥 HISTORY
  FirebaseJson json;
  json.set("jarak", distance);
  json.set("status", statusPintu);
  json.set("waktu", getTimeNow());
  Firebase.RTDB.pushJSON(&fbdo, "/monitoring/history", &json);

  // 🔥 LCD LOGIC
  lcd.clear();

  if (distance < 50) {
    lcd.setCursor(0,0);
    lcd.print(textNear != "" ? textNear : "ADA ORANG");

    if (!buzzerAllowed) {
      lcd.setCursor(0,1);
      lcd.print("BUZZER OFF");
    }
  } else {
    lcd.setCursor(0,0);
    lcd.print(textFar != "" ? textFar : "AMAN");
  }

  // 🔥 BUZZER
  if (distance < 50 && buzzerAllowed) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
  }

  delay(3000);
}