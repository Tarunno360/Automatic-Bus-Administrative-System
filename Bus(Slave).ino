// Bus Passenger & RFID Access Control System
// This Arduino sketch manages passenger counting using IR sensors,
// controls access via an RFID reader, and includes an emergency alert system.

// --- LIBRARIES ---
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>

// --- PIN DEFINITIONS ---
#define RST_PIN 9
#define SS_PIN 10
#define SERVO_PIN 6
#define BUZZER_PIN 7
#define EMERGENCY_BUTTON_PIN 8
#define IR_SENSOR_1_PIN A0
#define IR_SENSOR_2_PIN A1

// --- CONSTANTS & SETTINGS ---
#define TIMEOUT_MS 10000
#define GATE_BLOCK_CHECK_DELAY_MS 1500 // Delay before checking if gate is blocked
#define MAGIC_VALUE 0xAFCB             // A unique value to check if EEPROM has been initialized
#define MAX_NAME_LENGTH 16
#define MAX_REGISTERED_CARDS 5
#define UID_MAX_SIZE 10
#define SEQ_TIMEOUT_MS 1000            // Timeout for IR sensor sequence
#define IR_DEBOUNCE_MS 50              // Debounce delay for IR sensors
#define BUZZER_DURATION_MS 3000        // How long the overload buzzer stays on
#define EMERGENCY_TIMEOUT_MS 5000      // How long the emergency mode stays active

// --- EEPROM ADDRESSES ---
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_NUM_CARDS (EEPROM_ADDR_MAGIC + sizeof(uint16_t))
#define EEPROM_ADDR_CARDS_DATA (EEPROM_ADDR_NUM_CARDS + sizeof(byte))

// --- OBJECT INITIALIZATIONS ---
Servo myServo;
MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- GLOBAL VARIABLES ---
bool hasRotated = false;
unsigned long lastMessageTime = 0;
int passengerCount = 0;
bool ir1Triggered = false;
bool ir2Triggered = false;
unsigned long lastTriggerTime = 0;
unsigned int emergencyPressCount = 0;
char lastScannedName[MAX_NAME_LENGTH + 1] = "None";
unsigned int lastScannedCount = 0;
unsigned int overloadCount = 0;
byte numLoadedCards = 0;
unsigned long buzzerOnTime = 0;
int lastButtonState = HIGH;
unsigned long lastEmergencyTime = 0;
bool emergencyMode = false;
bool servoOpen = false;
bool isOverloaded = false; // Flag to track if the bus is currently overloaded

// --- DATA STRUCTURES ---
struct RegisteredCard {
  byte uid[UID_MAX_SIZE];
  byte uidSize;
  char name[MAX_NAME_LENGTH + 1];
  unsigned int scanCount;
};
RegisteredCard registeredCards[MAX_REGISTERED_CARDS];

// --- PREDEFINED USERS ---
RegisteredCard predefinedUsers[] = {
  {{0xF3, 0x97, 0x17, 0x2D}, 4, "BUS DRIVER", 0},
  {{0x19, 0x78, 0x97, 0x3F}, 4, "HELPER", 0},
  {{0x11, 0x22, 0x33, 0x44}, 4, "STATION MASTER", 0},
};
const byte NUM_PREDEFINED_USERS = sizeof(predefinedUsers) / sizeof(RegisteredCard);

// --- FUNCTION PROTOTYPES ---
void writePredefinedCardsToEEPROM();
void loadCardsFromEEPROM();
int findCardIndex(byte* uid, byte uidSize);
void activateServo();
void updateLCD();
void resetIRStates();
void handleEmergencyButton();
void checkAndCloseServo();
void sendAdminData();

// --- SETUP FUNCTION ---
void setup() {
  Serial.begin(9600);
  while (!Serial); // Wait for serial connection to be established

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("System Ready: Bus System Operational."));

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Passenger Count:");
  updateLCD();

  pinMode(IR_SENSOR_1_PIN, INPUT_PULLUP);
  pinMode(IR_SENSOR_2_PIN, INPUT_PULLUP);

  myServo.attach(SERVO_PIN);
  myServo.write(60); // Initial servo position (closed)

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(EMERGENCY_BUTTON_PIN, INPUT_PULLUP);

  // Check if EEPROM has been initialized, if not, write predefined data
  uint16_t storedMagicValue;
  EEPROM.get(EEPROM_ADDR_MAGIC, storedMagicValue);
  if (storedMagicValue != MAGIC_VALUE) {
    Serial.println(F("EEPROM not initialized. Writing predefined cards..."));
    writePredefinedCardsToEEPROM();
    EEPROM.put(EEPROM_ADDR_MAGIC, MAGIC_VALUE);
  } else {
    Serial.println(F("EEPROM already initialized. Loading cards..."));
  }
  loadCardsFromEEPROM();
}

// --- MAIN LOOP ---
void loop() {
  handleEmergencyButton();

  if (!emergencyMode) {
    // Handle Bluetooth commands
    if (Serial.available()) {
      String msg = Serial.readStringUntil('\n');
      Serial.print("Received via Bluetooth: ");
      Serial.println(msg);
      lastMessageTime = millis();
      if (msg.indexOf("Hello from Master") != -1 && !hasRotated) {
        delay(6000); // MODIFIED: Wait for 3 seconds before opening the gate
        myServo.write(120); // Open gate
        hasRotated = true;
        sendAdminData();
      }
    }

    // Timeout for Bluetooth control
    if (hasRotated) {
      if (millis() - lastMessageTime > TIMEOUT_MS) {
        if (millis() - lastMessageTime > (TIMEOUT_MS + GATE_BLOCK_CHECK_DELAY_MS)) {
          if (digitalRead(IR_SENSOR_1_PIN) == HIGH && digitalRead(IR_SENSOR_2_PIN) == HIGH) {
            Serial.println("Bluetooth timeout -> Reset servo to 0");
            myServo.write(60); // Close gate
            hasRotated = false;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Passenger Count:");
            updateLCD();
          } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Gate blocked!");
            lcd.setCursor(0, 1);
            lcd.print("Please move away");
          }
        }
      }
    }

    // Handle RFID card scanning when gate is closed
    if (!hasRotated) {
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        Serial.print("Scanned Card UID:");
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
          Serial.print(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println();

        int cardIndex = findCardIndex(mfrc522.uid.uidByte, mfrc522.uid.size);
        if (cardIndex != -1) {
          Serial.print(F("Access Granted! Welcome, "));
          Serial.println(registeredCards[cardIndex].name);
          registeredCards[cardIndex].scanCount++;
          strcpy(lastScannedName, registeredCards[cardIndex].name);
          lastScannedCount = registeredCards[cardIndex].scanCount;
          activateServo();
        } else {
          Serial.println(F("Access Denied: Unknown Card."));
        }
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    }

    // --- IR Sensor Logic for Passenger Counting ---
    int reading1 = digitalRead(IR_SENSOR_1_PIN);
    int reading2 = digitalRead(IR_SENSOR_2_PIN);

    // Entry sequence: Sensor 1 triggered, then Sensor 2
    if (reading1 == LOW && !ir1Triggered && !ir2Triggered) {
      ir1Triggered = true;
      lastTriggerTime = millis();
    }
    if (ir1Triggered && reading2 == LOW && (millis() - lastTriggerTime < SEQ_TIMEOUT_MS)) {
      passengerCount++;
      updateLCD();
      Serial.print("Entry -> Count: ");
      Serial.println(passengerCount);
      resetIRStates();
      delay(IR_DEBOUNCE_MS);
    }

    // Exit sequence: Sensor 2 triggered, then Sensor 1
    if (reading2 == LOW && !ir2Triggered && !ir1Triggered) {
      ir2Triggered = true;
      lastTriggerTime = millis();
    }
    if (ir2Triggered && reading1 == LOW && (millis() - lastTriggerTime < SEQ_TIMEOUT_MS)) {
      if (passengerCount > 0) passengerCount--;
      updateLCD();
      Serial.print("Exit  -> Count: ");
      Serial.println(passengerCount);
      resetIRStates();
      delay(IR_DEBOUNCE_MS);
    }

    // Reset IR states if sequence times out
    if ((ir1Triggered || ir2Triggered) && (millis() - lastTriggerTime > SEQ_TIMEOUT_MS)) {
      resetIRStates();
    }

    // Turn off overload buzzer after duration
    if (digitalRead(BUZZER_PIN) == HIGH && (millis() - buzzerOnTime > BUZZER_DURATION_MS)) {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Overload buzzer timed out, turning OFF.");
    }

    checkAndCloseServo();
  }
}

// --- HELPER FUNCTIONS ---

/**
 * @brief Sends administrative data over Serial.
 * Format: D:passengerCount,emergencyCount,overloadCount,name1,scanCount1,name2,scanCount2,...
 */
void sendAdminData() {
  char buffer[150];
  sprintf(buffer, "D:%d,%u,%u", passengerCount, emergencyPressCount, overloadCount);
  for (byte i = 0; i < numLoadedCards; i++) {
    char userData[50];
    sprintf(userData, ",%s,%u", registeredCards[i].name, registeredCards[i].scanCount);
    strcat(buffer, userData);
  }
  Serial.println(buffer);
  Serial.print("Sent to Admin: ");
  Serial.println(buffer);
}

/**
 * @brief Writes the predefined user cards to EEPROM memory.
 */
void writePredefinedCardsToEEPROM() {
  if (NUM_PREDEFINED_USERS > MAX_REGISTERED_CARDS) {
    Serial.println(F("WARNING: Too many predefined users to fit in EEPROM!"));
  }

  struct CardForEEPROM {
    byte uid[UID_MAX_SIZE];
    byte uidSize;
    char name[MAX_NAME_LENGTH + 1];
  };

  EEPROM.put(EEPROM_ADDR_NUM_CARDS, NUM_PREDEFINED_USERS);
  for (byte i = 0; i < NUM_PREDEFINED_USERS; i++) {
    int startAddress = EEPROM_ADDR_CARDS_DATA + (i * sizeof(CardForEEPROM));
    CardForEEPROM card_to_store;
    memcpy(card_to_store.uid, predefinedUsers[i].uid, sizeof(card_to_store.uid));
    card_to_store.uidSize = predefinedUsers[i].uidSize;
    strcpy(card_to_store.name, predefinedUsers[i].name);
    EEPROM.put(startAddress, card_to_store);
  }
  Serial.println(F("Predefined cards written to EEPROM."));
}

/**
 * @brief Loads the registered cards from EEPROM into memory.
 */
void loadCardsFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_NUM_CARDS, numLoadedCards);
  if (numLoadedCards > MAX_REGISTERED_CARDS || numLoadedCards < 0) {
    Serial.println(F("Error: Invalid card count in EEPROM, resetting to 0."));
    numLoadedCards = 0;
    return;
  }

  struct CardForEEPROM {
    byte uid[UID_MAX_SIZE];
    byte uidSize;
    char name[MAX_NAME_LENGTH + 1];
  };

  for (byte i = 0; i < numLoadedCards; i++) {
    int startAddress = EEPROM_ADDR_CARDS_DATA + (i * sizeof(CardForEEPROM));
    CardForEEPROM card_from_eeprom;
    EEPROM.get(startAddress, card_from_eeprom);
    memcpy(registeredCards[i].uid, card_from_eeprom.uid, sizeof(card_from_eeprom.uid));
    registeredCards[i].uidSize = card_from_eeprom.uidSize;
    strcpy(registeredCards[i].name, card_from_eeprom.name);
    registeredCards[i].scanCount = 0; // Reset scan count on startup

    Serial.print(F("Loaded Card "));
    Serial.print(i + 1);
    Serial.print(F(": UID="));
    for (byte j = 0; j < registeredCards[i].uidSize; j++) {
      Serial.print(registeredCards[i].uid[j] < 0x10 ? " 0" : " ");
      Serial.print(registeredCards[i].uid[j], HEX);
    }
    Serial.print(F(", Name="));
    Serial.println(registeredCards[i].name);
  }
  Serial.print(F("Total cards loaded: "));
  Serial.println(numLoadedCards);
}

/**
 * @brief Finds the index of a card in the registeredCards array.
 * @param uid Pointer to the UID byte array.
 * @param uidSize The size of the UID.
 * @return The index of the card if found, otherwise -1.
 */
int findCardIndex(byte* uid, byte uidSize) {
  for (byte i = 0; i < numLoadedCards; i++) {
    if (registeredCards[i].uidSize != uidSize) continue;
    bool match = true;
    for (byte j = 0; j < uidSize; j++) {
      if (registeredCards[i].uid[j] != uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return i;
  }
  return -1;
}

/**
 * @brief Activates the servo to open the gate.
 */
void activateServo() {
  Serial.println(F("Activating servo..."));
  myServo.write(120);
  servoOpen = true;
  lastMessageTime = millis();
}

/**
 * @brief Checks if the servo can be closed and closes it after a timeout.
 */
void checkAndCloseServo() {
  if (servoOpen) {
    if (millis() - lastMessageTime > (TIMEOUT_MS + GATE_BLOCK_CHECK_DELAY_MS)) {
      if (digitalRead(IR_SENSOR_1_PIN) == HIGH && digitalRead(IR_SENSOR_2_PIN) == HIGH) {
        myServo.write(60);
        servoOpen = false;
        Serial.println("Servo closed successfully.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Passenger Count:");
        updateLCD();
      } else {
        Serial.println("Cannot close servo: Gate is blocked.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Gate blocked!");
        lcd.setCursor(0, 1);
        lcd.print("Please move away");
        lastMessageTime = millis(); // Reset timeout while blocked
      }
    }
  }
}

/**
 * @brief Updates the LCD display with the current passenger count and overload status.
 * Triggers the overload alarm only once when the count exceeds 5.
 */
void updateLCD() {
  lcd.setCursor(0, 1); // Set cursor to the beginning of the second line

  if (passengerCount > 5) {
    // This block handles the overload condition.

    // Check if we just crossed the overload threshold.
    if (!isOverloaded) {
      isOverloaded = true; // Set the overload state flag.
      // This ensures the following actions happen only once per overload event.
      digitalWrite(BUZZER_PIN, HIGH); // Activate the buzzer.
      buzzerOnTime = millis();        // Start the timer for the buzzer duration.
      overloadCount++;                // Increment the persistent overload counter.
      Serial.println("Overload detected, buzzer ON.");
    }

    // Format and display the overload message on the LCD.
    char buffer[17]; // 16 characters for the LCD line + 1 for null terminator.
    sprintf(buffer, "%d (Overload)", passengerCount);
    lcd.print(buffer);
    // Pad with spaces to clear any previous, longer text on the line.
    for (int i = strlen(buffer); i < 16; i++) {
      lcd.print(" ");
    }

  } else {
    // This block handles the normal, non-overloaded condition.

    // If the system was in an overload state, reset the flag now that the count is normal.
    if (isOverloaded) {
      isOverloaded = false; // Reset the overload state flag.
      Serial.println("Passenger count normal. Overload state reset.");
    }
    
    // Format and display the normal passenger count.
    char buffer[17];
    sprintf(buffer, "Count: %d", passengerCount);
    lcd.print(buffer);
    // Pad with spaces to clear the rest of the line.
    for (int i = strlen(buffer); i < 16; i++) {
      lcd.print(" ");
    }
    
    // Ensure the buzzer is off in the normal state. The main loop also handles
    // turning it off after BUZZER_DURATION_MS, but this handles the case
    // where the count drops back to normal before the timeout.
    digitalWrite(BUZZER_PIN, LOW);
  }
}

/**
 * @brief Resets the state of the IR sensor triggers.
 */
void resetIRStates() {
  ir1Triggered = false;
  ir2Triggered = false;
}

/**
 * @brief Handles the emergency button press and activates/deactivates emergency mode.
 */
void handleEmergencyButton() {
  int buttonState = digitalRead(EMERGENCY_BUTTON_PIN);
  if (buttonState == LOW && lastButtonState == HIGH) {
    emergencyMode = true;
    lastEmergencyTime = millis();
    emergencyPressCount++;
    tone(BUZZER_PIN, 1000, 200); // Short beep to confirm
    Serial.println("Emergency button pressed! Buzzer and LCD activated.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EMERGENCY!!!");
    lcd.setCursor(0, 1);
    lcd.print("Alert! Button!");
  }

  if (emergencyMode && (millis() - lastEmergencyTime > EMERGENCY_TIMEOUT_MS)) {
    emergencyMode = false;
    noTone(BUZZER_PIN);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Passenger Count:");
    updateLCD();
    Serial.println("Emergency alert timed out.");
  }
  lastButtonState = buttonState;
}



