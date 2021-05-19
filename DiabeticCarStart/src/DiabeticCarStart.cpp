/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "c:/Users/Killi/Documents/IoT/Diabetic_Car_Start/DiabeticCarStart/src/DiabeticCarStart.ino"
/*
 * Project DiabeticCarStart
 * Description:
 * Author: Kaleb Glodowski
 * Date: MAY-11-2021
 */

#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h" 
#include "Adafruit_MQTT/Adafruit_MQTT.h" 
#include <SPI.h>
#include "SdFat.h"
#include <neopixel.h>
#include <colors.h>
#include <Adafruit_SSD1306.h>
#include "credentials.h"

void setup();
void loop();
void collectData();
void logData(char* timeLog, int data1);
void createName();
void write2SD();
void MQTT_connect();
void MQTT_Publish();
void MQTT_Subscribe();
void _dateTime();
void OLED_display();
void unlockCar();
#line 19 "c:/Users/Killi/Documents/IoT/Diabetic_Car_Start/DiabeticCarStart/src/DiabeticCarStart.ino"
#define FILE_BASE_NAME "Data" // Log file base name.  Must be six characters or less.
#define OLED_RESET D4

/************ Global State (you don't need to change this!) ***   ***************/ 
TCPClient TheClient; 

/************Declare Constants*************/

const int SCREEN_ADDRESS = 0x3C;                           //OLED| IS2 address
const int SD_CHIPSELECT = A4;                              //uSD | placed on A4
const int SD_INTERVAL_MS = 1000;                           //uSD
const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1; //uSD | for the file name
// const int PIXELCOUNT = 1;                               //neoPixel | amount of neoPixels being used
// const int PIXELPIN = A5;                                //neoPixel | data pin
const int GLUCOSEPIN = A1;                                 //Glucose Monitor | data in pin
const int RELAYPIN = D7;                                   //Relay | data pin
//Variables

SdFat sd;                                    //uSD | File system object.
SdFile file;                                 //uSD | Log file.
unsigned int logTime;                        //uSD | Time in micros for next data record.
char fileName[13] = FILE_BASE_NAME "00.csv"; //uSD | file variable
float value;                                 //MQTT subscribe | value being pulled from the dashboard
unsigned int last;                           //MQTT connect | timer
unsigned int lastTime;                       //MQTT publish | timer
int glucoseRead;                             //Glucose Monitor | data in reading
int lastRead;                                //Glucose Monitor | saves last reading to compare to new reading
bool carCanBeOn;                             //Carstart | boolean turned on when button pressed on adafruit dashboard
unsigned int carCanBeOnTimer;                //Carstart | timer since carCanBeOn
String DateTime, TimeOnly;                   //DateTime | time and date variables seperate
char currentDateTime[25], currentTime[9];    //DateTime | combined DateTime and current time

//Objects

// Adafruit_NeoPixel pixel (PIXELCOUNT, PIXELPIN); //neoPixel
Adafruit_SSD1306 oled(OLED_RESET);

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details. 
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/****************************** Feeds ***************************************/ 
// Setup Feeds to publish or subscribe 
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname> 
Adafruit_MQTT_Subscribe startOverrideFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/carStartOverride");
Adafruit_MQTT_Publish glucoseMonFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/glucoseMon");

void setup() {
  //Connecting to particle universal timer
  Particle.connect(); //connecting to particle cloud
  Time.zone ( -4) ; //EST = -4 MST = -7, MDT = -6
  Particle.syncTime () ; // Sync time with Particle Cloud
  
  //Starting serial monitoring
  Serial.begin(9600);
  delay(1000);
  Serial.println("Serial monitor started.");

  pinMode (GLUCOSEPIN, INPUT); //glucose data in pin

  //Starting neoPixel
  // pixel.begin();
  // pixel.show();

  //Starting uSD reader
  if (!sd.begin(SD_CHIPSELECT, SD_SCK_MHZ(50))) {
    Serial.printf("Error starting SD Module"); 
    sd.initErrorHalt();
    return;
  }
  Serial.println("SD card intialized.");

  //Ensuring file name is in acceptable range
  if (BASE_NAME_SIZE > 6) {
    Serial.println("FILE_BASE_NAME too long");
    while(1); //stop program
  } 

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
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0,0);

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&startOverrideFeed);
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
  if (glucoseRead != lastRead) {
    Serial.printf("Glucose reads: %i.\n",glucoseRead);   
    _dateTime();      //pulls date/time from the particle cloud servers universal time
    write2SD();       //records the glucose monitor data to an SD card
    lastRead = glucoseRead;
  }
}

void logData(char* timeLog, int data1) {
  file.printf("%s , %i \n",timeLog,data1);
}

void createName() {
  Serial.printf("Starting Create Name function.\n");
  while (sd.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } 
    else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } 
    else {
      Serial.println("Can't create file name");
      while(1); //stop program
    }
  }  
}

void write2SD() {
  createName();                                               //Button pressed, create file and name
  if (!file.open(fileName, O_WRONLY | O_CREAT | O_EXCL)) {    //check if file open
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

// // void neoPixel() {

//   //respond to lock/unlock of 
//   pixel.setPixelColor(i, 0xe942f5);
//   pixel.setBrightness(30);
//   pixel.show();

// }

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
      glucoseMonFeed.publish(glucoseRead);
      Serial.printf("Publishing %i \n",glucoseRead);                           
      } 
    lastTime = millis();
  }
}

void MQTT_Subscribe() {
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(1000))) {
    if (subscription == &startOverrideFeed) {
      value = atof((char *)startOverrideFeed.lastread);
      carCanBeOn = true; //closes relay to allow car to start without needing the glucose reading
      carCanBeOnTimer = millis() + 300000; //sets a timer for 5 minutes
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
  if (glucoseRead > 200 && carCanBeOn != true) {
    digitalWrite(RELAYPIN, HIGH);
    carCanBeOn = true;
    carCanBeOnTimer = millis() + 300000; //sets a timer for 5 minutes
  }
  if (carCanBeOn == true && millis() > carCanBeOnTimer) {   //opens relay after 5 minutes
    digitalWrite(RELAYPIN, LOW);
    carCanBeOn = false;
  }

  //displayed to OLED (locked/unlocked and reading, no timestamp necessary)
}