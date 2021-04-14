
// include the library code:
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>

bool alarmTriggerState();
bool lightState();


///////////////Wifi Access Credentials 
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

//Your IP address or domain name with URL path
const char* serverNameAlarmState = "http://192.168.4.1/alarmState";
const char* serverNameAlarmShutOff = "http://192.168.4.1/alarmShutOff";

////Setup Global Vars
unsigned long previousMillis = 0;
const long interval = 1000;
String alarmstate;


//LED, Buzzer vars
int buzzer = 4;
int redLED = 2;
int greenLED = 13;
int lightValue;


/////////////////Button Setup////////////////////////
struct InputButton {
  int GPIO;
  bool pressed;
};

InputButton ib1 = {22, false};

void IRAM_ATTR buttonISR(){
  ib1.pressed = true;
  Serial.printf("ISR Test\n");
}

void setup() {
 
  Serial.begin(115200);
  pinMode(buzzer,OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

///////////////////////Connecting to WiFi///////////////////////////////////////
 WiFi.begin(ssid, password);
 Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
///////////////////setup interrupt for button, photoresistor, and LED/////////////////////////
  pinMode(ib1.GPIO, INPUT_PULLUP);
  attachInterrupt(ib1.GPIO, buttonISR, FALLING);
}


void loop() {
  unsigned long currentMillis = millis();
  bool flag_lightStatus;
  
  if(currentMillis - previousMillis >= interval) {
     // Check WiFi connection status

    //HTTP GET from client is requesting status of alarm state
    if(WiFi.status()== WL_CONNECTED){ 
      alarmstate = httpGETRequest(serverNameAlarmState);  
      Serial.println("Alarm State = " + alarmstate);

      flag_lightStatus = lightState();
      alarmTriggerState(alarmstate);
  

      //interrupt from button or light sensor, sends message to server to enable flag that it has been disabled
      if (ib1.pressed){
          if(flag_lightStatus){
               httpGETRequest(serverNameAlarmShutOff);
              //Serial.printf("Testing Interrupt\n");
               
               alarmstate = "false";
                }
           ib1.pressed = false;
      }  
      // save the last HTTP GET Request
      previousMillis = currentMillis;
    }
    else {
      delay(500);
      Serial.println("WiFi Disconnected");
    }
  }
  



}



bool alarmTriggerState(String alarmTriggerFlag) //switch case in main? depends how we integrate this code with Justins
{
  if (alarmTriggerFlag == "true"){ //optimize this later on by doing nested if's, with most likely to fail first (timematch)
    digitalWrite(buzzer,HIGH);
    digitalWrite(redLED, HIGH);
    digitalWrite(greenLED, LOW);
    //add in if button isnt pressed wait here, if its pressed then it can exit
  }
  else
  {
    digitalWrite(buzzer,LOW);
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
  }
}



bool lightState()
{
  
  lightValue = analogRead(34); //photoresistor pin 34
  Serial.println("Analog value : ");
  Serial.println(lightValue);

  if (lightValue < 4000){ //adjust value based on typical lightvalue
    return false;
  }
  else{
    return true;
  }
}


String httpGETRequest(const char* serverName) {
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "--"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
