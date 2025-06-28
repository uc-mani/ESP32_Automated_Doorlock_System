//ESP32 Door Lock Transmitter Code
/**
 * The ESP32 for Door Lock/Unlock Tranmitter will normally stay in deep sleep mode 
 * and wakes up only on following conditions
    >Wakeup by touchpin to transmit LoRa message
**/

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>
#include <string.h>
#include "BluetoothSerial.h"

#define RW_MODE false
#define RO_MODE true

// Pin setup
//#define LORA_SCK 18       //lora SCK pin
//#define LORA_MISO 19      //lora MISO pin
//#define LORA_MOSI 23      //lora MOSI pin
#define LORA_CS 5         //lora NSS pin
#define LORA_DIO0 26      //lora DIO0 pin
#define LORA_RST 27       //lora RESET pin
#define biLed 2           //GPIO 2 - built-in led
#define RTC_SDA 21        //RTC pin SDA
#define RTC_SCL 22        //RTC pin SCL

//Touch pins
#define TOUCH_PIN_0 T0    // GPIO 4 - to send message from LoRa
#define TOUCH_THRESHOLD 40

// Define frequency and sync word
#define LORA_FREQ 433E6 
#define LORA_SYNC_WORD 0xF3

// Global variables
int counter = 0;
String unlockTime = "52800";  //set unlock time accordingly (e.g. unlock time=25200 => 7*3600=25200 (7:00am))
String lockTime = "58000";    //set lock time accordingly (e.g. lock time=64800 => 18*3600=64800 (6:00pm))

RTC_DS3231 rtc; // RTC module init
String device_name = "DoorsGateWay";
String action_msg = "";

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

void setup() 
{
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("Boot starting...");

  //Start Bluetooth
  SerialBT.begin(device_name);  //Bluetooth device name
  //SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());

  // Pinmode pins
  pinMode(biLed, OUTPUT);

  // Initialize RTC
  initializeRTC();

  // Populate lock and unlock array from memory
  GetDoorList("unlocked");
  GetDoorList("locked");

  // Initialize LoRa
  initializeLoRa();

  // enable LoRa receiver and goto deep sleep waiting for next packet
  LoRa.receive();
  Serial.println("Running DoorUnlocker Base Transceiver!");
}

// Initialize realtime clock
void initializeRTC()
{
  // Initialize I2C for RTC
  Wire.begin(RTC_SDA, RTC_SCL);  // ESP32 default SDA: GPIO21, SCL: GPIO22

  if (!rtc.begin()) 
  {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) //always adjust current time
  {
    Serial.println("RTC lost power, lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

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
  Serial.println("LoRa init success!");
}

// Get Current time in seconds
unsigned long CurrentTimeSec()
{
  DateTime now = rtc.now();
  unsigned long CurrentTimeSec =  now.hour() * 3600 + now.minute() * 60 + now.second();

  return CurrentTimeSec;
}

void SendLoRaMsg()
{
  // Transmitting a message
  Serial.print("::Sending packet::");
  Serial.println(counter);
  String message = "," + unlockTime + "," + lockTime; //set clock (CurrentTimeSec()), unlockTime, lockTime. (Send time in seconds.)
  //Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.print(CurrentTimeSec());
  LoRa.print(message);
  LoRa.endPacket();
  Serial.println(CurrentTimeSec() + message);
  //Serial.println(message);

  counter++;

  delay(1000); //Send message after 1 seconds delay
}

// function to parse received message from LoRa transmitter
// and store the status accroding to the unit and Building number
bool parseLoRaMessage(String message) 
{
  int collonIndex1  = message.indexOf(':');
  if (collonIndex1 == -1)
  {
    Serial.println("Error: Invalid LoRa sent message format");
    return false;
  }

  String doorStatus = message.substring(0, collonIndex1);
  String doorNum = message.substring(collonIndex1 + 1);
  const char *strValue = doorNum.c_str();
  int16_t ubNo = 0; //unit and building number
  ubNo = (int16_t)strtol(strValue, NULL, 10);   // Convert the string to int16_t
  Serial.print("doorStatus:");
  Serial.println(doorStatus);
  Serial.print("doorNum:");
  Serial.println(ubNo);

  //store complete list of unitAndBuilding containing status of locked and unlocked appartements.

  if(!strcmp(doorStatus.c_str(), "unlocked") && ubNo != 0) //door unlock status received
  {
    processUnlockArr(ubNo); // add the unit&building no in unlock array
  }
  else if(!strcmp(doorStatus.c_str(), "locked") && ubNo != 0) //door lock status received
  {
    processLockArr(ubNo); // add the unit&building no in lock array
  }
  else if(!strcmp(doorStatus.c_str(), "error"))
  {
    Serial.println(doorStatus.c_str());
  }
  else
  {
    Serial.println("Error: Invalid String received from LoRa message");
  }

  return true;
}

// LORA received messages will parse the messages and update the unitAndBuilding status stored in the transmitter
// It then print out the appartments which are not locked or unlocked depending upon the operation and time.
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
      Serial.println(LoRaData); // check the validity of received data
    }

    // print RSSI of packet
    //Serial.print("' with RSSI ");
    //Serial.println(LoRa.packetRssi());
  
    if (parseLoRaMessage(LoRaData))
    {
      Serial.println("doorList updated");
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

void loop() 
{
  //send lora msg
  if(touchRead(TOUCH_PIN_0) < TOUCH_THRESHOLD)
  {
    Serial.println("Wakeup caused by touchpad");

    unsigned long timer1 = millis();
    while(millis() - timer1 < 5000) //timer runs for 10 seconds to send multiple messages from LoRa
    {
      //Serial.println("Woke up from LoRa message!");
      SendLoRaMsg();
    }
  }
  //receive lora msg
  //LoRa.receive();
  handleLoRaMessage();

  if (SerialBT.available()) 
  {
    char msg = SerialBT.read();
    if(msg != '\n')
    {
      action_msg += String(msg);
    }
    else
    {
      action_msg = "";
    }
    //Serial.write(msg);
  }

  //Serial.println(action_msg);
  if(action_msg == "unlock")
  {
    GetDoorList("unlocked");
  }
  else if(action_msg == "lock")
  {
    GetDoorList("locked");
  }

  //delay(1000);
}




