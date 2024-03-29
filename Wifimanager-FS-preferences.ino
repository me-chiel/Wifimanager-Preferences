/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/


/*
Me-Chiel
Updated this tutorial to eliminate dependency on SPIFFS (as it's cumbersome in Arduino IDE 2) and utilize the Preferences library instead.
This tutorial leverages a segment of the ESP32's built-in non-volatile memory (NVS) for data storage. 
This data persists even through system restarts and power loss events.

Additionally, a feature has been incorporated to allow resetting of this memory by holding down the BOOT button for 10 seconds
(or 3 seconds for a standard reset).
*/               
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "Preferences.h"

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_INPUT_1 = "ssid";
const char* PARAM_INPUT_2 = "pass";
const char* PARAM_INPUT_3 = "ip";
const char* PARAM_INPUT_4 = "gateway";

// variable for reset button on ESP32 (boot button)
static int gpio_0 = 0;

//Variables to save values from HTML form
String ssid="";
String pass="";
String ip="";
String gateway="";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

Preferences preferences;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP WEB SERVER</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" href="style.css">
    <link rel="icon" type="image/png" href="favicon.png">
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  </head>
  <body>
    <div class="topnav">
      <h1>ESP WEB SERVER</h1>
    </div>
    <div class="content">
      <div class="card-grid">
        <div class="card">
          <p class="card-title"><i class="fas fa-lightbulb"></i> GPIO 2</p>
          <p>
            <a href="on"><button class="button-on">ON</button></a>
            <a href="off"><button class="button-off">OFF</button></a>
          </p>
          <p class="state">State: %STATE%</p>
        </div>
      </div>
    </div>
  </body>
</html>
)rawliteral";

const char index_html3[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  html {font-family: Arial, Helvetica, sans-serif;display: inline-block; text-align: center;}
h1 {font-size: 1.8rem; color: white;}
p { font-size: 1.4rem;}
.topnav { overflow: hidden;  background-color: #0A1128;}
body {    margin: 0;}
.content {   padding: 5%;}
.card-grid {   max-width: 800px;   margin: 0 auto;   display: grid;   grid-gap: 2rem;   grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
.card {   background-color: white;   box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
.card-title {   font-size: 1.2rem;  font-weight: bold;  color: #034078}
input[type=submit] {  border: none;  color: #FEFCFB;  background-color: #034078;  padding: 15px 15px;  text-align: center;  text-decoration: none;  display: inline-block;  font-size: 16px;  width: 100px;  margin-right: 10px;  border-radius: 4px;  transition-duration: 0.4s;  }
input[type=submit]:hover {  background-color: #1282A2;}
input[type=text], input[type=number], select {  width: 50%;  padding: 12px 20px;  margin: 18px;  display: inline-block;  border: 1px solid #ccc;  border-radius: 4px;  box-sizing: border-box;}
label {  font-size: 1.2rem; }
.value{  font-size: 1.2rem;  color: #1282A2;  }
.state {  font-size: 1.2rem;  color: #1282A2;}
button {  border: none;  color: #FEFCFB;  padding: 15px 32px;  text-align: center;  font-size: 16px;  width: 100px;  border-radius: 4px;  transition-duration: 0.4s;}
.button-on {  background-color: #034078;}
.button-on:hover {  background-color: #1282A2;}
.button-off {  background-color: #858585;}
.button-off:hover {  background-color: #252524;} 
  </style>
  
</head>
<body>
  <div class="topnav">
    <h1>ESP Wi-Fi Manager</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <label for="ip">IP Address</label>
            <input type="text" id ="ip" name="ip" value="192.168.1.200"><br>
            <label for="gateway">Gateway Address</label>
            <input type="text" id ="gateway" name="gateway" value="192.168.1.1"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";




// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || pass==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());


  if (!WiFi.config(localIP, localGateway, subnet)){
    Serial.println("STA Failed to configure");
    return false;
  }
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    delay(1000);Serial.print(".");
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }
  Serial.println("");
  Serial.println(WiFi.localIP());
  return true;
}

// Replaces placeholder with LED state value
String processor(const String& var) {
  if(var == "STATE") {
    if(digitalRead(ledPin)) {
      ledState = "ON";
    }
    else {
      ledState = "OFF";
    }
    return ledState;
  }
  return String();
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  //preferences.begin("memory", false); //Uncomment if you want to clear the NVS (on-board non-volatile memory)
  //preferences.clear();                //Uncomment if you want to clear the NVS (on-board non-volatile memory)
  //Serial.println("NVS ereased!");     //Uncomment if you want to clear the NVS (on-board non-volatile memory)
  //preferences.end();                  //Uncomment if you want to clear the NVS (on-board non-volatile memory)
  preferences.begin("memory", false);
  unsigned int counter = preferences.getUInt("counter", 0);  // Get the counter value, if the key does not exist, return a default value of 0
  counter++;                                                 // Increase Reset counter by 1
  Serial.printf("Current reboots: %u\n", counter);           // Print the counter to Serial Monitor
  preferences.putUInt("counter", counter);                   // Store the counter to the Preferences
  ssid=preferences.getString("ssid","");                     // get ssid, or return "" if empty
  pass=preferences.getString("pass","");                     // same
  ip=preferences.getString("ip","");                         // same
  gateway=preferences.getString("gateway","");               // same
  preferences.end(); // Close the Preferences

  
  pinMode(ledPin, OUTPUT);// Set GPIO 2 as an OUTPUT
  digitalWrite(ledPin, LOW);
  pinMode(gpio_0, INPUT); // Set GPIO 0 as an INPUT
  Serial.print("ssid:");Serial.println(ssid);
  Serial.print("pass:");Serial.println(pass);
  Serial.print("ip:");Serial.println(ip);
  Serial.print("gateway:");Serial.println(gateway);

  if(initWiFi()) {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html, processor);
    });
    
    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, HIGH);
      request->send_P(200, "text/html", index_html, processor);
    });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
      digitalWrite(ledPin, LOW);
      request->send_P(200, "text/html", index_html, processor);
    });
    server.begin();
  }
  else {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("ESP-WIFI-MANAGER", NULL);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", index_html3, processor);
    });
    
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            preferences.begin("memory", false);
            preferences.putString("ssid", ssid);
            preferences.end();
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            preferences.begin("memory", false);
            preferences.putString("pass", pass);
            preferences.end();
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            preferences.begin("memory", false);
            preferences.putString("ip", ip);
            preferences.end();
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            preferences.begin("memory", false);
            preferences.putString("gateway", gateway);
            preferences.end();
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
}


void loop() {
    if (digitalRead(gpio_0) == LOW) {  // Push button pressed
        Serial.println("Boot button pressed");     
        // Key debounce handling
        delay(100);
        int startTime = millis();
        int countdown = 1;
        while (digitalRead(gpio_0) == LOW) {
          Serial.println(countdown);
            delay(1000);
            countdown++;
            
        }
        int endTime = millis();

        if ((endTime - startTime) > 10000) {
            // If key pressed for more than 10secs, reset all
            Serial.println("Reset NVS");
            preferences.begin("memory", false); 
            preferences.clear();                
            Serial.println("NVS ereased!");     
            preferences.end(); 
            ESP.restart();                 
        } else if ((endTime - startTime) > 3000) {
            Serial.println("Reset ESP32");
            // If key pressed for more than 3secs, but less than 10, reset ESP32
           ESP.restart();
        } 
    }
    delay(100);
}
