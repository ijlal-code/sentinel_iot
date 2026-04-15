#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // Wajib ditambahkan untuk koneksi HTTPS
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiManager.h>

#define TRIG 5
#define ECHO 18
#define BUZZER 23
#define RESET_PIN 0 // Menggunakan tombol BOOT bawaan ESP32 untuk reset WiFi

LiquidCrystal_I2C lcd(0x27, 16, 2);

long duration;
float distance;

// URL Server Hosting Anda (Sudah Fix)
const char* serverUrl = "https://sentinel.wuaze.com/api/kirim-data";

// Variabel untuk menyimpan izin menyalakan buzzer dari Web
bool buzzerAllowed = true; 

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RESET_PIN, INPUT_PULLUP); // Setup pin tombol reset

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Setup WiFi...");

  WiFiManager wm;

  // --- FITUR RESET WIFI ---
  // Jika tombol BOOT (Pin 0) ditahan saat ESP32 dinyalakan, WiFi akan di-reset
  // if (digitalRead(RESET_PIN) == LOW) {
  //   Serial.println("Tombol Reset Ditekan! Menghapus pengaturan WiFi...");
  //   lcd.clear();
  //   lcd.print("Reset WiFi...");
  //   wm.resetSettings(); // Menghapus credential WiFi yang tersimpan
  //   delay(2000);
  // }

  // OPSIONAL: Jika Anda ingin ESP32 SELALU minta WiFi baru setiap di-restart (tanpa tekan tombol),
  // hapus tanda // pada baris di bawah ini:
  wm.resetSettings(); 

  // Memulai portal WiFi (Nama WiFi: "PINTU_AP")
  if (!wm.autoConnect("PINTU_AP")) {
    Serial.println("Gagal terhubung dan timeout");
    delay(3000);
    ESP.restart();
  }

  lcd.clear();
  lcd.print("WiFi Connected!");
  Serial.println("WiFi Connected!");
}

void loop() {
  // 1. Baca Sensor Ultrasonik
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  duration = pulseIn(ECHO, HIGH);
  distance = duration * 0.034 / 2;
  
  String statusPintu = (distance < 50) ? "ADA ORANG" : "AMAN";

  // 2. KIRIM DATA KE LARAVEL & MINTA STATUS BUZZER
  if(WiFi.status() == WL_CONNECTED){
    
    // Konfigurasi HTTPS Aman namun di-set Insecure agar bypass cek sertifikat SSL (mempercepat proses)
    WiFiClientSecure client;
    client.setInsecure(); 

    HTTPClient http;
    http.begin(client, serverUrl); // Menggunakan client secure + URL HTTPS
    http.addHeader("Content-Type", "application/json");

    String httpRequestData = "{\"jarak\":" + String(distance) + ",\"status\":\"" + statusPintu + "\"}";
    
    // Kirim Data POST
    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
      String payload = http.getString();
      
      // Tampilkan balasan Server Web di Serial Monitor
      Serial.print("HTTP Code: "); Serial.println(httpResponseCode);
      Serial.println("Balasan Server: " + payload); 
      
      // Cek apakah ada kata "OFF" dari Web Laravel Anda
      if (payload.indexOf("\"OFF\"") > 0 || payload.indexOf("OFF") > 0) {
        buzzerAllowed = false; // Matikan Buzzer
      } else {
        buzzerAllowed = true;  // Nyalakan Buzzer
      }
    } else {
      Serial.print("Gagal Kirim Data. HTTP Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }

  // 3. TAMPILKAN KE LCD & BUNYIKAN BUZZER
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