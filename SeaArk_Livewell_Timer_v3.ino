/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updated by chegewara

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
   And has a characteristic of: beb5483e-36e1-4688-b7f5-ea07361b26a8

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   A connect hander associated with the server starts a background task that performs notification
   every couple of seconds.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
//#include <BLE2902.h>
//#include <BLE2904.h>
#include <EEPROM.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicA = NULL;
BLECharacteristic* pCharacteristicB = NULL;
BLECharacteristic* pCharacteristicC = NULL;
BLECharacteristic* pCharacteristicD = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
uint32_t onState = 1;

//These will need to be updated to the GPIO pins for each control circuit.
int POWER = 13; //13
int TIMER_SWITCH = 2; 
int WIFI_CONNECTION = 15; //15
int WIFI_CLIENT_CONNECTED = 17; //16
int Timer_LED = 16;  //17
int SPEED = 14; 
int LEFT = 12; 
int RIGHT = 13;
const int ANALOG_PIN = A0;

int onoff,powerOn = 1; 

volatile byte switch_state = HIGH;
boolean pumpOn = false;
boolean timer_state = false;
boolean timer_started = false;
boolean wifi_state = false;
boolean wifi_client_conn = false;
int startup_state;

int Clock_seconds;

int characteristic_value;

int result;

//Timer variables
hw_timer_t * timer = NULL;
int ontime_value;  //number of ON minutes store in EEPROM
int offtime_value; //number of OFF minutes store in EEPROM
int ontime;   //On time setting from mobile web app
int offtime;  //Off time setting from mobile web app

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"      //Livewell_Timer_Service_CBUUID
#define CHARACTERISTIC_A_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a6"    //Livewell_OnOff_Switch_Characteristic_CBUUID
#define CHARACTERISTIC_B_UUID "beb5483e-36e1-4688-b7f6-ea07361b26b7"    //Livewell_OFFTIME_Characteristic_CBUUID
#define CHARACTERISTIC_C_UUID "beb5483e-36e1-4688-b7f7-ea07361b26c8"    //Livewell_ONTIME_Characteristic_CBUUIDD
#define CHARACTERISTIC_D_UUID "beb5483e-36e1-4688-b7f8-ea07361b26d9"    //Livewell_TIMER_Characteristic_CBUUID

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Callback");
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("Power New value: ");
        
        char Str[] = {value[0], value[1], value[2], value[3], value[4]};
        
        Serial.println(atoi(Str));
        powerOn = atoi(Str);
        Serial.println();
        Serial.println("*********");

        if (powerOn == 1)
                  {
                    startup_state = 1;
                    EEPROM.write(0,startup_state);
                    EEPROM.commit();
                    byte value = EEPROM.read(0);
                    Serial.println("Power turned ON " + String(value));
                    timer_state = true;
                    pumpOn = true;
                    digitalWrite(Timer_LED, HIGH);
                  }
         if (powerOn == 0)
                  {
                    startup_state = 0;
                    EEPROM.write(0,startup_state);
                    EEPROM.commit();
                    byte value = EEPROM.read(0);
                    Serial.println("Power turned OFF " + String(value));
                    timer_state = false;
                    pumpOn = false;
                    digitalWrite(Timer_LED, LOW);
                  }
      }
    };
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class offTimeCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Callback");
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("Off Time New value: ");
        
        char Str[] = {value[0], value[1], value[2], value[3], value[4]};
        
        Serial.println(atoi(Str));
        offtime = atoi(Str);
        Serial.printf("Off Time = ");
        Serial.println(offtime);
        EEPROM.write(2,offtime);
        EEPROM.commit();
        delay(500);
        byte value = EEPROM.read(2);
        Serial.println("Off Time stored in EEPROM " + String(value));
        Serial.println();
        Serial.println("*********");
      }
    };
};


class onTimeCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Callback");
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("On Time New value: ");
        
        char Str[] = {value[0], value[1], value[2], value[3], value[4]};
        
        Serial.println(atoi(Str));
        ontime = atoi(Str);
        Serial.print("On Time = ");
        Serial.println(ontime);
        EEPROM.write(1,ontime);
        EEPROM.commit();
        delay(500);
        byte value = EEPROM.read(1);
        Serial.println("On Time Stored in EEPROM " + String(value));
        Serial.println();
        Serial.println("*********");
      }
    };
};

class timerCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      Serial.println("Characteristic Callback");
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("On Time New value: ");
        
        char Str[] = {value[0], value[1], value[2], value[3], value[4]};
        
        Serial.println(atoi(Str));
        offtime = atoi(Str);
        Serial.println();
        Serial.println("*********");
     }
    };
};
void setup() {

  pinMode(POWER, OUTPUT);
  //pinMode(TIMER_SWITCH, OUTPUT);
  pinMode(WIFI_CONNECTION, OUTPUT);
  pinMode(WIFI_CLIENT_CONNECTED, OUTPUT);
  pinMode(Timer_LED, OUTPUT);
  pinMode(SPEED, OUTPUT);
  
  digitalWrite(POWER, LOW);
  //digitalWrite(TIMER_SWITCH, LOW);
  digitalWrite(WIFI_CONNECTION, LOW);
  digitalWrite(WIFI_CLIENT_CONNECTED, LOW);
  digitalWrite(Timer_LED, LOW);
  digitalWrite(SPEED, LOW);
  
  Serial.begin(115200);

  Serial.println("Start up state of pump is OFF");
  timer_state = false;
  pumpOn = false;

  EEPROM.begin(3); //Index of three for - On/Off state 1 or 0, OnTime value, OffTime value

  //Determine the value set for ON Time in EEPROM
  ontime_value = EEPROM.read(1);
  if (ontime_value > 0) 
  {
    //ontime = ontime_value;
    Clock_seconds = ontime_value*5;
    Serial.println("On Time setting is " + String(Clock_seconds));
  }
    
  //Determine the value set for OFF Time inEEPROM
  offtime_value = EEPROM.read(2);
  if (offtime_value > 0) 
  {
    //offtime = offtime_value;
    Clock_seconds = offtime_value*5;
    Serial.println("OFF Time setting is " + String(Clock_seconds));
  }

  
  // Create the BLE Device
  BLEDevice::init("SArkT");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic A
  pCharacteristicA = pService->createCharacteristic(
                      CHARACTERISTIC_A_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
                    
// Create a BLE Characteristic B
  pCharacteristicB = pService->createCharacteristic(
                      CHARACTERISTIC_B_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

// Create a BLE Characteristic C
  pCharacteristicC = pService->createCharacteristic(
                      CHARACTERISTIC_C_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

// Create a BLE Characteristic D
  pCharacteristicD = pService->createCharacteristic(
                      CHARACTERISTIC_D_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE_NR  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  //pCharacteristicA->addDescriptor(new BLE2902());
  //pCharacteristicB->addDescriptor(new BLE2904());

  pCharacteristicA->setCallbacks(new MyCallbacks());
  pCharacteristicB->setCallbacks(new offTimeCallback());
  pCharacteristicC->setCallbacks(new onTimeCallback());
  pCharacteristicD->setCallbacks(new timerCallback());

  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}


void IRAM_ATTR onoffTimer(){

  switch (powerOn) {

    case 0:
      if (pumpOn == false) 
      {
        digitalWrite(TIMER_SWITCH,LOW);  //We only need to set TIMER_SWITCH once....set pumpOn to TRUE in prep for end of OFFTIME.
        pumpOn = true;
        Serial.println("Turning OFF pump");
        digitalWrite(Timer_LED, LOW);
      }
      //Serial.println("Pump has been OFF for " + String(Clock_seconds) + " seconds");
      
      pCharacteristicD->setValue((uint8_t*)&Clock_seconds, 4);
      pCharacteristicD->notify();
      Clock_seconds--;
      if (Clock_seconds < 1){
        powerOn = 1;
        Clock_seconds = ontime*5;
      }
      break;
    
    case 1:
      if (pumpOn == true) {
        digitalWrite(TIMER_SWITCH,HIGH);  //We only need to set TIMER_SWITCH once....set pumpOn to FALSE in prep for end of OFFTIME.
        pumpOn = false;
        Serial.println("Turning ON pump");
        digitalWrite(Timer_LED, HIGH);
      }
      //Serial.println("Pump is running for " + String(Clock_seconds) + " seconds");
    
      pCharacteristicD->setValue((uint8_t*)&Clock_seconds, 4);
      pCharacteristicD->notify();
      Clock_seconds--;
      if (Clock_seconds < 1) {
        powerOn = 0;
        Clock_seconds = offtime*5;
      }
      break;
  }
}


void startTimer(){
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onoffTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  yield();
  timerAlarmEnable(timer);
  timer_started = true;
  Serial.println("Timer Started");
}

void stopTimer(){
  if (timer != NULL) {
    timerAlarmDisable(timer);
    timerDetachInterrupt(timer);
    timerEnd(timer);
    timer = NULL;
    timer_started = false;
    Serial.println("Timer Stopped");
    //digitalWrite(TIMER_SWITCH,LOW);
    //digitalWrite(Timer_LED,LOW);
  }
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        //Serial.println("device connected");
        //pCharacteristicD->setValue((uint8_t*)&value, 4);
        //pCharacteristicD->notify();
        value++;
        delay(1000); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
        //pCharacteristicB->setValue((uint8_t*)&onState, 4);
        //pCharacteristicB->notify(); 
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        Serial.println(" Device is connecting");
        oldDeviceConnected = deviceConnected;
    }
    if ((timer_state == true) && (timer_started == false)) startTimer();
    if ((timer_state == false) && (timer_started == true)) stopTimer();  
}
