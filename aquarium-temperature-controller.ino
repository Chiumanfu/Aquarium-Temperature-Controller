// Aquarium Temp Monitor by Chiu Fang

// Include Libraries
#include <OneWire.h>                                             // For DS18B20 Temperature Sensor
#include <DallasTemperature.h>                                   // For DS18B20 Temperature Sensor
#include <LiquidCrystalFast.h>                                   // For LCD Display
#include <SPI.h>               // For Ethernet and SD Card
#include <Ethernet.h>          // For Ethernet
#include <EthernetUdp.h>       // For UDP NTS sync

// Set constants
const byte temperaturePin = 39;                                  // DS18B20 temp sensor pin
const byte fanRelayPin = 37;
LiquidCrystalFast lcd (23, 25, 27, 29, 31, 33, 35);                     // rs,rw,en1,d4,d5,d6,d7 Init LCD
OneWire oneWire(temperaturePin);                                 // Init DS18B20 One Wire
DallasTemperature sensors(&oneWire);                             // Init Dallas Temp library
DeviceAddress temp = { 0x28, 0x2A, 0xC1, 0xD2, 0x05, 0x00, 0x00, 0xBB };     // DS18B20 ID#
//DeviceAddress temp = { 0x28, 0x50, 0x07, 0xD3, 0x05, 0x00, 0x00, 0xDD };

const float calibration = 0.6;                                   // Adjust for DS18B20 accuracy error
const float fanTempHi = 23.1;
const float fanTempLo = 22.85;

// Grovestream setup
char gsDomain[] = "grovestreams.com";                            // GroveStreams Domain
String gsApiKey = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";        // Grovestreams API Key
String gsComponentID = "Arduino";                                // Grovestreams Component ID
const unsigned long gsUpdateFrequency = 900000;                  // GroveStreams update frequency 15min
unsigned long gsLastSuccessfulUploadTime = 0;                    // Timer for interval data streams
unsigned long gsConnectAttemptTime = 0;                          // Timer for reconnects
boolean gsLastConnected = false;
int gsFailedCounter = 0;                                         // Counter for failed connects

// Ethernet Setup
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0x60, 0xCF }; // Unique MAC address
EthernetClient client; // Initialize the Etherenet Client

// Declare Variables
float tempValue;                                                 // Temperature Variable
float maxTemp = 25;                                              // Max Temperature Holder
float minTemp = 25;                                              // Min Temperature Holder
int i;                                                           // Scratchpad

void setup () {
  Serial.begin(115200);                                          // For debug
  lcd.begin(20, 4);                                              // Config LCD
  sensors.begin();                                               // Init DS18B20 sensors
  sensors.setResolution(temp, 12);                               // Set DS18B20 resolution
  sensors.requestTemperatures();                                 // Read temp from DS18B20 sensors
  tempValue = sensors.getTempC(temp) + calibration;              // Apply calibration value
  minTemp = tempValue;                                           // Preset min marker
  maxTemp = tempValue;                                           // Preset max marker
  digitalWrite(fanRelayPin, HIGH);
  pinMode(fanRelayPin, OUTPUT);
  startEthernet(); //Start or Restart the Ethernet Connection
}

void loop () {
// Get temperature from DS18B20
  sensors.requestTemperatures();                                 // Read temp from DS18B20 sensors
  tempValue = sensors.getTempC(temp) + calibration;              // Apply calibration value
  if (tempValue < minTemp && tempValue > -100) {                 // Check for Min temp
    minTemp = tempValue;
  }
  if (tempValue > maxTemp) {                                     // Check for Max temp
    maxTemp = tempValue;
  }
  lcd.setCursor(0, 0);                                           // Display info on LCD screen
  lcd.print(F("Temp:     "));
  lcd.setCursor(5, 0);
  lcd.print(tempValue, 3);
  lcd.setCursor(0, 1);
  lcd.print(F("Max :     "));
  lcd.setCursor(5, 1);
  lcd.print(maxTemp, 3);
  lcd.setCursor(0, 2);
  lcd.print(F("Min :     "));
  lcd.setCursor(5, 2);
  lcd.print(minTemp, 3);
  
// Control fan relay
  if (tempValue >= fanTempHi){
    digitalWrite(fanRelayPin, LOW); // turn fan on if tank temperature is high
  } else if (tempValue <= fanTempLo){
    digitalWrite(fanRelayPin, HIGH); // turn fan off if tank temperature drops
  }

// Create strings from floats for Grovestream string
  char temp1[7] = {0};                                           // Initialize buffer to nulls
  dtostrf(tempValue, 7, 3, temp1);                               // Convert float to string
  String tempValueS(temp1);                                      // Save string
  tempValueS.trim();                                             // Trim white space

// Start sending data
  while(client.available()) { // Send Ethernet status to serial monitor
    char c = client.read();
    Serial.print(c);
  }
  
  if(!client.connected() && gsLastConnected) { // Disconnect from GroveStreams
    Serial.println(F("...disconnected"));
    client.stop();
  }
  
  if(!client.connected() && millis() - gsLastSuccessfulUploadTime > gsUpdateFrequency) { // 15 minutes
    Serial.println(F("Starting Upload"));
    gsConnectAttemptTime = millis();                             // Set the interval timer
    if (client.connect(gsDomain, 80)) {                          // Connect to grovestream server
      Serial.println(F("Client Connect Successful"));
      String url = "PUT /api/feed?compId=" + gsComponentID;      // Construct the string
      url += "&api_key=" + gsApiKey;
      url += "&t=" + tempValueS;
      url += " HTTP/1.1";
      client.println(url);                                       //Send the string
      client.println("Host: " + String(gsDomain));
      client.println(F("Connection: close"));
      client.println(F("Content-Type: application/json"));
      client.println();
      
      Serial.println(url);                                       // Print the string for debug
   
      if (client.connected()) {
        gsLastSuccessfulUploadTime = gsConnectAttemptTime;
        gsFailedCounter = 0;
      } else {
      gsFailedCounter++; // Connection failed. Increase failed counter
      Serial.println("Connection to GroveStreams failed ("+String(gsFailedCounter, DEC)+")");  
      }
    } else {
    gsFailedCounter++; // Connection failed. Increase failed counter
    Serial.println("Connection to GroveStreams Failed ("+String(gsFailedCounter, DEC)+")");  
    }
  }
  
  if (gsFailedCounter > 3 ) { // Check if Arduino Ethernet needs to be restarted
    startEthernet();
  }

  gsLastConnected = client.connected();
  
  lcd.setCursor(14, 3);                                          // Display countdown to next update
  lcd.print(F("      "));
  lcd.setCursor(14, 3);
  lcd.print(millis() - gsLastSuccessfulUploadTime);
        
// Debug info
  Serial.print(F("Ram:"));
  Serial.print(freeRam());
  Serial.print(F(","));
  Serial.print(F("Millis:"));
  Serial.print(millis());
  Serial.println(F(","));
}

int freeRam() {                                                  // RAM monitor routine
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void startEthernet() {
  //Start or restart the Ethernet connection.
  client.stop();
  Serial.println(F("Connecting Arduino to network..."));
  Serial.println();  
  delay(2000); //Wait for the connection to finish stopping
  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Connection Failed, reset Arduino to try again"));
    Serial.println();
  } else {
    Serial.println(F("Arduino connected to network"));
    Serial.println();
  }
}
