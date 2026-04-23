#include <DHT.h>

//  PIN DEFINITIONS 
// Environment Sensors
#define DHTPIN 14          // DHT22 data pin
#define DHTTYPE DHT22      // DHT22 sensor type
#define LDR_PIN 34         // LDR analog pin for light level
#define PIR_PIN 5          // PIR Motion sensor for streetlight activation

// Traffic Control System
#define RED_LED 21         // Red traffic light
#define YELLOW_LED 22      // Yellow traffic light
#define GREEN_LED 23       // Green traffic light
#define VEHICLE_SENSOR 4   // Vehicle detection sensor (e.g., IR or inductive loop)

// Streetlight
#define STREETLIGHT_PIN 25 // PWM pin for controlling streetlight brightness

// Waste Bin Management
#define TRIG_PIN 26        // Ultrasonic sensor Trig pin
#define ECHO_PIN 13        // Ultrasonic sensor Echo pin

// Emergency System
#define EMERGENCY_BUTTON 18 // Button to trigger emergency mode
#define BUZZER 19           // Buzzer for alerts

//  Global Objects 
DHT dht(DHTPIN, DHTTYPE);

//  GLOBAL VARIABLES & STATE MANAGEMENT 

//  System Timers 
unsigned long lastTrafficChange = 0;
unsigned long lastMotionTime = 0;
unsigned long lastDHTRead = 0;
unsigned long lastBinCheck = 0;
unsigned long lastEnergyCalc = 0;
unsigned long lastTrafficLog = 0;
unsigned long lastFillCalc = 0;
unsigned long buzzerEndTime = 0;
unsigned long lastVehicleDetectedTime = 0; 
unsigned long lastMotionLogTime = 0; 

//  Traffic Light State 
enum TrafficState { RED, YELLOW, GREEN };
TrafficState currentTrafficState = RED;
int trafficCycleDuration = 10000; 
int hourlyVehicleCount = 0;
bool ecoModeSuggested = false; 

//  Streetlight State 
bool isStreetlightOn = false;
bool lastMotionState = false; 
bool isCurrentlyDark = false; 
const unsigned long STREETLIGHT_TIMEOUT_STD = 30000; 
const unsigned long STREETLIGHT_TIMEOUT_DST = 15000; 
unsigned long currentStreetlightTimeout = STREETLIGHT_TIMEOUT_STD;

//  Bin State 
const float BIN_HEIGHT_CM = 50.0; 
float lastFillPercentage = -1.0; 
float fillPattern[5] = {0};
int patternIndex = 0;
bool binHistoryInitialized = false; 
float lastFillForRateCalc = 0;
int lastBinAlertLevel = 0; 

//  Environment State 
float lastTemp = -100; 
float lastHumidity = -1; 

//  Emergency State 
bool emergencyModeActive = false;

//  Analytics 
float energyUsedKWh = 0;
float fillRatePerHour = 0;


//         SETUP 
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n-\|/--\|/--\|/--\|/--\|/--\|/-");
    Serial.println("🚦 Smart City System Initializing... 🚦");
    Serial.println("-\|/--\|/--\|/--\|/--\|/--\|/-");

    pinMode(RED_LED, OUTPUT);
    pinMode(YELLOW_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(STREETLIGHT_PIN, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    pinMode(VEHICLE_SENSOR, INPUT_PULLUP);
    pinMode(PIR_PIN, INPUT);
    pinMode(EMERGENCY_BUTTON, INPUT_PULLUP);
    pinMode(ECHO_PIN, INPUT);

    dht.begin();

    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(STREETLIGHT_PIN, LOW);
    digitalWrite(BUZZER, LOW);

    unsigned long initialMillis = millis();
    lastTrafficChange = initialMillis;
    lastMotionTime = initialMillis;
    lastEnergyCalc = initialMillis;
    lastTrafficLog = initialMillis;
    lastFillCalc = initialMillis;
    lastVehicleDetectedTime = initialMillis;

    Serial.println("System Ready!");
}

//           MAIN LOOP 
void loop() {
    unsigned long currentMillis = millis();

    handleEmergency(currentMillis);
    controlTrafficSystem(currentMillis);
    controlStreetlights(currentMillis);
    monitorWasteBin(currentMillis);
    monitorEnvironment(currentMillis);
    logTrafficPatterns(currentMillis);
    predictBinCollection(currentMillis);
    calculateEnergyConsumption(currentMillis);
    checkDaylightSaving(currentMillis);
    handleBuzzer(currentMillis);
    
    delay(20); 
}

//  CORE FUNCTIONS 

void playBuzzer(int frequency, int duration) {
    tone(BUZZER, frequency, duration);
    buzzerEndTime = millis() + duration;
}

void handleBuzzer(unsigned long currentMillis) {
    if (buzzerEndTime > 0 && currentMillis >= buzzerEndTime) {
        noTone(BUZZER);
        buzzerEndTime = 0;
    }
}

void handleEmergency(unsigned long currentMillis) {
    static unsigned long lastButtonPress = 0;
    bool buttonPressed = (digitalRead(EMERGENCY_BUTTON) == LOW);

    if (buttonPressed && (currentMillis - lastButtonPress > 500)) {
        lastButtonPress = currentMillis;
        emergencyModeActive = !emergencyModeActive;

        if (emergencyModeActive) {
            Serial.println("🚨 EMERGENCY MODE ACTIVATED! 🚨");
            playBuzzer(2000, 1500);
        } else {
            Serial.println("✅ Emergency Mode Cleared.");
        }
    }
}

void controlTrafficSystem(unsigned long currentMillis) {
    if (emergencyModeActive) {
        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(YELLOW_LED, (currentMillis % 1000 < 500));
        return; 
    }

    static bool lastVehicleState = false;
    bool vehicleDetected = (digitalRead(VEHICLE_SENSOR) == LOW);
    if (vehicleDetected && !lastVehicleState) {
        hourlyVehicleCount++;
        lastVehicleDetectedTime = currentMillis; 
        ecoModeSuggested = false; 
        Serial.print(" Vehicle detected. Count this hour: ");
        Serial.println(hourlyVehicleCount);
    }
    lastVehicleState = vehicleDetected;

    if (!vehicleDetected && (currentMillis - lastVehicleDetectedTime > 60000) && !ecoModeSuggested) {
        Serial.println(" AI: No vehicles detected for 1 minute. Suggesting intersection eco-mode.");
        ecoModeSuggested = true; 
    }

    int oldCycleDuration = trafficCycleDuration;
    trafficCycleDuration = 15000 - (min(hourlyVehicleCount, 100) * 100);
    trafficCycleDuration = max(5000, trafficCycleDuration);

    if (trafficCycleDuration != oldCycleDuration) {
        Serial.print(" AI: Traffic density changed. Green light cycle adjusted to ");
        Serial.print(trafficCycleDuration / 1000);
        Serial.println(" seconds.");
    }

    switch (currentTrafficState) {
        case RED:
            digitalWrite(RED_LED, HIGH);
            digitalWrite(YELLOW_LED, LOW);
            digitalWrite(GREEN_LED, LOW);
            if (vehicleDetected && (currentMillis - lastTrafficChange >= trafficCycleDuration)) {
                currentTrafficState = GREEN;
                lastTrafficChange = currentMillis;
                Serial.println(" Light changed to GREEN (Reason: Vehicle detected at red light)");
            }
            break;

        case GREEN:
            digitalWrite(RED_LED, LOW);
            digitalWrite(YELLOW_LED, LOW);
            digitalWrite(GREEN_LED, HIGH);
            if (currentMillis - lastTrafficChange >= trafficCycleDuration) {
                currentTrafficState = YELLOW;
                lastTrafficChange = currentMillis;
                Serial.print(" Light changed to YELLOW (Reason: Green cycle of ");
                Serial.print(trafficCycleDuration / 1000);
                Serial.println("s elapsed)");
            }
            break;

        case YELLOW:
            digitalWrite(RED_LED, LOW);
            digitalWrite(YELLOW_LED, HIGH);
            digitalWrite(GREEN_LED, LOW);
            if (currentMillis - lastTrafficChange >= 3000) {
                currentTrafficState = RED;
                lastTrafficChange = currentMillis;
                Serial.println(" Light changed to RED (Reason: Yellow phase finished)");
            }
            break;
    }
}


void controlStreetlights(unsigned long currentMillis) {
    int ldrValue = analogRead(LDR_PIN);
    bool isDark = (ldrValue < 1000); 

    if (isDark && !isCurrentlyDark) {
        Serial.println(" It's getting dark. Streetlight system is now active.");
        isCurrentlyDark = true;
    } else if (!isDark && isCurrentlyDark) {
        Serial.println(" It's daytime. Streetlight system is on standby.");
        isCurrentlyDark = false;
    }

    bool motionDetected = (digitalRead(PIR_PIN) == HIGH);

    //  ENHANCED: More interactive motion logging 
    if (motionDetected && !lastMotionState) {
        // Only log if it's been at least 5 seconds since the last log
        if (currentMillis - lastMotionLogTime > 5000) {
            Serial.println(" Motion detected! Resetting streetlight timer.");
            lastMotionLogTime = currentMillis; // Update the log timer
        }
    }
    lastMotionState = motionDetected;

    if (motionDetected) {
        lastMotionTime = currentMillis;
    }

    bool shouldBeOn = isDark && (motionDetected || (currentMillis - lastMotionTime < currentStreetlightTimeout));
    
    if (shouldBeOn && !isStreetlightOn) {
        isStreetlightOn = true;
        int brightness = map(ldrValue, 0, 1000, 255, 80);
        analogWrite(STREETLIGHT_PIN, brightness);
        Serial.print(" Streetlight ON. Brightness: ");
        Serial.println(brightness);
    } else if (!shouldBeOn && isStreetlightOn) {
        isStreetlightOn = false;
        analogWrite(STREETLIGHT_PIN, 0);
        // ENHANCED: Log the reason for turning off
        if (!isDark) {
            Serial.println(" Streetlight OFF. (Reason: It is now daytime)");
        } else {
            Serial.println(" Streetlight OFF. (Reason: No motion for timeout period)");
        }
    }
    
    if (emergencyModeActive) {
        analogWrite(STREETLIGHT_PIN, (currentMillis % 500 < 250) ? 255 : 0);
    }
}


void monitorWasteBin(unsigned long currentMillis) {
    if (currentMillis - lastBinCheck < 5000) return;
    lastBinCheck = currentMillis;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {
        return; 
    }

    float distance = duration * 0.034 / 2.0;
    float fillPercentage = 100.0 * (1.0 - (distance / BIN_HEIGHT_CM));
    fillPercentage = constrain(fillPercentage, 0, 100);

    if (!binHistoryInitialized) {
        for(int i=0; i<5; i++) fillPattern[i] = fillPercentage;
        binHistoryInitialized = true;
        Serial.println("Bin level history initialized.");
    }
    
    int currentAlertLevel = 0;
    if (fillPercentage > 90) currentAlertLevel = 90;
    else if (fillPercentage > 75) currentAlertLevel = 75;
    else if (fillPercentage > 50) currentAlertLevel = 50;

    if (currentAlertLevel > lastBinAlertLevel) {
        Serial.print(" Bin Fill Level: ");
        Serial.print(fillPercentage, 1);
        Serial.println("%");
        switch(currentAlertLevel) {
            case 50:
                Serial.println("ℹ️ INFO: Bin is now over 50% full.");
                break;
            case 75:
                Serial.println("⚠️ WARNING: Bin is now over 75% full. Schedule collection soon.");
                playBuzzer(800, 500);
                break;
            case 90:
                Serial.println("🆘 URGENT: Bin is over 90% full! Immediate collection required!");
                playBuzzer(1200, 1000);
                break;
        }
        lastBinAlertLevel = currentAlertLevel;
    } else if (currentAlertLevel < lastBinAlertLevel) {
        lastBinAlertLevel = currentAlertLevel;
        Serial.print(" Bin Fill Level: ");
        Serial.print(fillPercentage, 1);
        Serial.println("% (Bin has been partially or fully emptied)");
    }
    
    lastFillPercentage = fillPercentage;
}

void monitorEnvironment(unsigned long currentMillis) {
    if (currentMillis - lastDHTRead < 10000) return;
    lastDHTRead = currentMillis;

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temp) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    if (abs(temp - lastTemp) > 0.5 || abs(humidity - lastHumidity) > 1.0) {
        Serial.print(" Temp: ");
        Serial.print(temp, 1);
        Serial.print("°C |  Humidity: ");
        Serial.print(humidity, 1);
        Serial.println("%");
        
        if (temp > 35.0) {
            Serial.println(" ANALYSIS: Heat warning! High temperatures detected.");
            playBuzzer(1500, 1000);
        } else if (humidity > 85.0) {
            Serial.println(" ANALYSIS: High humidity! Possible rain or fog.");
        } else if (temp < 15.0) {
            Serial.println(" ANALYSIS: Cold weather detected.");
        } else if (temp >= 20.0 && temp <= 28.0 && humidity < 70.0) {
            Serial.println(" ANALYSIS: Pleasant weather conditions.");
        }
        
        lastTemp = temp;
        lastHumidity = humidity;
    }
}


//          ANALYTICS & PREDICTIVE FEATURES 

void checkDaylightSaving(unsigned long currentMillis) {
    static unsigned long lastCheck = 0;
    if (currentMillis - lastCheck < 3600000) return;
    lastCheck = currentMillis;

    static bool isDST = false;
    isDST = !isDST; 

    if (isDST) {
        currentStreetlightTimeout = STREETLIGHT_TIMEOUT_DST;
        Serial.println(" Daylight Saving Time active. Shorter streetlight timeout.");
    } else {
        currentStreetlightTimeout = STREETLIGHT_TIMEOUT_STD;
        Serial.println(" Standard Time active. Standard streetlight timeout.");
    }
}

void logTrafficPatterns(unsigned long currentMillis) {
    if (currentMillis - lastTrafficLog >= 3600000) {
        Serial.println("---  HOURLY TRAFFIC REPORT 📊 ---");
        Serial.print("Total vehicles this hour: ");
        Serial.println(hourlyVehicleCount);
        if (hourlyVehicleCount > 100) {
            Serial.println(" High traffic density detected this hour.");
        } else if (hourlyVehicleCount < 10) {
            Serial.println(" Low traffic density this hour.");
        }
        Serial.println("------------------------------------");

        hourlyVehicleCount = 0;
        lastTrafficLog = currentMillis;
    }
}

void predictBinCollection(unsigned long currentMillis) {
    if (currentMillis - lastFillCalc >= 3600000) {
        if (lastFillPercentage < 0) return;
        fillRatePerHour = (lastFillPercentage - lastFillForRateCalc);
        lastFillForRateCalc = lastFillPercentage;
        lastFillCalc = currentMillis;

        if (fillRatePerHour > 1) {
            float hoursToFull = (95.0 - lastFillPercentage) / fillRatePerHour;
            Serial.print(" PREDICTION: Bin will be full in approximately ");
            Serial.print(hoursToFull, 1);
            Serial.println(" hours.");
        } else {
             Serial.println(" PREDICTION: Bin fill rate is stable.");
        }
    }
}

void calculateEnergyConsumption(unsigned long currentMillis) {
    if (isStreetlightOn) {
        float elapsedHours = (currentMillis - lastEnergyCalc) / 3600000.0;
        energyUsedKWh += 0.05 * elapsedHours;
    }
    lastEnergyCalc = currentMillis;

    static unsigned long lastDailyReport = 0;
    if (currentMillis - lastDailyReport > 86400000) {
        Serial.print("---  DAILY ENERGY REPORT 💡 ---");
        Serial.print("Total streetlight energy used: ");
        Serial.print(energyUsedKWh, 3);
        Serial.println(" kWh");
        Serial.println("---------------------------------");
        energyUsedKWh = 0;
        lastDailyReport = currentMillis;
    }
}
