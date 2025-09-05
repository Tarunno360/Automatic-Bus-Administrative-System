#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// --- GLOBAL OBJECTS & CONSTANTS ---
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set LCD address to 0x27 for a 16 chars and 2 line display
const unsigned long REQUEST_INTERVAL_MS = 5000; // Request data every 5 seconds
const unsigned long DISPLAY_ROTATE_MS = 4000; // Rotate display screen every 4 seconds
const int MAX_USERS = 5; // Must match MAX_REGISTERED_CARDS on slave
const byte MAGIC_BYTE = 0xAA; // Magic value to indicate valid EEPROM data
const int EEPROM_START_ADDR = 0;

// --- DATA STORAGE VARIABLES ---
struct UserData {
  char name[17];
  int scanCount;
};
UserData users[MAX_USERS];
int passengerCount = 0;
int emergencyCount = 0;
int overloadCount = 0; // NEW: Counter for bus overload occurrences
int numUsers = 0;
bool dataReceived = false;

// --- TIMING & DISPLAY STATE VARIABLES ---
unsigned long lastRequestTime = 0;
unsigned long lastDisplayChangeTime = 0;
int currentDisplayScreen = 0; // 0 = main screen, 1+ = user screens

// --- FUNCTION PROTOTYPES ---
void parseData(String data);
void updateDisplay();

// --- SETUP ---
void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // Attempt to load data from EEPROM
  if (EEPROM.read(EEPROM_START_ADDR) == MAGIC_BYTE) {
    numUsers = EEPROM.read(EEPROM_START_ADDR + 1);
    EEPROM.get(EEPROM_START_ADDR + 2, passengerCount);
    EEPROM.get(EEPROM_START_ADDR + 4, emergencyCount);
    EEPROM.get(EEPROM_START_ADDR + 6, overloadCount); // NEW: Load overload count
    int addr = EEPROM_START_ADDR + 8; // Adjusted for new variable
    for (int i = 0; i < numUsers; i++) {
      for (int j = 0; j < 17; j++) {
        users[i].name[j] = EEPROM.read(addr++);
      }
      EEPROM.get(addr, users[i].scanCount);
      addr += 2;
    }
    dataReceived = true;
    currentDisplayScreen = 0;
    lastDisplayChangeTime = millis() - DISPLAY_ROTATE_MS; // Force immediate update
    lcd.setCursor(0, 0);
    lcd.print("Data loaded from");
    lcd.setCursor(0, 1);
    lcd.print("EEPROM");
    delay(2000); // Brief message
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Admin Station");
    lcd.setCursor(0, 1);
    lcd.print("Waiting for data");
  }
}

// --- LOOP ---
void loop() {
  // 1. Periodically request data from the slave
  if (millis() - lastRequestTime > REQUEST_INTERVAL_MS) {
    Serial.println("Hello from Master");
    lastRequestTime = millis();
  }

  // 2. Check for an incoming data packet from the slave
  if (Serial.available()) {
    String dataPacket = Serial.readStringUntil('\n');
    dataPacket.trim(); // Remove any whitespace
    if (dataPacket.startsWith("D:")) {
      parseData(dataPacket);
      dataReceived = true;
      // Reset display to the first screen upon receiving new data
      currentDisplayScreen = 0;
      lastDisplayChangeTime = millis() - DISPLAY_ROTATE_MS; // Force immediate update
    }
  }
  
  // 3. Update the LCD display continuously
  if (dataReceived) {
    updateDisplay();
  }
}

// --- FUNCTIONS ---
void parseData(String data) {
  // Remove the "D:" prefix
  data.remove(0, 2);

  // Use a character buffer for strtok()
  char dataBuffer[data.length() + 1];
  data.toCharArray(dataBuffer, sizeof(dataBuffer));

  char* token = strtok(dataBuffer, ",");
  int tokenIndex = 0;
  numUsers = 0; // Reset user count for new packet

  while (token != NULL) {
    if (tokenIndex == 0) {
      passengerCount = atoi(token); // First token is passenger count
    } else if (tokenIndex == 1) {
      emergencyCount = atoi(token); // Second token is emergency count
    } else if (tokenIndex == 2) {
      overloadCount = atoi(token); // NEW: Third token is overload count
    } else {
      // Subsequent tokens are user name and scan count pairs
      if ((tokenIndex % 2 == 1) && (numUsers < MAX_USERS)) {
        // This is a name
        strncpy(users[numUsers].name, token, 16);
        users[numUsers].name[16] = '\0'; // Ensure null termination
      } else if (numUsers < MAX_USERS) {
        // This is a scan count
        users[numUsers].scanCount = atoi(token);
        numUsers++; // Increment user count after processing a pair
      }
    }
    token = strtok(NULL, ",");
    tokenIndex++;
  }

  // Save the parsed data to EEPROM (overwrite previous data)
  EEPROM.write(EEPROM_START_ADDR, MAGIC_BYTE);
  EEPROM.write(EEPROM_START_ADDR + 1, (byte)numUsers);
  EEPROM.put(EEPROM_START_ADDR + 2, passengerCount);
  EEPROM.put(EEPROM_START_ADDR + 4, emergencyCount);
  EEPROM.put(EEPROM_START_ADDR + 6, overloadCount); // NEW: Save overload count
  int addr = EEPROM_START_ADDR + 8; // Adjusted for new variable
  for (int i = 0; i < numUsers; i++) {
    for (int j = 0; j < 17; j++) {
      EEPROM.write(addr++, users[i].name[j]);
    }
    EEPROM.put(addr, users[i].scanCount);
    addr += 2;
  }
}

void updateDisplay() {
  // Check if it's time to switch to the next screen
  if (millis() - lastDisplayChangeTime > DISPLAY_ROTATE_MS) {
    currentDisplayScreen++;
    // The total number of screens is 1 (main) + the number of users
    if (currentDisplayScreen > numUsers) {
      currentDisplayScreen = 0; // Loop back to the main screen
    }
    
    lastDisplayChangeTime = millis();
    lcd.clear();

    if (currentDisplayScreen == 0) {
      // --- Main Info Screen ---
      lcd.setCursor(0, 0);
      lcd.print("P:");
      lcd.print(passengerCount);
      lcd.print(" E:");
      lcd.print(emergencyCount);
      lcd.setCursor(0, 1);
      lcd.print("Overload: ");
      lcd.print(overloadCount); // NEW: Display overload count
    } else {
      // --- User Info Screen ---
      int userIndex = currentDisplayScreen - 1;
      if (userIndex < numUsers) {
        lcd.setCursor(0, 0);
        lcd.print(users[userIndex].name);
        lcd.setCursor(0, 1);
        lcd.print("Scans: ");
        lcd.print(users[userIndex].scanCount);
      }
    }
  }
}


