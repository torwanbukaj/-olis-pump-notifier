// This is an Arduino code for ESP8266 which uses Webhooks
// to trigger notifications via IFTTT.com
//
// By joefernwright 30th May 2022
// Diagnostics messages are available via Serial COM configured to 115200bps.
//
// Description:
// The code triggers multiple messages in the context of a defined Webhooks "EVENT" whenever:
// - it connects to a local WiFi spot,
// - boots up after getting powered on or reset,
// - a state of D5 input of Wemos D1 Mini board (or any similar) changes.
//
// Wemos D1 Mini built-in LED lights up when trying to connect to WiFi.
// Yoo will need "ESP8266 Webhooks" library to make this program work.

#include <ESP8266Webhook.h>
#include <ESP8266WiFi.h>

#define _SSID_ "your_SSID"          // WiFi SSID
#define _PASSWORD_ "your_password"   // WiFi Password
#define KEY "your_webhooks_key" // Webhooks Key
#define EVENT "pump-event"           // Webhooks Event Name

// Global variables
Webhook webhook(KEY, EVENT);
bool last_state;
bool new_state;
int response;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(115200);
  
  connect_to_wifi();

  // Initialization of the D5 input
  pinMode(D5, INPUT);
  last_state = digitalRead(D5);
  new_state = last_state;

  // Send "power-on" event trigger
  delay(1000);
  response = webhook.trigger("Notifier_powered_and_running!");
  if(response == 200) Serial.println("OK");
  else Serial.println("Failed");
}

void loop() {
  
  // Check for a new input state
  new_state = digitalRead(D5);
  if (new_state != last_state) {
      Serial.print("New input state: ");
      if (new_state == true) {
        Serial.println("HIGH (circuit OPEN)");
        response = webhook.trigger("Pump-relay_circuit_OPEN!");
      }
      else {
        Serial.println("LOW (circuit CLOSED)");  
        response = webhook.trigger("Pump-relay_circuit_CLOSED_OK!"); 
      }
           
  last_state = new_state;

  delay(5000); // prevents too frequent messaging after a state change
    
  if(response == 200) Serial.println("OK");
  else Serial.println("Failed");
  }

  // Check WiFi connection status
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Lost connection to WiFi!");
    // Try to reconnect to WiFi
    connect_to_wifi();    
  }
  
  delay(50); //slowing down the infinite main loop
}


void connect_to_wifi()  {

  digitalWrite(LED_BUILTIN, LOW);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  // Connect to WiFi
  Serial.println();
  Serial.print("Connecting to: ");
  Serial.println(_SSID_);
  WiFi.begin(_SSID_, _PASSWORD_);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("-");
  }
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.println("");
  Serial.println("WiFi Connected");

  // Print the IP address
  Serial.print("IP assigned by DHCP: ");
  Serial.println(WiFi.localIP());

  response = webhook.trigger("Pump-relay_connected_to_WiFi!");
  if(response == 200) Serial.println("OK");
  else Serial.println("Failed");
}
