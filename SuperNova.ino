#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <BluetoothSerial.h>

RTC_DS3231 rtc;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
BluetoothSerial BT;

// Pins
#define RELAY_PIN 23
#define BUTTON_UP 12
#define BUTTON_DOWN 14
#define BUTTON_OK 27
#define BUTTON_RESET 26

#define EEPROM_SIZE 64

int weekdayBellTimes[8][2];
int saturdayBellTimes[5][2];   // Saturday (5 times)

int settingIndex = 0, subSetting = 0;
bool settingMode = false, settingSaturday = true, daySelectMode = false;

bool showingAllTimes = false, showingSaturdayTimes = true;
unsigned long showingStartTime = 0, scrollTime = 0;
int scrollOffset = 0;

unsigned long resetButtonPressedTime = 0;
bool resetButtonHeld = false;

DateTime now;
const char* weekdayLabels[8] = {"Assembly", "SC", "1st P.", "2nd P.", "T.", "T.P.O.", "4th P.", "5th P."};
const char* saturdayLabels[5] = {"Assembly", "SC", "1st P.", "2nd P.", "T."};
const char* daysOfWeek[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!rtc.begin()) while (1);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);

  BT.begin("SmartBell");  // Bluetooth setup


  // Show welcome message first
 display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10,10); display.println("MADE BY");
  display.setTextSize(2);
  display.setCursor(10,25); display.println("GOLAM");
  display.setCursor(10,45); display.println("RABBY");
  display.display(); delay(2000);

  EEPROM.begin(EEPROM_SIZE); loadBellTimes();

  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
}

void loop() {
  now = rtc.now();
  checkBluetooth();
  checkButtons();

  if (showingAllTimes) {
    showStoredTimes();
  } else if (!settingMode && !daySelectMode) {
    updateDisplay();
    checkSchedule();
  }
}

void checkBluetooth() {
  while (BT.available()) {
    char c = BT.read();
    if (c == 49) digitalWrite(RELAY_PIN, LOW);
    else if (c == 48) digitalWrite(RELAY_PIN, HIGH);
  }
}

void checkButtons() {
  static unsigned long lastDebounce = 0;
  const int debounceDelay = 200;

  if (millis() - lastDebounce < debounceDelay) return;

  // Check reset button for press+hold or press
  if (digitalRead(BUTTON_RESET) == LOW) {
    if (!resetButtonHeld) {
      resetButtonPressedTime = millis();
      resetButtonHeld = true;
    } else {
      if (millis() - resetButtonPressedTime >= 2000) {
        // Long press detected: reset EEPROM
        resetEEPROM();
        displayMessage("Reset Done!", 1500);
        loadBellTimes();
        showingAllTimes = false;
        settingMode = false;
        daySelectMode = false;
        resetButtonHeld = false;
        lastDebounce = millis();
        return;
      }
    }
  } else {
    if (resetButtonHeld) {
      // Button was released before 2 sec = short press
      if (millis() - resetButtonPressedTime < 2000) {
        // Show stored times on short press
        showingAllTimes = true;
        showingSaturdayTimes = true;
        showingStartTime = millis();
      }
      resetButtonHeld = false;
      lastDebounce = millis();
      return;
    }
  }

  if (daySelectMode) {
    if (digitalRead(BUTTON_UP) == LOW || digitalRead(BUTTON_DOWN) == LOW) {
      settingSaturday = !settingSaturday;
      displayDaySelect();
      lastDebounce = millis();
    }
    if (digitalRead(BUTTON_OK) == LOW) {
      daySelectMode = false;
      settingMode = true;
      settingIndex = 0;
      subSetting = 0;
      displaySetting();
      lastDebounce = millis();
    }
    return;
  }

  if (settingMode) {
    if (digitalRead(BUTTON_UP) == LOW) {
      adjustTime(1);
      lastDebounce = millis();
    }
    if (digitalRead(BUTTON_DOWN) == LOW) {
      adjustTime(-1);
      lastDebounce = millis();
    }
    if (digitalRead(BUTTON_OK) == LOW) {
      nextSetting();
      lastDebounce = millis();
    }
    return;
  }

  // If showing stored times, any button press exits that mode
  if (showingAllTimes) {
    if (digitalRead(BUTTON_UP) == LOW ||
        digitalRead(BUTTON_DOWN) == LOW ||
        digitalRead(BUTTON_OK) == LOW) {
      showingAllTimes = false;
      lastDebounce = millis();
      return;
    }
  }

  // Not in setting or showingAllTimes mode
  if (digitalRead(BUTTON_UP) == LOW) {
    daySelectMode = true;
    displayDaySelect();
    lastDebounce = millis();
  }
}

void adjustTime(int delta) {
  if (settingSaturday) {
    if (subSetting == 0) {
      saturdayBellTimes[settingIndex][0] += delta;
      if (saturdayBellTimes[settingIndex][0] > 23) saturdayBellTimes[settingIndex][0] = 0;
      if (saturdayBellTimes[settingIndex][0] < 0) saturdayBellTimes[settingIndex][0] = 23;
    } else {
      saturdayBellTimes[settingIndex][1] += delta;
      if (saturdayBellTimes[settingIndex][1] > 59) saturdayBellTimes[settingIndex][1] = 0;
      if (saturdayBellTimes[settingIndex][1] < 0) saturdayBellTimes[settingIndex][1] = 59;
    }
  } else {
    if (subSetting == 0) {
      weekdayBellTimes[settingIndex][0] += delta;
      if (weekdayBellTimes[settingIndex][0] > 23) weekdayBellTimes[settingIndex][0] = 0;
      if (weekdayBellTimes[settingIndex][0] < 0) weekdayBellTimes[settingIndex][0] = 23;
    } else {
      weekdayBellTimes[settingIndex][1] += delta;
      if (weekdayBellTimes[settingIndex][1] > 59) weekdayBellTimes[settingIndex][1] = 0;
      if (weekdayBellTimes[settingIndex][1] < 0) weekdayBellTimes[settingIndex][1] = 59;
    }
  }
  displaySetting();
}

void nextSetting() {
  subSetting = 1 - subSetting;
  if (subSetting == 0) {
    settingIndex++;
    int limit = settingSaturday ? 5 : 8;
    if (settingIndex >= limit) {
      saveBellTimes();
      settingMode = false;
      displayMessage("Saved!", 1500);
      return;
    }
  }
  displaySetting();
}

void displaySetting() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Setting time for ");
  display.println(settingSaturday ? "Saturday" : "Sun-Thu");
  display.print("Set time for ");
  display.println(settingSaturday ? saturdayLabels[settingIndex] : weekdayLabels[settingIndex]);

  int h = settingSaturday ? saturdayBellTimes[settingIndex][0] : weekdayBellTimes[settingIndex][0];
  int m = settingSaturday ? saturdayBellTimes[settingIndex][1] : weekdayBellTimes[settingIndex][1];

  display.print("Hour: ");
  display.println(h);
  display.print("Min:  ");
  display.println(m);
  display.display();
}

void displayDaySelect() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Select Day to Set:");
  display.setCursor(0,20);
  display.println(settingSaturday ? "> Saturday" : "  Saturday");
  display.setCursor(0,30);
  display.println(!settingSaturday ? "> Sun-Thu" : "  Sun-Thu");
  display.display();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Day and Date
  display.setCursor(0,0);
  display.print(daysOfWeek[now.dayOfTheWeek()]);
  display.print(" ");
  display.print(now.day());
  display.print("/");
  display.print(now.month());
  display.print("/");
  display.print(now.year());

  // Time
  display.setCursor(0,12);
  display.print("TIME: ");
  printTime(now.hour(), now.minute(), now.second());

  // Mode
  display.setCursor(0,24);
  display.print("MODE: ");
  display.println(now.dayOfTheWeek() == 6 ? "Saturday" : "Sun-Thu");

  // Next Bell Time (NBT)
  display.setCursor(0,36);
  display.print("NEXT: ");
  String nbt = getNextBellTimeString();
  display.print(nbt);

  display.display();
}

String getNextBellTimeString() {
  int limit = (now.dayOfTheWeek() == 6) ? 5 : 8;
  int (*bellTimes)[2];
  if (now.dayOfTheWeek() == 6)
    bellTimes = saturdayBellTimes;
  else
    bellTimes = weekdayBellTimes;

  for (int i = 0; i < limit; i++) {
    int h = bellTimes[i][0];
    int m = bellTimes[i][1];
    if (h == 0 && m == 0) continue; // Skip if not set

    if (h > now.hour() || (h == now.hour() && m > now.minute())) {
      char buf[6];
      sprintf(buf, "%02d:%02d", h, m);
      return String(buf);
    }
  }
  return "--:--";
}

void printTime(int h, int m, int s) {
  if (h < 10) display.print("0");
  display.print(h);
  display.print(":");
  if (m < 10) display.print("0");
  display.print(m);
  display.print(":");
  if (s < 10) display.print("0");
  display.print(s);
}

void checkSchedule() {
  if (now.second() != 0) return;

  if (now.dayOfTheWeek() == 6) {
    for (int i = 0; i < 5; i++) {
      if (now.hour() == saturdayBellTimes[i][0] && now.minute() == saturdayBellTimes[i][1]) {
        triggerBell(i);
      }
    }
  } else {
    for (int i = 0; i < 8; i++) {
      if (now.hour() == weekdayBellTimes[i][0] && now.minute() == weekdayBellTimes[i][1]) {
        triggerBell(i);
      }
    }
  }
}

void triggerBell(int index) {
  switch(index) {
    case 0: // Assembly - 10 sec
      digitalWrite(RELAY_PIN, LOW);
      delay(10000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
    case 1: // SC - 5 sec
      digitalWrite(RELAY_PIN, LOW);
      delay(5000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
    case 2: // 1st P - 1 ring
      ringPattern(1);
      break;
    case 3: // 2nd P - 2 rings
      ringPattern(2);
      break;
    case 4: // T. (Tiffin) - 3 rings + 5 sec continuous
      ringPattern(3);
      digitalWrite(RELAY_PIN, LOW);
      delay(5000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
    case 5: // T.P.O. - 3 rings + 5 sec continuous
      ringPattern(3);
      digitalWrite(RELAY_PIN, LOW);
      delay(5000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
    case 6: // 4th P - 4 rings
      ringPattern(4);
      break;
    case 7: // 5th P - 5 rings + 10 sec continuous
      ringPattern(5);
      digitalWrite(RELAY_PIN, LOW);
      delay(10000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
  }
}

void ringPattern(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(RELAY_PIN, LOW);
    delay(100);
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
  }
}

void saveBellTimes() {
  for (int i = 0; i < 5; i++) {
    EEPROM.write(i * 2, saturdayBellTimes[i][0]);
    EEPROM.write(i * 2 + 1, saturdayBellTimes[i][1]);
  }
  for (int i = 0; i < 8; i++) {
    EEPROM.write(i * 2 + 10, weekdayBellTimes[i][0]);
    EEPROM.write(i * 2 + 11, weekdayBellTimes[i][1]);
  }
  EEPROM.commit();
}

void loadBellTimes() {
  for (int i = 0; i < 5; i++) {
    int h = EEPROM.read(i * 2);
    int m = EEPROM.read(i * 2 + 1);
    if (h > 23) h = 0;
    if (m > 59) m = 0;
    saturdayBellTimes[i][0] = h;
    saturdayBellTimes[i][1] = m;
  }
  for (int i = 0; i < 8; i++) {
    int h = EEPROM.read(i * 2 + 10);
    int m = EEPROM.read(i * 2 + 11);
    if (h > 23) h = 0;
    if (m > 59) m = 0;
    weekdayBellTimes[i][0] = h;
    weekdayBellTimes[i][1] = m;
  }
}

// Reset all EEPROM data to 0xFF (empty)
void resetEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 255);
  EEPROM.commit();
}

// Display all saved bell times (Saturday then Sun-Thu)
void showStoredTimes() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (showingSaturdayTimes) {
    display.println("Stored Times: Saturday");
    display.println();
    for (int i = 0; i < 5; i++) {
      display.print(saturdayLabels[i]);
      display.print(": ");
      printPaddedTime(display, saturdayBellTimes[i][0], saturdayBellTimes[i][1]);
      display.println();
    }
    display.display();

    if (millis() - showingStartTime > 10000) {
      showingSaturdayTimes = false;
      showingStartTime = millis();
    }
  } else {
    display.println("Stored Times: Sun-Thu");
    display.println();
    for (int i = 0; i < 8; i++) {
      display.print(weekdayLabels[i]);
      display.print(": ");
      printPaddedTime(display, weekdayBellTimes[i][0], weekdayBellTimes[i][1]);
      display.println();
    }
    display.display();

    if (millis() - showingStartTime > 10000) {
      // Hold here until another button press exits showingAllTimes mode
    }
  }
}

void printPaddedTime(Adafruit_SSD1306 &disp, int h, int m) {
  if (h < 10) disp.print("0");
  disp.print(h);
  disp.print(":");
  if (m < 10) disp.print("0");
  disp.print(m);
}
  
void displayMessage(const char* msg, int duration) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(msg);
  display.display();
  delay(duration);
}

