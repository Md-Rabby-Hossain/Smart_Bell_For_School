#include <WiFi.h>
#include "time.h"

#define RELAY_PIN 23

// Wi-Fi credentials
const char* ssid     = "LSH_520";
const char* password = "25488135";

// NTP server details
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 21600;     // GMT+6 for Bangladesh
const int   daylightOffset_sec = 0;

bool togglingDone = false;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized.");
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    delay(1000);
    return;
  }

  // Print current time in hh:mm:ss format
  Serial.print("Current time: ");
  Serial.print(timeinfo.tm_hour);
  Serial.print(":");
  Serial.print(timeinfo.tm_min);
  Serial.print(":");
  Serial.println(timeinfo.tm_sec);

  // Relay toggle at 00:04:00
  if (timeinfo.tm_hour == 15 && timeinfo.tm_min == 8 && timeinfo.tm_sec == 0 && !togglingDone) {
    Serial.println("Starting relay toggle 3 times");

    for (int i = 0; i < 3; i++) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay ON");
      delay(2000);

      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Relay OFF");
      delay(2000);
    }

    togglingDone = true;
    Serial.println("Toggling finished for today.");
  }

  // Reset togglingDone at 00:00:00
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 13 && timeinfo.tm_sec == 0) {
    togglingDone = false;
    Serial.println("Toggling reset for new day.");
  }

  delay(1000);  // Check every second
}
