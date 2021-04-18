// include the library code:
#include <LiquidCrystal.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include "time.h"
#include "ESPAsyncWebServer.h"

#define disabled 0
#define enabled 1

void printLocalTime();
bool alarmSetState();
bool alarmTriggerState();
bool lightState();
bool alarmOffFunction();

//LCD, LED, Buzzer vars
LiquidCrystal lcd(19, 23, 18, 17, 16, 15);
int buzzer = 4;
int redLED = 2;
int greenLED = 13;
int lightValue;

//time vars
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -14400; //-25200 for PST, -14400 for EST
const int   daylightOffset_sec = 0;

// Set your access point network credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//alarm set time from webpage
const char* PARAM_INPUT_2 = "value";

bool flag_lightStatus;

unsigned long start_time, end_time, loop_time;

//////////////////////////////////////////////////////////////////////////////
//////////////////////Storing alarm time structure////////////////////////////
//////////////////////////////////////////////////////////////////////////////
struct alarmTime{
  
  int time_min;
  int time_hr;
  char time_ampm;
  bool alarmState;
  bool alarmSet;
  int followerNode;
};

struct alarmTime AT1;


/////////////////Button Setup////////////////////////
struct InputButton {
  int GPIO;
  bool pressed;
};

InputButton ib1 = {22, false};

void IRAM_ATTR buttonISR(){
  ib1.pressed = true;
  alarmOffFunction(flag_lightStatus);
  alarmSetState(AT1.alarmSet);
  ib1.pressed = false;
  Serial.printf("ISR Test\n");
}


////////////////Interrupt Setup////////////////////
hw_timer_t * timer0 = NULL;
hw_timer_t * timer1 = NULL;

portMUX_TYPE timerMux0 = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMux1 = portMUX_INITIALIZER_UNLOCKED;


void IRAM_ATTR onTimer0(){
  // Critical Code here
  portENTER_CRITICAL_ISR(&timerMux0);
  printLocalTime();
  alarmTriggerState(AT1.alarmState);
  portEXIT_CRITICAL_ISR(&timerMux0);
}

void IRAM_ATTR onTimer1(){
  // Critical Code here
  portENTER_CRITICAL_ISR(&timerMux1);
  flag_lightStatus = lightState();
  portEXIT_CRITICAL_ISR(&timerMux1);
}

//////////////////////////////Web Page//////////////////////////////////////
const char index_html[] PROGMEM = R"rawliteral( //start of html code for website
<!DOCTYPE html>
<head>
    <title>ECE554 Alarm</title>
    <link href='https://fonts.googleapis.com/css?family=VT323' rel='stylesheet'>
    <style>
        h2 {
            color: #003333
        }
        .area {
            height: 70px;
            padding: 5px;
            margin: 6px;
        }

        .text-area-class {
            width: 400px;
            font-size: 18px;
        }

        .alarm-text {
            font-family: 'VT323', 'Courier New', monospace;
            font-size: 48px;
            font-weight: bolder;
            color: red;
            width: 200px;
            background: #222222;
            padding: 15px;
            text-align: center;
        }

        .off-button {
            border-radius: 8px;
            font-size: 16px;
            background-color: red;
            color: white;
        }
        .submit-button {
            border-radius: 8px;
            font-size: 16px;

        }
        .debug-button {
            font-size: 14px;
            background-color: #BBBBBB
        }
    </style>
</head>

<body>
    <h2>ECE 554 Distributed Alarm</h2>
    <h3>Bayer, Kobely, & Waterbury</h3>
    <div/>
    <button id='disable-alarm' name='disable-button' value='Turn Off Alarm' type='submit' class='off-button'
            onclick='turnOffAlarm()'>
        Turn Off Alarm
    </button>
    <div/>
    <label>Select a time (HH:MM AM/PM): </label>
    <input type='time' id='time-selector' name='alarm-time' required placeholder='13:30'/>
    <button id='submitButton' name='submit' value='Set Time' type='submit' disabled class='submit-button'
            onclick='setToUserEnteredTime()'>
        Set Time
    </button>
    <div/>

    <textarea id='submit-text' disabled class='area text-area-class'>
        </textarea>
    <textarea id='alarm-time-text' disabled class='area alarm-text'>
        </textarea>
    <div/>

    <div/>
    <hr/>
    <div>
        <table>
            <tr>
                <td>
                    <button id='debug-0600' name='debug-0600-button' type='submit' class='debug-button'
                            onclick='setToHardCodedTime("06:00")'>
                        DEBUG - set to 0600
                    </button>
                </td>
                <td>
                    <button id='debug-1800' name='debug-1800-button' type='submit' class='debug-button'
                            onclick='setToHardCodedTime("18:00")'>
                        DEBUG - set to 1800
                    </button>
                </td>
            </tr>
            <tr>
                <td>
                    <button id='debug-now' name='debug-now-button' type='submit' class='debug-button' onclick='setAlarmNow(1)'>
                        DEBUG - set to NOW + 1m
                    </button>
                </td>
                <td>
                    <button id='debug-now-plus-2' name='debug-nowplus2-button' type='submit' class='debug-button' onclick='setAlarmNow(2)'>
                        DEBUG - set to NOW + 2m
                    </button>
                </td>

            </tr>
        </table>
    </div>
    <script>
        let alarmTime = '00:00'
        let alarmEnabled = false;
        document.getElementById('submitButton').disabled = false;
        document.getElementById('time-selector').value = '12:00';
        document.getElementById('alarm-time-text').textContent = '--:--';

        function turnOffAlarm() {
            printTimeOfFunction('Request to shut off alarm received at');
            alarmEnabled = false;
            document.getElementById('submit-text').textContent = 'Alarm set off'
            document.getElementById('disable-alarm').disabled = true;
            document.getElementById('time-selector').value = null;
            document.getElementById('alarm-time-text').textContent = '--:--';
            // make GET web request to 192.168.4.1/alarmOff
            const url = 'http://192.168.4.1/alarmOff';
            console.debug('Alarm off');
            fetch(url, {method: 'GET'}).then(function (response) {
                if (response.ok) {
                    printTimeOfFunction('Request to shut off alarm completed at');
                }
            });
        }

        function setToUserEnteredTime() {
            printTimeOfFunction('Request to set alarm received at');
            const timeSet = document.getElementById('time-selector').value;
            console.log('You entered: ', timeSet);
            document.getElementById('disable-alarm').disabled = false;
            setAlarmHelper(timeSet);
        }

        function setToHardCodedTime(enteredTime) {
            printTimeOfFunction('Request to set alarm received at');
            document.getElementById('disable-alarm').disabled = false;
            console.log('Setting Alarm to : ', enteredTime);
            setAlarmHelper(enteredTime);
        }

        function setAlarmNow(additionalMinutes) {
            printTimeOfFunction('Request to set alarm received at');
            document.getElementById('disable-alarm').disabled = false;
            let d = new Date();
            d.setTime(d.getTime() + additionalMinutes * 60 * 1000);
            const timeNow = d.toLocaleTimeString('en-US', {hour12: false, hour: '2-digit', minute: '2-digit'});
            console.log('Setting Alarm to: ', timeNow);
            setAlarmHelper(timeNow);
        }

        function setAlarmHelper(alarmTimeParam) {
            alarmTime = alarmTimeParam;
            alarmEnabled = true;
            document.getElementById('submit-text').textContent = 'The time you entered is: ' + alarmTime;
            document.getElementById('alarm-time-text').textContent = alarmTime;
            console.debug('The time in the helper is: ' + alarmTime);
            // make GET web request to 192.168.4.1/setAlarm?value=<time>
            let url = 'http://192.168.4.1/setAlarm?value=' + alarmTime;
            fetch(url, {method: 'GET'}).then(function (response) {
                if (response.ok) {
                    printTimeOfFunction('Request to set alarm successfully completed at');
                }
            });
        }

        function printTimeOfFunction(customString) {
            const d0 = new Date();
            console.log(customString, ' [ISO] ', d0.toISOString());
            console.debug(customString, ' [Local] ', d0.toLocaleString());
        }
    </script>
</body>
</html>
)rawliteral"; //end of html code


void setup() {
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.clear();
  Serial.begin(115200);
  pinMode(buzzer,OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  lcd.print("ECE554 Project");

///////////////////////// Setting WiFi Connection////////////////////////////////////
 
   WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    WiFiManager wm;

    //reset settings - wipe credentials for testing
    //wm.resetSettings();
    bool res;
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }

///////////////////////// Setting the ESP as an access point////////////////////////////////////
    Serial.print("Setting AP (Access Point)…");
    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    //IPAddress local_IP(10, 0, 0, 99);
    Serial.print("AP IP address: ");
    Serial.println(IP);


/////////////////WEBSITE INTERACTION/////////////////////////////////////

    server.on("/setAlarm", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input1 value on <ESP_IP>/setAlarm?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT_2)) {
      inputMessage = request->getParam(PARAM_INPUT_2)->value();
     AT1.time_hr = (inputMessage[0]-48)*10 + inputMessage[1]-48;
     AT1.time_min = (inputMessage[3]-48)*10 + inputMessage[4]-48;
     AT1.time_ampm = inputMessage[5];
     AT1.alarmSet = true;
     
     alarmSetState(AT1.alarmSet);
    }
    else {
      inputMessage = "No message sent";
    }
    //Serial.println(inputMessage);
    Serial.printf("The time is %d:%d%c\n", AT1.time_hr, AT1.time_min, AT1.time_ampm);
    request->send(200, "text/plain", "OK");
    });

    server.on("/alarmOff", HTTP_GET, [] (AsyncWebServerRequest *request) {
      AT1.alarmState = false;
      AT1.alarmSet = false;
      AT1.followerNode = false;
      alarmSetState(AT1.alarmSet);
      request->send(200, "text/plain", "Disabled");
      Serial.println("Alarm Has Been Disabled");
    
    });

//////////////////////////////CLIENT INTERACTION///////////////////////////////////////////////
  //Client checking alarm state
   server.on("/alarmState", HTTP_GET, [](AsyncWebServerRequest *request){
    if (AT1.followerNode == true){
      request->send_P(200, "text/plain", "true"); 
    }
    else{
      request->send_P(200, "text/plain", "false");
    }
    
  });

  //shut off flag from follower node
   server.on("/alarmShutOff", HTTP_GET, [](AsyncWebServerRequest *request){
      
      AT1.followerNode = disabled;
      Serial.println("Follower Has Disabled Its Alarm");
      request->send_P(200, "text/palin", "OK");
  });


    
 /////////////////////////////START SERVER///////////////////////////////////////////////////////
    server.begin();


/////////////////////////////Interrupt Setup//////////////////////////////////////////////
  pinMode(ib1.GPIO, INPUT_PULLUP);
  attachInterrupt(ib1.GPIO, buttonISR, FALLING);


///////////////////////////Configure time////////////////////////////////////////////////////////

    
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

/////////////////////////Setup Interrupts///////////////////////////////////////////////////////
  Serial.println("start timer 1");
  timer1 = timerBegin(1, 80, true);  // timer 1, MWDT clock period = 12.5 ns * TIMGn_Tx_WDT_CLK_PRESCALE -> 12.5 ns * 80 -> 1000 ns = 1 us, countUp
  timerAttachInterrupt(timer1, &onTimer1, true); // edge (not level) triggered 
  timerAlarmWrite(timer1, 2000000, true); // 250000 * 1 us = 250 ms, autoreload true

  Serial.println("start timer 0");
  timer0 = timerBegin(0, 80, true);  // timer 0, MWDT clock period = 12.5 ns * TIMGn_Tx_WDT_CLK_PRESCALE -> 12.5 ns * 80 -> 1000 ns = 1 us, countUp
  timerAttachInterrupt(timer0, &onTimer0, true); // edge (not level) triggered 
  timerAlarmWrite(timer0, 1000000, true); // 2000000 * 1 us = 2 s, autoreload true

  // at least enable the timer alarms
  timerAlarmEnable(timer0); // enable
  timerAlarmEnable(timer1); // enable
  //timerAlarmEnable(timer2); // enable

  alarmSetState(false);
} //End of setup


  
  


void loop() {

  //delay(900);
  start_time = millis();

//  Serial.println("The alarm is: %b", AT1.alarmState);

  //Serial.printf("Loop is %lu\n",millis()-start_time);

}

void printLocalTime(){
  //Time struct info located here:
  //https://www.cs.auckland.ac.nz/references/unix/digital/AQTLTBTE/DOCU_099.HTM
  struct tm timeinfo;
  struct tm tm_hour;

  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %I:%M:%S -- %p");
  lcd.setCursor(0, 1);
  lcd.print(&timeinfo, "%I:%M:%S %p");

  int hours = timeinfo.tm_hour; //note this is in 24 hour time not 12
  int minutes = timeinfo.tm_min; 
  int seconds = timeinfo.tm_sec;
  
    if (AT1.alarmSet){
      if(AT1.time_min == minutes){
              if(AT1.time_hr == hours){
                  AT1.alarmState = true;
                  AT1.followerNode = enabled; 
                 }
          }
      }
}

bool alarmSetState(bool alarmOnFlag)
{
  if (alarmOnFlag == true) //change to at1
  {
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);  
  }

  else
  {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);  
  }
}

bool alarmTriggerState(bool alarmTriggerFlag)
{
  if (alarmTriggerFlag == true){
    digitalWrite(buzzer,HIGH);
  }
  else
  {
    digitalWrite(buzzer,LOW);
  }
}

bool alarmOffFunction(bool lightStateFlag)
{
  if (AT1.alarmState == true){
      if (lightStateFlag)
      {
              AT1.alarmState = false;
              AT1.alarmSet = false;
      }
   }
}
bool lightState()
{
  
  lightValue = analogRead(34); //photoresistor pin 34
  Serial.println("Analog value : ");
  Serial.println(lightValue);
  

  if (lightValue < 1500){ //adjust value based on typical lightvalue
    return false;
  }
  else{
    return true;
  }
}
