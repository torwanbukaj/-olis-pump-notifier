// This is an Arduino code for ESP8266 which uses Webhooks
// to trigger notifications via IFTTT.com
//
// By torwanbukaj 4th March 2023
// Diagnostics messages are available via Serial COM configured to 115200bps.
//
// Description:
// The code triggers a Webhooks message in the context of a defined Webhooks "EVENT" whenever:
// - it connects to a local WiFi spot,
// - boots up after getting powered on or reset,
// - a state of D5 (default) input of Wemos D1 Mini board (or any similar) changes
//   (preconditoned by TON and TOF timers).
//
// Required libraries:
// - ESP8266WiFi.h
//
// Wemos D1 Mini built-in LED lights up when trying to connect to WiFi.


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "secrets.h"

#define MONITORED_INPUT D5

// ------ GLOBAL VARIABLES ------

// LED state
int led_state = LOW;
unsigned long current_led_millis = 0;  // to store current millis() value for LED blinking function
unsigned long previous_led_millis = 0;  // to store the last time LED state was updated
const unsigned long led_interval = 100; // blinking interval when in approved ALARM state [ms],
                                        // compare with delay() which slows down the main loop

// Webhooks
String _webhooks_event_ = _WEBHOOKS_EVENT_;
String _webhooks_key_ = _WEBHOOKS_KEY_;
int response;

// Input state change recognition
bool last_state;
bool new_state;

// TON and TOF timers
unsigned long ton_threshold = 1800;
unsigned long tof_threshold = 1600;
unsigned long ton_start_marker;
unsigned long tof_start_marker;
bool TON_running = false;
bool TOF_running = false;
bool approved_alarm_state = false;

// WiFi related
const char* deviceName = "Pump_Notifier";

// OTA related
const char* ota_host_name = "ota-pump-notifier";

// ------ SETUP ------

void setup() {

  // Initialization of the built-in LED output
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialization of the serial port
  Serial.begin(115200);
  
  // Initialization of the WiFi connection
  connect_to_wifi();

  // OTA setup
  ota_setup();


  // Initialization of the MONITORED_INPUT
  pinMode(MONITORED_INPUT, INPUT_PULLUP);
  last_state = digitalRead(MONITORED_INPUT);
  new_state = last_state;

  // Send "power-on" event trigger
  delay(1000);
  response = webhookTrigger("Notifier_powered_and_running!");
  if(response == 200) Serial.println("Webhook OK for Notifier_powered_and_running!");
  else Serial.println("Failed");

}

// ------ MAIN LOOP ------

void loop() {
  
  // Handling OTA
    ArduinoOTA.handle();

  // Check for a new input state
  new_state = digitalRead(MONITORED_INPUT);

  // Detection of an input change (rising or falling edge)
  if (new_state != last_state) {
      Serial.print("New input state: ");

      // Detection of the rising edge before HIGH state is approved
      if (new_state == true && approved_alarm_state == false) {
        Serial.println("HIGH (circuit got OPEN) and not in the approved ALARM state");
        TOF_running = false; 
        TON_running = true;
        ton_start_marker = millis();        
      }

      // Detection of the rising edge after HIGH state is approved
      if (new_state == true && approved_alarm_state == true) {
        Serial.println("HIGH (circuit got OPEN) when still in approved ALARM state");
        TOF_running = false; 
        TON_running = false;
      }

      // Detection of the falling edge after HIGH state is approved
      // (it triggers TOF)
      if (new_state == false && approved_alarm_state == true) {
        Serial.println("LOW (circuit got CLOSED) when in approved ALARM state");
        TOF_running = true; 
        TON_running = false;      
        tof_start_marker = millis();
      }   

      // Detection of the falling edge before HIGH state is approved
      // (it does not trigger TOF)
      if (new_state == false && approved_alarm_state == false) {
        Serial.println("LOW (circuit got CLOSED) when not in approved ALARM state");
        TOF_running = false; 
        TON_running = false;      
      }       

  last_state = new_state;
  }

  // TON implementation
  if (TON_running == true) {
    Serial.print("TON running for:"); Serial.print(millis()-ton_start_marker); Serial.println(" [ms]");
    if(millis()-ton_start_marker >= ton_threshold) {
      Serial.println("Entering approved ALARM state.");
      TON_running = false;
      approved_alarm_state = true;
      response = webhookTrigger("Pump-relay_circuit_OPEN!");
      if(response == 200) Serial.println("Webhooks: OK for Pump-relay_circuit_OPEN!");
      else Serial.println("Failed");
    }

  } 

  // TOF implementation
  if (TOF_running == true) {
    Serial.print("TOF running for:"); Serial.print(millis()-tof_start_marker); Serial.println(" [ms]");
    if(millis()-tof_start_marker >= tof_threshold) {
      Serial.println("Leaving approved ALARM state.");
      TOF_running = false;
      approved_alarm_state = false;
      response = webhookTrigger("Pump-relay_circuit_CLOSED_OK!");
      if(response == 200) Serial.println("Webhooks: OK for Pump-relay_circuit_CLOSED_OK!");
      else Serial.println("Failed"); 
    }
  } 

 // Manage in-built LED blinking fast when in approved ALARM state
 toggle_inbuilt_led_fast();

  // Check WiFi connection status
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lost connection to WiFi!");
    // Try to reconnect to WiFi
    connect_to_wifi();    
  }
  
  delay(100); //slowing down the infinite main loop

} // main loop end

// ------ HELPING FUNCTIONS ------

void connect_to_wifi() {

  digitalWrite(LED_BUILTIN, LOW);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  // Connect to WiFi
  Serial.println();
  Serial.print("Device name: ");
  Serial.println(deviceName);
  WiFi.hostname(deviceName); 

  Serial.print("Connecting to: ");
  Serial.println(_SSID_);
  WiFi.begin(_SSID_, _PASSWORD_);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("*");
  }
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("");
  Serial.println("WiFi Connected");

  // Print the IP address
  Serial.print("IP assigned by DHCP: ");
  Serial.println(WiFi.localIP());
  // Print the MAC address
  Serial.print("ESP Board MAC Address: ");
  Serial.println(WiFi.macAddress());
  // Print the Gateway IP address
  //Serial.print("Gateway IP address: ");
  //Serial.println(WiFi.gatewayIP());

  // Print current RSSI
  Serial.print("Current RSSI: ");
  Serial.println(WiFi.RSSI());

  response = webhookTrigger("Pump-relay_connected_to_WiFi!");
  if(response == 200) Serial.println("Webhook OK for Pump-relay_connected_to_WiFi!");
  else Serial.println("Failed");
}

void toggle_inbuilt_led_fast() {

  if (approved_alarm_state == true) {
   current_led_millis = millis();
   if (current_led_millis-previous_led_millis >= led_interval) {
    previous_led_millis = current_led_millis;
    if (led_state == LOW) {led_state = HIGH;} else {led_state = LOW;} // toggle LED state
   }    
  }

  if (approved_alarm_state == false) {
    led_state = HIGH;
  }

 digitalWrite(LED_BUILTIN, led_state); // write new LED state to the output  
 
}

void ota_setup() {

  ArduinoOTA.setHostname(ota_host_name);
  ArduinoOTA.setPassword(_OTA_PASSWORD_);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

int webhookTrigger(String value){
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://maker.ifttt.com/trigger/" + _webhooks_event_ + "/with/key/" + _webhooks_key_ + "?value1="+value);
  int httpCode = http.GET();
  http.end();
  return httpCode;
}
