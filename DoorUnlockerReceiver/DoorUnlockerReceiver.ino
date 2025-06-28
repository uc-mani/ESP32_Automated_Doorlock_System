
//ESP32 Door Lock Receiver Code
/**
 * The ESP32 for Door Lock/Unlock project will normally in deep sleep mode 
 * and wakes up only on following conditions:
  1. Wakeup by timer
  2. Wakeup by touchpin
  3. Wakeup by LoRa signal

 * Setup the Pins:
    touchpin 0,4,3
    Motor In1,In2; //to control the clockwise and couter clockwise of motor rotation
    biLed
    tiltpin
   Setup Variables:
    deadboltDirection // could be clockwise or counter clockwise which locks/unlocs the door
    lock-tag
 */

#include <SPI.h>
#include <LoRa.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"

#define TOUCH_THRESHOLD 40

//define the pins used by the transceiver module
//Touch pins
#define TOUCH_PIN_0 T0    // GPIO 4 - unlock
#define TOUCH_PIN_1 T5    // GPIO 12 - set: to be defined later
#define TOUCH_PIN_2 T3    // GPIO 15 - lock
//Lora Pins
#define LORA_SCK 18                 //lora SCK pin
#define LORA_MISO 19                //lora MISO pin
#define LORA_MOSI 23                //lora MOSI pin
#define LORA_CS 5                   //lora NSS pin
#define LORA_DIO0 GPIO_NUM_26       //lora DIO0 pin //wake-up enable GPIO pin set it to 27(as pin 26 wont work)
#define LORA_RST -1 //GPIO_NUM_27   //lora RESET pin //set RESET pin = -1, to wakeup ESP32 from deepsleep
//Other pins
#define biLed 2               //GPIO 2 - built-in led
#define tiltUnlockedPin 13    //GPIO 13 - default HIGH
#define tiltLockedPin 14      //GPIO 14 - default HIGH

// Define frequency and sync word
#define LORA_FREQ 433E6 
#define LORA_SYNC_WORD 0xF3

// Global variables
//If door lock is "r"(right) aligned then rotate the motor anticlockwise to unlock the door and clockwise to lock the door
//If door lock is "l"(left) aligned then rotate the motor anticlockwise to lock the door and clockwise to unlock the door
char deadboltDirection = 'r'; //default is 'r' (right)

String unitAndBuilding = "1032"; //suppose it is random
//then probably we will add a random delay with for transmission.
long randomDelay = 10; //10 sec delay
int counter = 0;

const int in1 = 33;       // Connect to IN1 on L298N, if HIGH then rotate motor in anticlockwise direction
const int in2 = 25;       // Connect to IN2 on L298N, if HIGH then rotate motor in clockwise direction

// Shunt sensing (to stop motor when stuck)
const int shuntPin = 36; // ADC1_CH0 (GPIO36), connected across shunt resistor
const float shuntResistance = 0.1; // Ohms (example)
const float voltageRef = 3.3;      // ESP32 ADC reference voltage
const float adcResolution = 4095.0;

// Threshold for current trip
const float CURRENT_THRESHOLD = 0.8; // in Amperes (adjust per motor)

// Predefined times in seconds (offset from midnight)
const unsigned long DEFAULT_UNLOCK_TIME_SEC = 7 * 3600; //(25200 sec) 7:00 AM
const unsigned long DEFAULT_LOCK_TIME_SEC = 18 * 3600;  //(64800) 6:00 PM

// Override schedule time from LoRa
RTC_DATA_ATTR unsigned long unlockTimeSec = DEFAULT_UNLOCK_TIME_SEC;
RTC_DATA_ATTR unsigned long lockTimeSec = DEFAULT_LOCK_TIME_SEC;
RTC_DATA_ATTR unsigned long currentTime = 0;

void setup() 
{
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("Boot starting...");

  // Pinmode pins
  pinMode(tiltLockedPin, INPUT_PULLUP);
  pinMode(tiltUnlockedPin, INPUT_PULLUP);
  pinMode(biLed, OUTPUT);
  //digitalWrite(biLed, HIGH);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(shuntPin, INPUT); //ADC pin for current measurement

  blinkForXSeconds(3);
  // Initialize LoRa
  initializeLoRa(); 
  // Actions to take, after wakeup from sleep
  doActionDependingOnWakeupReason();  //"Wakeup caused by (nothing at first), timer, touch, lora msg etc."

  //******************* LoRa WakeUp *******************
  // enable LoRa receiver and goto deep sleep waiting for next packet
  LoRa.receive();
  //enable EXT0 wakeup for LoRa module receiver
  //esp_sleep_enable_ext0_wakeup(LORA_DIO0, 1); // Wake on HIGH signal   //In revisions 0 and 1 of the ESP32, this wakeup source is incompatible with ULP and touch wakeup sources.
  esp_sleep_enable_ext1_wakeup(1ULL << LORA_DIO0, ESP_EXT1_WAKEUP_ANY_HIGH);
  rtc_gpio_pullup_dis(LORA_DIO0);
  rtc_gpio_pulldown_en(LORA_DIO0);
 
  //******************* TouchPad  WakeUp *******************
  // Configure touch pads for wake up and immediate detection
  touchSleepWakeUpEnable(TOUCH_PIN_0, TOUCH_THRESHOLD);
  touchSleepWakeUpEnable(TOUCH_PIN_1, TOUCH_THRESHOLD);
  touchSleepWakeUpEnable(TOUCH_PIN_2, TOUCH_THRESHOLD);

  // Enable touch pad as wakeup source
  esp_sleep_enable_touchpad_wakeup();

  //******************* Timer WakeUp *******************
  // Calculate time to the next wakeup
  unsigned long nextWakeupTime = calculateNextWakeupTime();
  Serial.print("Next wakeup in seconds: ");
  Serial.println(nextWakeupTime);
  
  // Enable timer as wakeup source
  esp_sleep_enable_timer_wakeup(nextWakeupTime * 1000000ULL); //wakeup time is set according to the schedule defined above or updated message received in LoRa communication.
  
  //******************* ESP32 Sleep *******************
  //Go to sleep now
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}

void loop() 
{
  // This will not run in deep sleep mode
}

// Action to perform when ESP32 wakes up
void doActionDependingOnWakeupReason() 
{
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.println("Waking Up...");
  switch(wakeup_reason) 
  {
    case ESP_SLEEP_WAKEUP_EXT1: // DIO0 <-> GPIO27
    {
      Serial.println("Wakeup caused by external signal DIO0 from LoRa");
      unsigned long timer1 = millis();
      while(millis() - timer1 < 10000)
      {
        //Serial.println("Woke up from LoRa message!");
        if(handleLoRaMessage())
        {
          break;
        }
      } 
      calculateNextWakeupTime();
      checkScheduledActions();    
      break;
    }
    case ESP_SLEEP_WAKEUP_TIMER:
    {
      Serial.println("Wakeup caused by timer");
      calculateNextWakeupTime();
      currentTime += calculateNextWakeupTime();

      checkScheduledActions();                   
      break;
    }
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
    {
      Serial.println("Wakeup caused by touchpad");
      doActionDependingOnTouchPin();
      break;
    }
    default:
    {
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
    }
  }
}

/******************************
 * LORA MESSAGE HANDLING START
 ******************************/                                                                                                                                     
// Initialize LoRa module
void initializeLoRa()
{
  //setup LoRa transceiver module
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  //replace the LoRa.begin(---E-) argument with your location's frequency 
  //433E6 for Asia
  //866E6 for Europe
  //915E6 for North America
  while (!LoRa.begin(LORA_FREQ)) 
  {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  // Change sync word (0xF3) to match the receiver. The sync word assures you don't get LoRa messages from other LoRa transceivers (ranges from 0-0xFF)
  LoRa.setSyncWord(LORA_SYNC_WORD);
}

// LORA received messages will synch the module clock and override the unlock and lock timer
bool handleLoRaMessage()
{
  //Serial.println("waiting for LoRa message");

  // try to parse packet
  String LoRaData = "";
  int packetSize = LoRa.parsePacket();
  if (packetSize) 
  {
    // received a packet
    Serial.print("Received packet: ");
    // read packet
    while (LoRa.available()) 
    {
      LoRaData = LoRa.readString();
      //Serial.println(LoRaData); // check the validity of received data
    }

    // print RSSI of packet
    //Serial.print("' with RSSI ");
    //Serial.println(LoRa.packetRssi());
  
    if (parseLoRaMessage(LoRaData)) //parse and set clock, unlock time and lock time
    {
      Serial.println("Schedule updated via LoRa.");
      return true;
    }
    else
    {
      Serial.println("Invalid LoRa message");
      return false;
    }
  }
  else
  {
    //Serial.print("."); //No Packet received
    return false;
  }
}

// function to parse received message from LoRa transmitter
// and Update the time(clock, unlock and lock) from LoRa
bool parseLoRaMessage(String message) 
{
  int commaIndex1 = message.indexOf(',');
  if (commaIndex1 == -1)
  {
    Serial.println("Error: Invalid LoRa sent message format");
    return false;
  }

  int commaIndex2 = message.indexOf(',', commaIndex1 + 1);
  if (commaIndex2 == -1)
  {
    Serial.println("Error: Invalid LoRa sent message format");
    return false;
  }

  unsigned long clock = message.substring(0, commaIndex1).toInt();
  unsigned long unlockTime = message.substring(commaIndex1 + 1, commaIndex2).toInt();
  unsigned long lockTime = message.substring(commaIndex2 + 1).toInt();

  if ((clock >= 0 && clock < 86400) && (unlockTime >= 0 && unlockTime < 86400) &&  (lockTime >= 0 && lockTime < 86400))
  {
    currentTime = clock; //sync the clock
    Serial.print("currentTime: ");
    Serial.println(currentTime);
    unlockTimeSec = unlockTime;
    Serial.print("unlockTimeSec: ");
    Serial.println(unlockTimeSec);
    lockTimeSec = lockTime;
    Serial.print("lockTimeSec: ");
    Serial.println(lockTimeSec);
    return true;
  }
  Serial.println("Error: Unable to Update Time");
  return false;
}

//Send LoRa Message
void SendLoRaMsg(String message)
{
  randomDelay = random(3, 10); //random delay before sending message to make sure LoRa message is recvied properly
  Serial.print("::Sending packet with ");
  Serial.print(randomDelay);
  Serial.println("sec delay::");
  delay(randomDelay * 1000);
  //send loRa packet
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();

  Serial.println(message);
  delay(1000); //Send message after 1 seconds delay
}


/****************************
 * LORA MESSAGE HANDLING END
 ****************************/

void doActionDependingOnTouchPin() 
{
  Serial.println("doActionDependingOnTouchPin:");
  touch_pad_t touchPin = esp_sleep_get_touchpad_wakeup_status();
  Serial.print("touchPin:");
  Serial.println((touch_pad_t)touchPin); //TBC output is not as expected
  switch(touchPin) 
  {
    case 0:
      Serial.println("Touch detected on GPIO 4");
      unlockDoor();
      break;
    case 5:
      Serial.println("Touch detected on GPIO 13");
      break;
    case 3:
      Serial.println("Touch detected on GPIO 15");
      lockDoor();
      break;
    default:
      Serial.print("Wakeup by touchpad With wrong touchPin: ");
      Serial.println(touchPin);
      break;
  }
}

// Function to calculate time to the next wakeup
unsigned long calculateNextWakeupTime() 
{
  unsigned long todaySec = currentTime % 86400;  // Seconds since midnight

  if (todaySec < unlockTimeSec) 
  {
    return unlockTimeSec - todaySec;  // Time to 7:00 AM
  } 
  else if (todaySec < lockTimeSec) 
  {
    return lockTimeSec - todaySec;  // Time to 6:00 PM
  } 
  else 
  {
    return (86400 - todaySec) + unlockTimeSec;  // Time to 7:00 AM next day
  }
}

// Function to check and execute scheduled actions
// Check the internal clock to see if scheduled time has elasped or not
void checkScheduledActions() 
{
  Serial.println("checkScheduledActions");
  unsigned long todaySec = currentTime % 86400;  // Seconds since midnight
  Serial.print("todaySec:");
  Serial.println(todaySec);
  Serial.print("unlockTimeSec:");
  Serial.println(unlockTimeSec);
  Serial.print("lockTimeSec:");
  Serial.println(lockTimeSec);
  
  if (todaySec >= unlockTimeSec && todaySec < unlockTimeSec + 60)
  {
    unlockDoor();
  }
  else if (todaySec >= lockTimeSec && todaySec < lockTimeSec + 60)
  {
    lockDoor();
  }
  else
  {
    Serial.println("No Action performed");
  }
}

/*******************************
* DOOR LOCK/UNLOCK FUNCTIONALITY 
********************************/

// Read voltage drop across shunt and calculate current
float readMotorCurrent() 
{
  int raw = analogRead(shuntPin);
  float voltage = (raw / adcResolution) * voltageRef;
  float current = voltage / shuntResistance;
  Serial.print("Current (A): ");
  Serial.println(current);
  return current;
}

// Function to unlock the door
void unlockDoor()
{
  Serial.println("unlockDoor(): Unlocking Door...");
    
  int tiltSensorValue = digitalRead(tiltUnlockedPin); //tiltSensorValue = tiltUnlockedPin = 1 means door is locked
  Serial.print("tiltSensorValue: "); 
  Serial.println(tiltSensorValue);
  if(tiltSensorValue == 1)
  {
    digitalWrite(biLed, HIGH);
    retractDeadbolt();      //Rotate motor clockwise/anticlockwise to unlock door according to deadboltDirection
    delay(100);
    breakIfUnlockTripped();
    Serial.println("motor stopped");
    stopMotor();
    tiltSensorValue = digitalRead(tiltUnlockedPin); //tiltUnlockedPin should be '0' now, means door is unlocked
  }
  String message;
  //Now door is unlocked successfully, send message to LoRa transmistter including unitAndBuilding number.
  if(tiltSensorValue == 0) //'0' means door is unlocked
  {
    // Transmitting a message
    message = "unlocked: " + unitAndBuilding;
    //send message to LoRa transmistter
    SendLoRaMsg(message);
  }
  else
  {
    // Transmitting a message
    message = "error: unlocked Unsuccessful";
    //send message to LoRa transmistter
    SendLoRaMsg(message);
  }
}

//Rotate motor clockwise/anticlockwise to unlock door according to deadboltDirection
void retractDeadbolt()
{
  Serial.println("retractDeadbolt");
  if(deadboltDirection == 'r')
  {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } 
  else if (deadboltDirection == 'l')
  {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  else 
  {
    Serial.println("retractDeadbolt() error");
  }
}

// Wait till Deadbolt is fully rotated to unlock the door
void breakIfUnlockTripped ()
{
  for (int i = 0; i < 100; i++)
  {
    int tiltSensorValue = digitalRead(tiltUnlockedPin); 
    float current = readMotorCurrent();
    if(tiltSensorValue == 0 || current > CURRENT_THRESHOLD) //tiltSensorValue = tiltUnlockedPin = 0 means door is unlocked now
    {
      if(current > CURRENT_THRESHOLD)
      {
        Serial.println("High current detected! Stopping motor.");
        SendLoRaMsg("error: High current during unlock");
        delay(500);
        break;
      }
      if(tiltSensorValue == 0)
      {
        Serial.println("Door unlock detected via tilt sensor.");
        Serial.print(tiltSensorValue);
        delay(500);
        break;
      }
    }
    delay(240); //240*100 = 24 sec 
    Serial.print(".");
  }
}

// Function to lock the door
void lockDoor()
{
  Serial.println("lockDoor(): Locking Door...");

  int tiltSensorValue = digitalRead(tiltLockedPin); //tiltSensorValue = tiltLockedPin = 1 means door is unlocked
  Serial.print("tiltSensorValue: "); 
  Serial.println(tiltSensorValue);
  if(tiltSensorValue == 1)
  {
    blinkForXSeconds(7); //Blink the In-Built LED
    digitalWrite(biLed, HIGH);
    releaseDeadbolt(); //Rotate motor clockwise/anticlockwise to Lock door according to deadboltDirection
    delay(100);
    breakIfLockTripped();
    Serial.println("motor stopped");
    stopMotor();
    digitalWrite(biLed, LOW);
    tiltSensorValue = digitalRead(tiltLockedPin); //tiltLockedPin should be '0' now, means door is locked
  }
  //Now door is locked successfully, send message to LoRa transmistter including unitAndBuilding number.
  if(tiltSensorValue == 0) //'0' means door is locked
  {
    // Transmitting a message
    String message = "locked: " + unitAndBuilding;
    //send message to LoRa transmistter
    SendLoRaMsg(message);
  }
  else
  {
    // Transmitting a message
    String message = "error: locked Unsuccessful";
    //send message to LoRa transmistter
    SendLoRaMsg(message);
  }
}

//Rotate motor clockwise/anticlockwise to Lock door according to deadboltDirection
void releaseDeadbolt()
{
  Serial.println("releaseDeadbolt");
  if(deadboltDirection == 'l')
  {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } 
  else if (deadboltDirection == 'r')
  {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } 
  else 
  {
    Serial.println("releaseDeadbolt() error");
  }
}

// Wait till Deadbolt is fully rotated to lock the door
void breakIfLockTripped(){
  for (int i = 0; i < 100; i++)
  {
    int tiltSensorValue = digitalRead(tiltLockedPin);
    float current = readMotorCurrent();
    if (tiltSensorValue == 0 || current > CURRENT_THRESHOLD)
    {
      if (current > CURRENT_THRESHOLD)
      {
        Serial.println("High current detected! Stopping motor.");
        SendLoRaMsg("error: High current during lock");
        delay(500);
        break;
      }

      if (tiltSensorValue == 0)
      {
        Serial.println("Door lock detected via tilt sensor.");
        delay(500);
        break;
      }
    }
    delay(240); // 240*100 = 24 sec
    Serial.print(".");
  }
}

//Function to stop the motor
void stopMotor()
{
  pinMode(biLed, OUTPUT);
  digitalWrite(biLed, LOW);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
}

// Blink the in-built LED for x seconds
void blinkForXSeconds(int x)
{
  //pinMode(biLed, OUTPUT);
  for (int i = 0; i < x; i++)
  {
    digitalWrite(biLed, HIGH);
    delay(250);
    digitalWrite(biLed, LOW);
    delay(750);
  }
  //digitalWrite(biLed, HIGH); delay(125); digitalWrite(biLed, LOW); delay(125); digitalWrite(biLed, HIGH); delay(125); digitalWrite(biLed, LOW); delay(125); digitalWrite(biLed, HIGH); delay(125); digitalWrite(biLed, LOW); delay(125);
}


