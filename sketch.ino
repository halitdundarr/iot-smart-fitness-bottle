#include "thingProperties.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <WiFi.h>

// Hardware Definitions
Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Pin Assignments
const int fsrPin = 34;
const int buzzerPin = 25;

// --- HASSAS KALİBRASYON EŞİKLERİ ---
const int gripThreshold = 300;
const float xDownLimit = 9.0;    // 9 ve üzeri dambıl aşağıda kabul edilir
const float xUpLimit = 1.0;      // 1 ve altı dambıl yukarıda kabul edilir
const float zUpThreshold = -6.5; // Yukarıdayken Z (-7, -8 arası)
const float yFormLimit = 3; // Y ekseni formu (Sola/Sağa fazla açılma sınırı)

// State Tracking
bool isGoingUp = false;
bool formError = false;

void setup() {
  Serial.begin(9600);
  delay(1500);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  pinMode(buzzerPin, OUTPUT);

  if (!mpu.begin()) {
    Serial.println("MPU6050 Error!");
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Error!");
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  ArduinoCloud.update(); // Bulut senkronizasyonu

  int fsrValue = analogRead(fsrPin);
  gripStatus = (fsrValue > gripThreshold); //

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // 1. Form Kontrolü ve Otomatik Durum Sıfırlama
  if (abs(a.acceleration.y) > yFormLimit) {
    formError = true;
    motionState = "BAD FORM!"; // Hata varsa buluta yaz
  } else {
    formError = false;
    // KRİTİK DÜZELTME: Eğer form az önce hatalıydıysa ve şimdi düzelmişse,
    // durumu mevcut harekete göre hemen güncelle (Bulutta takılı kalmasın)
    if (motionState == "BAD FORM!") {
      if (!gripStatus)
        motionState = "IDLE";
      else
        motionState = isGoingUp ? "UP" : "READY";
    }
  }

  // 2. Hareket Sayma Mantığı
  if (gripStatus && !formError) {

    // YUKARI (UP) KONTROLÜ
    if (a.acceleration.x <= xUpLimit && a.acceleration.z <= zUpThreshold &&
        !isGoingUp) {
      isGoingUp = true;
      motionState = "UP"; //
    }

    // AŞAĞI (DOWN) KONTROLÜ
    else if (a.acceleration.x >= xDownLimit && isGoingUp) {
      repetitionCount++; // Bir tekrar tamamlandı
      isGoingUp = false;
      motionState = "DOWN"; //

      // Hedef kontrolü ve Buzzer
      if (repetitionCount >= targetReps && buzzerEnabled) {
        digitalWrite(buzzerPin, HIGH);
        delay(300);
        digitalWrite(buzzerPin, LOW);
      }
    }
  }
  // Eğer tutuş yoksa her şeyi sıfırla
  else if (!gripStatus) {
    motionState = "IDLE"; //
    isGoingUp = false;
  }

  updateOLED();
  delay(50);
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Reps: ");
  display.print(repetitionCount);
  display.print("/");
  display.print(targetReps);

  display.setCursor(0, 15);
  display.print("Grip: ");
  display.print(gripStatus ? "ON" : "OFF");

  display.setCursor(0, 30);
  // Ekranda her zaman anlık durum neyse o görünsün
  if (formError) {
    display.print("FORM: CHECK ARM!");
  } else {
    display.print("Motion: ");
    display.print(motionState);
  }

  display.setCursor(0, 45);
  display.print("Mode: Curl  Bz:");
  display.print(buzzerEnabled ? "ON" : "OFF");

  display.setCursor(0, 55);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi:");
    display.print(WiFi.RSSI());
    display.print("dB ");
  } else {
    display.print("WiFi:WAIT  ");
  }
  display.print(ArduinoCloud.connected() ? "Cld:OK" : "Cld:WAIT");

  display.display();
}

// Cloud Callbacks
void onResetCounterChange() {
  if (resetCounter) {
    repetitionCount = 0;
    resetCounter = false;
  }
}
void onTargetRepsChange() {}
void onBuzzerEnabledChange() {}
void onMotionStateChange() {}