/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "c:/Users/Killi/Documents/IoT/Diabetic_Car_Start/DiabeticCarStart/src/DiabeticCarStart.ino"
/*
 * Project DiabeticCarStart
 * Description: see readme
 * Author: Kaleb Glodowski
 * Date: MAY-11-2021
 */

#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h" 
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include <SPI.h>
#include "SdFat.h"
#include <Adafruit_SSD1306.h>
#include "credentials.h"

void setup();
void loop();
void collectData();
void logData(char* timeLog, int data1);
void write2SD();
void MQTT_connect();
void MQTT_Publish();
void MQTT_Subscribe();
void _dateTime();
void OLED_display();
void unlockCar();
#line 17 "c:/Users/Killi/Documents/IoT/Diabetic_Car_Start/DiabeticCarStart/src/DiabeticCarStart.ino"
#define OLED_RESET D4

/************ Global State (you don't need to change this!) ******************/ 
TCPClient TheClient; 

/************Declare Constants*************/

const int SCREEN_ADDRESS = 0x3C;                           //OLED| IS2 address
const int SD_CHIPSELECT = A4;                              //uSD | placed on A4
const int SD_INTERVAL_MS = 1000;                           //uSD
const int GLUCOSEPIN = A1;                                 //Glucose Monitor | data in pin
const int RELAYPIN = D7;                                   //Relay | data pin

//Variables

unsigned int logTime;                        //uSD | Time in micros for next data record.
char fileName[12] = "datalog.csv";           //uSD | file variable
float value;                                 //MQTT subscribe | value being pulled from the dashboard
unsigned int last;                           //MQTT connect | timer
unsigned int lastTime;                       //MQTT publish | timer
int glucoseRead;                             //Glucose Monitor | data in reading
bool hasRead;                                //Glucose Monitor | glucose monitor has been read recently or not
unsigned int glucoseTimer;                   //Glucose Monitor | 60 second cooldown timer when glucose can not be read
bool displayedBad;                           //Glucose Monitor | has displayed bad glucose readings recently?
bool displayedWait;                          //Glucose Monitor | has displayed wait recently
bool carCanBeOn;                             //Carstart | boolean turned on when button pressed on adafruit dashboard
unsigned int carCanBeOnTimer;                //Carstart | timer since carCanBeOn
String DateTime, TimeOnly;                   //DateTime | time and date variables seperate
char currentDateTime[25], currentTime[9];    //DateTime | combined DateTime and current time

//Objects

Adafruit_SSD1306 oled(OLED_RESET);           //OLED| object
SdFat sd;                                    //uSD | File system object.
SdFile file;                                 //uSD | Log file.

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/****************************** Feeds ***************************************/ 
// Setup Feeds to publish or subscribe 
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname> 
Adafruit_MQTT_Subscribe startOverrideFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/carstartoverride");
Adafruit_MQTT_Publish glucoseMonFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/glucosemon");

void setup() {
  //Connecting to particle universal timer
  Particle.connect();      //connecting to particle cloud
  Time.zone ( -4) ;        //EST = -4 MST = -7, MDT = -6
  Particle.syncTime () ;   // Sync time with Particle Cloud
  
  //Starting serial monitoring
  Serial.begin(9600);
  delay(1000);
  Serial.println("Serial monitor started.");

  pinMode (GLUCOSEPIN, INPUT); //glucose data in pin
  pinMode (RELAYPIN, OUTPUT);  //relay output data pin

  //Starting uSD reader
  if (!sd.begin(SD_CHIPSELECT, SD_SCK_MHZ(50))) {
    Serial.printf("Error starting SD Module"); 
    sd.initErrorHalt();
    return;
  }
  Serial.println("SD card intialized.");

  //Connecting to Wifi
  Serial.printf("Connecting to Internet \n");
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
    delay(100);
  }
  Serial.printf("\n Connected!!!!!! \n");

  //OLED settings
  oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); //NEED THIS!
  oled.display();
  delay(3000);
  oled.clearDisplay();
  oled.display();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&startOverrideFeed);

  digitalWrite(RELAYPIN, HIGH); //opening the relay, it is set-up as a NORMALLY CLOSED.
}

void loop() {
  collectData();    //collecting data from glucose monitor and saving data and time to an SD card
  unlockCar();      //closes relay to allow car to start, will re-open relay after 5 minutes of being closed | displays status to OLED
  MQTT_connect();   //connects to the MQTT server
  MQTT_Publish();   //publishes glucose monitor data to the Adafruit.io dashboard
  MQTT_Subscribe(); //pulls any button input from the adafruit.io dashboard
}

void collectData() {
  glucoseRead = analogRead(GLUCOSEPIN);
  if (hasRead != true && glucoseRead < 3500) {
    Serial.printf("Glucose reads: %i.\n",glucoseRead);   
    _dateTime();                                           //pulls date/time from the particle cloud servers universal time
    write2SD();                                            //records the glucose monitor data to an SD card
    hasRead = true;
    glucoseTimer = millis() + 10000; 
  }
  if (millis() > glucoseTimer) {      //allows glucose to be read again after 60 seconds
    hasRead = false;                
  } 
}

void logData(char* timeLog, int data1) {
  file.printf("%s , %i \n",timeLog,data1);
}

void write2SD() {                                             //Button pressed, create file and name
  if (!file.open(fileName, FILE_WRITE)) {                //check if file open
    Serial.println("File Failed to Open");
    while(1);                                                 //stop program
  }  
  Serial.printf("File opened. \n");                           //timestamp
  Serial.printf("Logging to: %s.\n",fileName);
  logData(currentDateTime, glucoseRead);                      //logging the data of the Glucose Monitor
  file.close();                                               //closing file
  Serial.printf("Done.\n");
  Serial.println("Completed writing to SD.");
}

void MQTT_connect() {
  // Function to connect and reconnect as necessary to the MQTT server.
  // Should be called in the loop function and it will take care if connecting.
  int8_t ret;
 
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");

  //pinging MQTT to keep connection open
  if ((millis()-last)>120000) {
    Serial.printf("Pinging MQTT \n");
    if(! mqtt.ping()) {
      Serial.printf("Disconnecting \n");
      mqtt.disconnect();
      }
      last = millis();
  }
}

void MQTT_Publish() {
  if((millis()-lastTime > 60000)) { //publish every 60 seconds
    if(mqtt.Update()) {
      if (hasRead != true && glucoseRead < 3500) {
        glucoseMonFeed.publish(glucoseRead);
        Serial.printf("Publishing %i \n",glucoseRead);
        lastTime = millis();      
      }                     
    } 
  }
}

void MQTT_Subscribe() {
  Serial.println("Checking subscribe.");
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    if (subscription == &startOverrideFeed) {
      value = atof((char *)startOverrideFeed.lastread);
      carCanBeOn = true;                                 
      digitalWrite(RELAYPIN, LOW);                        //closes relay to allow car to start without needing the glucose reading
      OLED_display();
      oled.printf("Manual Override\nhas been pressed\n");
      oled.setTextSize(2);
      oled.printf("Unlocked.");
      oled.display();
      Serial.println("Manual dashboard button pressed. Car can be turned on without readings.");
    }
  }  
}

void _dateTime() {
  DateTime = Time.timeStr();            //current data and time from particle time class
  TimeOnly = DateTime.substring(11,19); //Extract the time from the datetime string
  //Convert String to char arrays - this is needed for formatted print
  DateTime.toCharArray(currentDateTime, 25);
  TimeOnly.toCharArray(currentTime, 9);
  //Print using formatted print
  Serial.printf("Date and time is %s.\n", currentDateTime);
  Serial.printf("Time is %s.\n", currentTime);
}

void OLED_display() {
  //Clear OLED
  oled.clearDisplay();
  oled.display();
  //OLED Settings
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);
}

void unlockCar() {
  if (glucoseRead < 650 && glucoseRead > 100 && carCanBeOn != true) {
    digitalWrite(RELAYPIN, LOW); //closing relay
    carCanBeOn = true;
    OLED_display();
    oled.printf("Blood Glucose is in\nacceptable parameters\n"); 
    oled.printf("Reading: %i.\n", glucoseRead);
    oled.setTextSize(2);
    oled.printf("Unlocked."); 
    oled.display();
    Serial.println("Glucose reads good, closing relay for car to start.");
    displayedBad = false;
    displayedWait = false;
  }
  if (glucoseRead >= 650 && glucoseRead < 3500 && displayedBad != true && carCanBeOn != true) {
    digitalWrite(RELAYPIN, HIGH); //ensuring relay is open
    OLED_display();
    oled.printf("Blood Glucose is not in acceptable range\n");
    oled.printf("Reading: %i.\n", glucoseRead); 
    oled.setTextSize(2);
    oled.printf("Locked.");   
    oled.display();  
    Serial.println("Glucose reads bad, keeping relay open.\n Recommend corrections and re-test..");
    displayedBad = true;
    displayedWait = false;
  }
  if ((glucoseRead >= 3500 && displayedWait != true && carCanBeOn != true) || (glucoseRead <= 100 && displayedWait != true && carCanBeOn != true)) {
    //Clear OLED
    OLED_display();
    oled.setTextSize(2);
    oled.printf("Waiting\nfor\nglucose\nreading...");
    oled.display();
    Serial.println("Waiting for glucose reading...");
    displayedBad = false;
    displayedWait = true; 
  }
}