#include<ESP8266WiFi.h>
#include"PubSubClient.h" //Version 2.8
#include<ArduinoOTA.h>
#include<Wire.h>
#include"Adafruit_MCP23017.h"

#define _WLAN_HOST "WemosSecurityPub"
#define _WLAN_SSID "MySSID"
#define _WLAN_PASS "MyPasswrod"

#define _MQTT_BROKER "MyMQTTIP"
#define _MQTT_PORT 1883
#define _MQTT_USER "MyMQTTUser"
#define _MQTT_PASS "MyMQTTPassword"

#define DEBOUNCE_TIME       0.3
#define SAMPLE_FREQUENCY    10
#define MAX_DEBOUNCE        (DEBOUNCE_TIME * SAMPLE_FREQUENCY)

const char* ssid = _WLAN_SSID;
const char* password = _WLAN_PASS;

WiFiClient wlanClient;
PubSubClient mqttClient(wlanClient);
Adafruit_MCP23017 mcp;

//D0 = pin 16
//D1 = pin 5
//D2 = pin 4
//D3 = pin 0
//D4 = pin 2
//D5 = pin 14
//D6 = pin 12
//D7 = pin 13
//D8 = pin 15
//TX = pin 1
//RX = pin 3

long lastMsg = 0;
char msg[50];
int buttonState[9]= {-1, -1, -1, -1, -1, -1, -1, -1, -1};
int integratorButtonState[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int previousButtonState[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
//int pins[6] = {3, 1, 5, 4, 12, 13};
const int onboardLEDPin = 2;
unsigned long ledBlinkTimer;
unsigned long sampleTimer;
int onboardLEDState = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname(_WLAN_HOST);
  WiFi.begin(ssid, password);

  while (WiFi.status()!= WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("SecurityWemosPub", _MQTT_USER, _MQTT_PASS, "security/LWT", 1, 1, "Disconnected")) {
      Serial.println("Connected");
      mqttClient.publish("security/LWT", "Connected", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(", trying again in 5 seconds");
      delay(5000);
    } 
  }
  //force an update:
  for(int pin = 0; pin <= 8; pin++) {
    publishState(pin);
  }
}

void setup_OTA()
{
    // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setup() {
  mcp.begin();
  
  for (int pin=0; pin<=8;pin++){
    mcp.pinMode(pin, INPUT);
    mcp.pullUp(pin, HIGH);
  }
  pinMode(onboardLEDPin, OUTPUT);
  Serial.begin(9600);
  setup_wifi();
  setup_OTA();
  mqttClient.setServer(_MQTT_BROKER, _MQTT_PORT);
  ledBlinkTimer = millis();
  sampleTimer = ledBlinkTimer;
}

void loop() {

  int rc = -1;
  unsigned long currentMillis = millis();
  int input;

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  ArduinoOTA.handle();

  if ((currentMillis - sampleTimer) > 100UL) {
    for (int pin=0; pin<=8; pin++) {    
      //buttonState[pin] = mcp.digitalRead(pin); //before integrator debounce
      //begin debounce
      input = mcp.digitalRead(pin); //for integrator debounce
      if (input == 0) {
        if (integratorButtonState[pin] > 0) {
          integratorButtonState[pin]--;
        }
      }
      else if (integratorButtonState[pin] < MAX_DEBOUNCE) {
        integratorButtonState[pin]++;
      }

      if (integratorButtonState[pin] == 0) {
        buttonState[pin] = 0;
      }
      else if (integratorButtonState[pin] >= MAX_DEBOUNCE) {
        buttonState[pin] = 1;
        integratorButtonState[pin] = MAX_DEBOUNCE;
      }
      //end debounce
      if (buttonState[pin] != previousButtonState[pin]) {
        previousButtonState[pin] = buttonState[pin];
        publishState(pin);
      }
    }
    sampleTimer = currentMillis;
  }
  
  if ((currentMillis - ledBlinkTimer) > 2000UL) {
    ledBlinkTimer = currentMillis;
    if (onboardLEDState == 0) {
      digitalWrite(onboardLEDPin, HIGH);
      onboardLEDState = 1;
    } else {
      digitalWrite(onboardLEDPin, LOW);
      onboardLEDState = 0;
    }
  }
}

String zones[9] = {"Front Door", "Garage Door", "Deck Door","Basement Door", "Master", "Family & Kitchen", "Dining", "Basement", "Inside Basement Door"};

void publishState(int pin)
{
  int rc = -1;
  String zone = "security/zone" + String(pin + 1);;
  String message = "{\"value\":" + String(buttonState[pin]) + ", \"zoneName\":\"" + zones[pin] + "\"}";

  rc = mqttClient.publish(zone.c_str(), message.c_str(), true);
  Serial.println( message.c_str() );

}
