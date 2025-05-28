#define RELAY_PIN 23

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
}

void loop() {
  // Turn relay ON
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Relay ON");
  delay(200);  // Wait for 10 seconds

  // Turn relay OFF
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Relay OFF");
  delay(200);  // Wait for 10 seconds
}
