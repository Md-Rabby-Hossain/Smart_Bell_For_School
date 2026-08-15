#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

RTC_DS3231 rtc;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

#define RELAY_PIN 23
#define BUTTON_UP 34
#define BUTTON_DOWN 35
#define BUTTON_OK 32
#define BUTTON_RESET 33

#define EEPROM_SIZE 20

int bellTimes[7][2]; // 7 events, [hour, min]
int settingIndex = 0;
int subSetting = 0;
bool settingMode = false;

unsigned long lastDebounce = 0;
const int debounceDelay = 300;

DateTime now;
const char* labels[7] = {"S.S.", "1st P.", "2nd P.", "T.", "T.P.O.", "4th P.", "5th P."};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  if (!rtc.begin()) while (1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (1);
  display.clearDisplay();
  display.display();

  EEPROM.begin(EEPROM_SIZE);
  loadBellTimes();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_OK, INPUT);
  pinMode(BUTTON_RESET, INPUT);

  displayMessage("MADE BY\nGOLAM RABBY", 2000);
}

void loop() {
  now = rtc.now();
  checkButtons();

  if (!settingMode) {
    updateDisplay();
    checkSchedule();
  }
}

void checkButtons() {
  if (millis() - lastDebounce < debounceDelay) return;

  if (digitalRead(BUTTON_RESET) == HIGH) {
    resetEEPROM();
    displayMessage("Reset Done!", 1500);
    loadBellTimes();
    lastDebounce = millis();
  }

  if (settingMode) {
    if (digitalRead(BUTTON_UP) == HIGH) {
      adjustTime(1);
      lastDebounce = millis();
    }
    if (digitalRead(BUTTON_DOWN) == HIGH) {
      adjustTime(-1);
      lastDebounce = millis();
    }
    if (digitalRead(BUTTON_OK) == HIGH) {
      nextSetting();
      lastDebounce = millis();
    }
  } else {
    if (digitalRead(BUTTON_UP) == HIGH) {
      settingMode = true;
      settingIndex = 0;
      subSetting = 0;
      displaySetting();
      lastDebounce = millis();
    }
  }
}

void adjustTime(int delta) {
  if (subSetting == 0) {
    bellTimes[settingIndex][0] += delta;
    if (bellTimes[settingIndex][0] > 23) bellTimes[settingIndex][0] = 0;
    if (bellTimes[settingIndex][0] < 0) bellTimes[settingIndex][0] = 23;
  } else {
    bellTimes[settingIndex][1] += delta;
    if (bellTimes[settingIndex][1] > 59) bellTimes[settingIndex][1] = 0;
    if (bellTimes[settingIndex][1] < 0) bellTimes[settingIndex][1] = 59;
  }
  displaySetting();
}

void nextSetting() {
  if (subSetting == 0) {
    subSetting = 1;
  } else {
    subSetting = 0;
    settingIndex++;
    if (settingIndex >= 7) {
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
  display.print("Set time for ");
  display.println(labels[settingIndex]);
  display.print("Hour: ");
  display.println(bellTimes[settingIndex][0]);
  display.print("Minute: ");
  display.println(bellTimes[settingIndex][1]);
  display.display();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(now.day(), DEC);
  display.print('/');
  display.print(now.month(), DEC);
  display.print('/');
  display.print(now.year(), DEC);
  display.setCursor(0,12);
  display.print("Time: ");
  printTime(now.hour(), now.minute(), now.second());
  display.setCursor(0,30);
  display.print("NBT: ");
  int nh, nm;
  getNextBell(nh, nm);
  printTime(nh, nm, 0);
  display.display();
}

void printTime(int h, int m, int s) {
  if (h<10) display.print("0");
  display.print(h); display.print(":");
  if (m<10) display.print("0");
  display.print(m); display.print(":");
  if (s<10) display.print("0");
  display.print(s);
}

void checkSchedule() {
  if (now.second() != 0) return;

  for (int i=0; i<7; i++) {
    if (now.hour() == bellTimes[i][0] && now.minute() == bellTimes[i][1]) {
      ringBell(i);
    }
  }
}

void ringBell(int index) {
  switch(index) {
    case 0: // SS
      digitalWrite(RELAY_PIN, LOW);
      delay(10000);
      digitalWrite(RELAY_PIN, HIGH);
      break;
    case 1: ringPattern(1); break;
    case 2: ringPattern(2); break;
    case 3: ringPattern(3); delay(5000); break;
    case 4: ringPattern(3); delay(5000); break;
    case 5: ringPattern(4); break;
    case 6: ringPattern(5); delay(10000); break;
  }
}

void ringPattern(int times) {
  for (int i=0; i<times; i++) {
    digitalWrite(RELAY_PIN, LOW);
    delay(100);
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
  }
}

void saveBellTimes() {
  for (int i=0; i<7; i++) {
    EEPROM.write(i*2, bellTimes[i][0]);
    EEPROM.write(i*2+1, bellTimes[i][1]);
  }
  EEPROM.commit();
}

void loadBellTimes() {
  for (int i=0; i<7; i++) {
    bellTimes[i][0] = EEPROM.read(i*2);
    bellTimes[i][1] = EEPROM.read(i*2+1);
  }
}

void resetEEPROM() {
  for (int i=0; i<EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
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

void getNextBell(int &nh, int &nm) {
  nh = 99; nm = 99;
  for (int i=0; i<7; i++) {
    if (bellTimes[i][0] > now.hour() || 
        (bellTimes[i][0]==now.hour() && bellTimes[i][1]>now.minute())) {
      if (bellTimes[i][0]<nh || (bellTimes[i][0]==nh && bellTimes[i][1]<nm)) {
        nh = bellTimes[i][0];
        nm = bellTimes[i][1];
      }
    }
  }
  if (nh==99) { nh=bellTimes[0][0]; nm=bellTimes[0][1]; }
}
