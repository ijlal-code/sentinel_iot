#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

#define TRIG 5
#define ECHO 18
#define BUZZER 23

LiquidCrystal_I2C lcd(0x27, 16, 2);

long duration;
float distance;
char serverIp[40] = "192.168.43.204"; 

// Variabel untuk menyimpan izin menyalakan buzzer dari Web
bool buzzerAllowed = true; 

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Setup WiFi...");

  WiFiManager wm;
  WiFiManagerParameter custom_server_ip("server", "IP Server Laravel", serverIp, 40);
  wm.addParameter(&custom_server_ip);

  if (!wm.autoConnect("PINTU")) {
    Serial.println("Gagal terhubung dan timeout");
    delay(3000);
    ESP.restart();
  }

  lcd.clear();
  lcd.print("WiFi Connected!");
  strcpy(serverIp, custom_server_ip.getValue());
  Serial.print("IP Server: "); Serial.println(serverIp);
}

void loop() {
  // 1. Baca Sensor Ultrasonik
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;
  
  String statusPintu = (distance < 50) ? "ADA ORANG" : "AMAN";

  // 2. KIRIM DATA KE LARAVEL DULU & MINTA STATUS BUZZER
  if(WiFi.status() == WL_CONNECTED){
    HTTPClient http;
    String serverName = "http://" + String(serverIp) + ":8000/api/kirim-data";
    
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String httpRequestData = "{\"jarak\":" + String(distance) + ",\"status\":\"" + statusPintu + "\"}";
    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      // Tampilkan balasan Laravel di Serial Monitor untuk memastikan
      Serial.println("Balasan Server: " + payload); 
      
      // Cek apakah ada kata "OFF" dari Laravel
      if (payload.indexOf("\"OFF\"") > 0) {
        buzzerAllowed = false; // Matikan Buzzer
      } else {
        buzzerAllowed = true;  // Nyalakan Buzzer
      }
    }
    http.end();
  }

  // 3. BARU TAMPILKAN KE LCD & BUNYIKAN BUZZER (Sesuai Izin Server)
  if (distance < 50) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Ada Orang!");

    if (buzzerAllowed) {
      // Jika diizinkan (ON), bunyi 3x
      for(int i=0; i<3; i++){
        digitalWrite(BUZZER, HIGH);
        delay(200);
        digitalWrite(BUZZER, LOW);
        delay(200);
      }
    } else {
      // Jika tidak diizinkan (OFF/Mute), tampilkan pesan
      lcd.setCursor(0,1);
      lcd.print("Buzzer Di-Mute");
      delay(1200); // Waktu ganti buzzer yang tidak bunyi
    }

  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Aman");
  }

  delay(3000); // Jeda sebelum mendeteksi lagi
}