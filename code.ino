#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =======================
// PIN DEFINITIONS
// =======================

// Sensors
#define DHT_PIN 14
#define DHT_TYPE DHT22

#define ULTRASONIC_TRIG 26
#define ULTRASONIC_ECHO 13

#define LDR_PIN 34
#define MQ135_PIN 35

#define PIR_PIN 18
#define EMERGENCY_BUTTON 19

// Outputs
#define RED_LED 21
#define YELLOW_LED 22
#define GREEN_LED 23
#define STREETLIGHT_LED 25
#define BUZZER_PIN 27

// OLED
#define OLED_SDA 4
#define OLED_SCL 5
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// =======================
// OBJECTS
// =======================

DHT dht(DHT_PIN, DHT_TYPE);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =======================
// THRESHOLDS
// =======================

int lightThreshold = 1800;       // Lower/higher may need adjustment
int trafficDistanceHigh = 20;    // cm
int trafficDistanceMedium = 50;  // cm
int airQualityThreshold = 2200;  // MQ135 analog rough threshold

// =======================
// VARIABLES
// =======================

float temperature = 0;
float humidity = 0;
long distanceCM = 0;
int lightValue = 0;
int airQualityValue = 0;
int pirState = 0;
int emergencyState = 1;

String trafficStatus = "LOW";
String airStatus = "GOOD";
String streetlightStatus = "OFF";
String safetyStatus = "NORMAL";

// =======================
// ULTRASONIC FUNCTION
// =======================

long readDistanceCM() {
  digitalWrite(ULTRASONIC_TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(ULTRASONIC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG, LOW);

  long duration = pulseIn(ULTRASONIC_ECHO, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  long distance = duration * 0.034 / 2;
  return distance;
}

// =======================
// TRAFFIC CONTROL FUNCTION
// =======================

void updateTrafficSignal(String status) {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  if (status == "HIGH") {
    digitalWrite(GREEN_LED, HIGH);
  } 
  else if (status == "MEDIUM") {
    digitalWrite(YELLOW_LED, HIGH);
  } 
  else {
    digitalWrite(RED_LED, HIGH);
  }
}

// =======================
// BUZZER CONTROL
// =======================

void buzzerAlert(bool alert) {
  if (alert) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// =======================
// OLED DISPLAY FUNCTION
// =======================

void updateOLED() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("CivicSense AI Node");

  display.setCursor(0, 12);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 22);
  display.print("Hum : ");
  display.print(humidity);
  display.println(" %");

  display.setCursor(0, 32);
  display.print("Traffic: ");
  display.println(trafficStatus);

  display.setCursor(0, 42);
  display.print("Air: ");
  display.println(airStatus);

  display.setCursor(0, 52);
  display.print("Safety: ");
  display.println(safetyStatus);

  display.display();
}

// =======================
// SETUP
// =======================

void setup() {
  Serial.begin(115200);

  // Start I2C with custom ESP32 pins
  Wire.begin(OLED_SDA, OLED_SCL);

  dht.begin();

  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  pinMode(LDR_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);

  pinMode(PIR_PIN, INPUT);
  pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(STREETLIGHT_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(STREETLIGHT_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found. Check wiring/address.");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("CivicSense AI");
    display.println("System Starting...");
    display.display();
    delay(2000);
  }

  Serial.println("=================================");
  Serial.println("CivicSense AI Node Started");
  Serial.println("Phase 1: Local Hardware Testing");
  Serial.println("=================================");
}

// =======================
// LOOP
// =======================

void loop() {
  // Read DHT22
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT22 reading failed!");
    temperature = 0;
    humidity = 0;
  }

  // Read ultrasonic distance
  distanceCM = readDistanceCM();

  if (distanceCM == -1) {
    trafficStatus = "UNKNOWN";
  } 
  else if (distanceCM <= trafficDistanceHigh) {
    trafficStatus = "HIGH";
  } 
  else if (distanceCM <= trafficDistanceMedium) {
    trafficStatus = "MEDIUM";
  } 
  else {
    trafficStatus = "LOW";
  }

  // Update traffic signal LEDs
  updateTrafficSignal(trafficStatus);

  // Read LDR
  lightValue = analogRead(LDR_PIN);

  if (lightValue < lightThreshold) {
    digitalWrite(STREETLIGHT_LED, HIGH);
    streetlightStatus = "ON";
  } else {
    digitalWrite(STREETLIGHT_LED, LOW);
    streetlightStatus = "OFF";
  }

  // Read MQ135
  airQualityValue = analogRead(MQ135_PIN);

  if (airQualityValue > airQualityThreshold) {
    airStatus = "POOR";
  } else {
    airStatus = "GOOD";
  }

  // Read PIR
  pirState = digitalRead(PIR_PIN);

  // Read emergency button
  emergencyState = digitalRead(EMERGENCY_BUTTON);

  bool emergencyPressed = (emergencyState == LOW);
  bool motionDetected = (pirState == HIGH);
  bool badAir = (airStatus == "POOR");

  if (emergencyPressed) {
    safetyStatus = "EMERGENCY";
  } 
  else if (motionDetected) {
    safetyStatus = "MOTION";
  } 
  else {
    safetyStatus = "NORMAL";
  }

  // Buzzer logic
  if (emergencyPressed || badAir) {
    buzzerAlert(true);
  } else {
    buzzerAlert(false);
  }

  // OLED update
  updateOLED();

  // Serial Monitor Output
  Serial.println("----------- CivicSense Data -----------");

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Distance: ");
  Serial.print(distanceCM);
  Serial.println(" cm");

  Serial.print("Traffic Status: ");
  Serial.println(trafficStatus);

  Serial.print("LDR Value: ");
  Serial.println(lightValue);

  Serial.print("Streetlight: ");
  Serial.println(streetlightStatus);

  Serial.print("MQ135 Air Value: ");
  Serial.println(airQualityValue);

  Serial.print("Air Status: ");
  Serial.println(airStatus);

  Serial.print("PIR Motion: ");
  Serial.println(motionDetected ? "YES" : "NO");

  Serial.print("Emergency Button: ");
  Serial.println(emergencyPressed ? "PRESSED" : "NORMAL");

  Serial.print("Safety Status: ");
  Serial.println(safetyStatus);

  Serial.println("---------------------------------------");
  Serial.println();

  delay(2000);
}
