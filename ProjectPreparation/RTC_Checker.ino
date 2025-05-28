#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

const int relayPin = 13; // GPIO 13 for Relay

// Define bell times: {hour, minute}
const int bellTimes[10][2] = {
  {0, 30},
  {0, 31},
  {0, 32},
  {0, 33},
  {0, 34},
  {0, 35},
  {0, 36},
  {0, 37},
  {0, 38},
  {0, 39}
};

bool bellRung[10]; // To prevent multiple triggers in the same minute

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize bellRung array to false
  for (int i = 0; i < 10; i++) {
    bellRung[i] = false;
  }
}

void loop() {
  DateTime now = rtc.now();

  Serial.print("Time: ");
  Serial.print(now.hour());
  Serial.print(':');
  Serial.print(now.minute());
  Serial.print(':');
  Serial.println(now.second());

  for (int i = 0; i < 10; i++) {
    if (now.hour() == bellTimes[i][0] && now.minute() == bellTimes[i][1] && !bellRung[i]) {
      Serial.print("Bell ringing at ");
      Serial.print(now.hour());
      Serial.print(':');
      Serial.println(now.minute());

      digitalWrite(relayPin, HIGH); // Relay ON
      delay(5000);                  // Ring for 5 seconds
      digitalWrite(relayPin, LOW);  // Relay OFF

      bellRung[i] = true; // Mark as rung for this minute
    }

    // Reset bellRung flag when minute changes
    if (now.minute() != bellTimes[i][1]) {
      bellRung[i] = false;
    }
  }

  delay(1000); // Check every second
}
