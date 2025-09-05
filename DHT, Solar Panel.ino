// Include the DHT sensor library
#include "DHT.h"

// Define the pins used
#define DHTPIN 2      // Pin connected to the DHT11 data pin
#define MOTORPIN 3    // PWM pin connected to the MOSFET Gate

// Define the sensor type
#define DHTTYPE DHT11

// Initialize the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Set your temperature threshold in Celsius
const int TEMP_THRESHOLD = 32; // Motor turns on above 28°C

void setup() {
  // Start serial communication for debugging
  Serial.begin(9600);
  
  // Set the motor pin as an output
  pinMode(MOTORPIN, OUTPUT);
  
  // Start the DHT sensor
  dht.begin();
  
  // Ensure motor is off at start
  digitalWrite(MOTORPIN, LOW);
  Serial.println("System Initialized. Motor is OFF.");
}

void loop() {
  // Wait a couple of seconds between measurements
  delay(2000);
  
  // Read temperature from the sensor
  float t = dht.readTemperature(); // Read temperature in Celsius

  // Check if any reads failed and exit early to try again.
  if (isnan(t)) {
    Serial.println("Fan is currently on");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" °C");

  // Control logic for the motor
  if (t > TEMP_THRESHOLD) {
    // Temperature is high, turn the motor on
    // Use analogWrite for speed control (0 = off, 255 = full speed)
    int motorSpeed = 200; // Set a speed (e.g., ~80% power)
    analogWrite(MOTORPIN, motorSpeed);
    Serial.println("Threshold exceeded. Motor ON.");
  } else {
    // Temperature is normal, turn the motor off
    analogWrite(MOTORPIN, 0); // Speed 0 = off
    Serial.println("Temperature is normal. Motor OFF.");
  }
}


