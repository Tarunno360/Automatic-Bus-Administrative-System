#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16x4 display.
LiquidCrystal_I2C lcd(0x27, 16, 4);

const int hallPin = 2; // pin 2 = interrupt 0
const float TIRE_RADIUS_M = 0.03; // Tire radius in meters (3 cm = 0.03 m).

// Volatile variables for the pulse count and timing.
volatile unsigned long cntTime = 0;
volatile unsigned long cnt = 0;

// Variables for RPM and speed calculation.
unsigned long rpm = 0;
float speed_kmh = 0.0;
unsigned long measureTime = 0;
int measureCnt = 0;

// Variable to store the top speed.
float top_speed_kmh = 0.0;

// --- NEW --- Variables for hourly reset of top speed.
const unsigned long HOURLY_RESET_INTERVAL = 3600000UL; // 1 hour in milliseconds (1000 * 60 * 60)
unsigned long lastResetTime = 0;

const int resetTime = 2000; // Reset RPM to 0 if no pulses for 2 seconds.
const int minRotNum = 1;

// Interrupt Service Routine (ISR) to handle pulses.
void doCount() {
  if (digitalRead(hallPin) == LOW) {
    cnt++;
    cntTime = millis();
  }
}

void setup() {
  Serial.begin(9600);

  // Initialize the LCD.
  lcd.begin(16, 4);
  lcd.backlight();

  // Set up the Hall sensor pin and interrupt.
  pinMode(hallPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(hallPin), doCount, FALLING);

  // Set up the static display layout.
  lcd.setCursor(0, 0);
  lcd.print("RPM: ");
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.setCursor(0, 2);
  lcd.print("Top:   ");
  lcd.setCursor(0, 3);
  lcd.print("Safety First!");

  measureTime = millis();
  lastResetTime = millis(); // Initialize the reset timer.
}

void loop() {
  unsigned long curTime = millis();

  // --- NEW --- Check if an hour has passed to reset the top speed.
  if (curTime - lastResetTime >= HOURLY_RESET_INTERVAL) {
    top_speed_kmh = 0.0;      // Reset the top speed
    lastResetTime = curTime;  // Record the time of this reset
  }

  // Reset if no pulses
  if (curTime - cntTime > resetTime) {
    rpm = 0;
    speed_kmh = 0.0;
    cnt = 0;
    measureCnt = 0;
  }

  // Calculate values if rotation occurred
  if (cnt - measureCnt >= minRotNum) {
    if (cntTime - measureTime > 0) {
      rpm = 60000L * (cnt - measureCnt) / (cntTime - measureTime);
      speed_kmh = 3.6 * ((2.0 * PI * TIRE_RADIUS_M * (float)rpm) / 60.0);
    }
    measureCnt = cnt;
    measureTime = cntTime;
  }

  // Check if the current speed is a new top speed.
  if (speed_kmh > top_speed_kmh) {
    top_speed_kmh = speed_kmh;
  }

  // --- LCD Update Section ---

  // Row 0: RPM
  lcd.setCursor(5, 0);
  lcd.print("           "); // Clear previous value
  lcd.setCursor(5, 0);
  lcd.print(rpm);

  // Row 1: Current Speed
  lcd.setCursor(7, 1);
  lcd.print("         "); // Clear previous value
  lcd.setCursor(7, 1);
  lcd.print(speed_kmh, 2);
  lcd.print(" km/h");

  // Row 2: Top Speed
  lcd.setCursor(7, 2);
  lcd.print("         "); // Clear previous value
  lcd.setCursor(7, 2);
  lcd.print(top_speed_kmh, 2);
  lcd.print(" km/h");
  
  // Row 3 remains static with "Safety First!"

  // Serial Debugging (includes top speed for comparison)
  Serial.print("RPM: ");
  Serial.print(rpm);
  Serial.print(" | Speed: ");
  Serial.print(speed_kmh);
  Serial.print(" | Top Speed: ");
  Serial.print(top_speed_kmh);
  Serial.println(" km/h");

  delay(200);
}

