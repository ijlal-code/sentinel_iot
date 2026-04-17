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

float distance;
bool buzzerAllowed = true;

String textNear = "";
String textFar = "";

// 🔥 fungsi split teks ke LCD
void printLCD(String text){
  lcd.clear();

  String line1 = text.substring(0,16);
  String line2 = "";

  if(text.length() > 16){
    line2 = text.substring(16,32);
  }

  lcd.setCursor(0,0);
  lcd.print(line1);

  lcd.setCursor(0,1);
  lcd.print(line2);
}

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

  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();

  printLCD("Menghubungkan WiFi...");

  WiFiManager wm;
  if (!wm.autoConnect("ESP32-PINTU")) {
    ESP.restart();
  }

  printLCD("WiFi Connected");

  configTime(gmtOffset_sec, 0, ntpServer);

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {

  // sensor
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;

  String status = (distance < 50) ? "ADA ORANG" : "AMAN";

  Firebase.RTDB.getString(&fbdo, "/monitoring/buzzer_state");
  buzzerAllowed = fbdo.stringData() == "ON";

  Firebase.RTDB.getString(&fbdo, "/monitoring/text_near");
  textNear = fbdo.stringData();

  Firebase.RTDB.getString(&fbdo, "/monitoring/text_far");
  textFar = fbdo.stringData();

  Firebase.RTDB.setFloat(&fbdo, "/monitoring/realtime/jarak", distance);
  Firebase.RTDB.setString(&fbdo, "/monitoring/realtime/status", status);

  FirebaseJson json;
  json.set("jarak", distance);
  json.set("status", status);
  json.set("waktu", getTimeNow());
  Firebase.RTDB.pushJSON(&fbdo, "/monitoring/history", &json);

  // LCD
  if(distance < 50){
    printLCD(textNear != "" ? textNear : "ADA ORANG");

    if(!buzzerAllowed){
      lcd.setCursor(0,1);
      lcd.print("BUZZER OFF");
    }

    // 🔥 buzzer 3x
    if(buzzerAllowed){
      for(int i=0;i<3;i++){
        digitalWrite(BUZZER, HIGH);
        delay(200);
        digitalWrite(BUZZER, LOW);
        delay(200);
      }
    }

  }else{
    printLCD(textFar != "" ? textFar : "AMAN");
  }

  delay(3000);
}