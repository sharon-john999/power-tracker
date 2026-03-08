#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== USER SETTINGS =====
const char* ssid = "Meep";
const char* password = "whyarewestillhere";
String apiKey = "";

#define CURRENT_PIN 34
#define SAMPLES 1000
#define ADC_RESOLUTION 4095.0
#define VREF 3.3
#define MAINS_VOLTAGE 230.0
#define ACS_SENSITIVITY 0.100  // 20A version

float offsetVoltage = 0;
float energy_Wh = 0;
unsigned long lastMillis = 0;
unsigned long lastUpload = 0;

// ===== Offset Calibration =====
void calibrateOffset() {
  long sum = 0;
  for (int i = 0; i < 2000; i++) {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }
  float avg = sum / 2000.0;
  offsetVoltage = (avg / ADC_RESOLUTION) * VREF;
}

// ===== WiFi Connect =====
void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  connectWiFi();
  calibrateOffset();

  lastMillis = millis();
}

void loop() {

  float sumSquares = 0;

  for (int i = 0; i < SAMPLES; i++) {
    int adcValue = analogRead(CURRENT_PIN);
    float voltage = (adcValue / ADC_RESOLUTION) * VREF;
    float current = (voltage - offsetVoltage) / ACS_SENSITIVITY;
    sumSquares += current * current;
    delayMicroseconds(200);
  }

  float Irms = sqrt(sumSquares / SAMPLES);
  float power = MAINS_VOLTAGE * Irms;

  unsigned long now = millis();
  float deltaTime = (now - lastMillis) / 3600000.0;
  energy_Wh += power * deltaTime;
  lastMillis = now;

  // ===== OLED DISPLAY =====
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.print("I: ");
  display.print(Irms,3);
  display.println(" A");

  display.print("P: ");
  display.print(power,2);
  display.println(" W");

  display.print("E: ");
  display.print(energy_Wh,2);
  display.println(" Wh");

  display.display();

  // ===== ThingSpeak Upload (every 20 sec) =====
  if (millis() - lastUpload > 20000) {

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;

      String url = "http://api.thingspeak.com/update?api_key=" + apiKey +
                   "&field1=" + String(Irms,3) +
                   "&field2=" + String(power,2) +
                   "&field3=" + String(energy_Wh,2);

      http.begin(url);
      int httpResponseCode = http.GET();
      http.end();
    }

    lastUpload = millis();
  }

  delay(500);
}
