// Required for Bluetooth functionality
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Motor status definitions
#define Hold 0
#define Move 1

// Motor control pins
#define INA 18
#define INB 19

// Analog-digital converter and reference voltage
#define ADread 34
#define rfv 155

// Obstruction detection threshold
#define zuse 50 

// Photocells for end position detection
#define light1 15
#define light2 23

// Initialize the BluetoothSerial library
BluetoothSerial SerialBT;

// Initialize status variables
bool Movestatus = Move;
bool isopen = 0;
bool recode_terminal = 0;

// Setup Bluetooth module
void btsetup() {
  Serial.begin(115200);          // Set baud rate to 115200
  SerialBT.begin("ESP32");       // Set Bluetooth module name to "ESP32"
}

// Check if Bluetooth command is received and update lock status
bool bttest() {
  if (SerialBT.available()) {
    char tempc = SerialBT.read();
    // Clear the Bluetooth buffer
    while (SerialBT.available()) {
      SerialBT.read();
    }
    // Print received command and lock status for debugging
    Serial.println("Received Bluetooth command");
    Serial.print(tempc);
    Serial.print(" Lock status: ");
    Serial.println(isopen);
    // Return new lock status based on command received
    return tempc == '1';
  }
  // If no command is received, return the current lock status
  return isopen;
}

// Read the photocell sensors and determine if the end position is reached
bool Move_terminal() {
  return (analogRead(light1) > 1000) || (analogRead(light2) > 1000);
}

// Read the ADC value for current obstruction test
int curtest() {
  int advalue = analogRead(ADread);
  Serial.println(advalue);
  return advalue;
}

// Setup motor pins as output
void motorsetup() {
  pinMode(INA, OUTPUT);
  pinMode(INB, OUTPUT);
}

// Check if there is an obstruction based on ADC value
bool Move_curtest() {
  return curtest() >= zuse;
}

// Define motor movement functions
void forward() {
  bool high = isopen ^ HIGH;
  digitalWrite(INA, high);
  digitalWrite(INB, !high);
}

void back() {
  bool high = isopen ^ HIGH;
  digitalWrite(INA, !high);
  digitalWrite(INB, high);
}

// Define motor status functions
void brake() {
  digitalWrite(INA, HIGH);
  digitalWrite(INB, HIGH);
}

void standby() {
  digitalWrite(INA, LOW);
  digitalWrite(INB, LOW);
}

// Arduino setup function
void setup() {
  btsetup();
  Serial.println("Bluetooth setup complete"); 
  motorsetup();
  Serial.println("Motor setup complete");
}

// Arduino main loop function
void loop() {
  // Check if lock state has changed
  if (isopen != bttest()) {
    isopen = !isopen;
    Serial.println("Lock state changed");
    Serial.println(isopen ? "Locked" : "Unlocked");
    Movestatus = Move;
    forward();
    delay(200);
  }

  // If in Hold state, perform reset detection
  if (Movestatus == Hold) {
    Serial.print("Movebutton=");
    Serial.println(light1);
    Serial.print("Movebutton2=");
    Serial.println(light2);
    
    // If light1 is blocked, keep moving forward
    while ((analogRead(light1) > 1000) && recode_terminal) {
      forward();
      delay(20);
      standby();  
    }
    // If light2 is blocked, keep moving forward
    while ((analogRead(light2) > 1000) && (!recode_terminal)) {
      forward();
      delay(20);
      standby();    
    }
    delay(200);
  } else {
    // In Move state, continue moving forward
    forward();
    Serial.print("ADC reading: ");   
    Serial.println(analogRead(ADread));
    delay(30);
    
    // If movement reaches a predefined endpoint
    if (Move_terminal()) {
      Serial.println("Reached endpoint");
      brake(); // Stop moving
      Movestatus = Hold; // Change status to Hold
      recode_terminal = (analogRead(light1) > 1000);
      Serial.print("End position on right: ");
      Serial.println(recode_terminal);
    } else if (Move_curtest()) {
      // If movement is obstructed
      Serial.println("Obstruction detected");
      brake(); // Stop motor
      delay(100);    
      back(); // Move backward
      delay(10);
      standby(); // Enter standby mode
      delay(500);
    }
  }
}
