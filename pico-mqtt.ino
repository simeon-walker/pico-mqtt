#include "arduino_secrets.h"
#include "local_config.h"

#include <MQTT.h>
#include <MQTTClient.h>
#include <WiFi.h>
#include <Arduino_JSON.h>

#define DECODE_NEC
#define DECODE_SAMSUNG
#define DECODE_SONY
#define DECODE_RC5
#define DECODE_RC6
#define DECODE_RC6A
#define EXCLUDE_UNIVERSAL_PROTOCOLS
#define EXCLUDE_EXOTIC_PROTOCOLS
// defines MUST come before include
#include <IRremote.hpp>

const int irRxPin = IR_RX_PIN;
const decode_type_t relayIrProtocol = NEC;
const uint16_t relayIrAddr = 0x00;
const uint16_t relayIrOnCmd = 0x40;
const uint16_t relayIrOffCmd = 0x19;

const int relayPin = RELAY_PIN;

const char* ssid = SECRET_SSID;
const char* password = SECRET_WIFI_PASS;
const char* mqttServer = MQTT_SERVER;
const char* mqttClientId = MQTT_CLIENTID;
const char* mqttUser = MQTT_USER;

const char* mqttTopic = MQTT_PREFIX;
const char* topicRelayPin = TOPIC_RELAY_PIN;
const char* topicRelayControl = TOPIC_RELAY_CONTROL;
const char* topicRelayStatus = TOPIC_RELAY_STATUS;
const char* topicIpAddrStatus = TOPIC_IP_ADDRESS;
const char* topicUptimeStatus = TOPIC_UPTIME;
const char* topicIrRx = TOPIC_IR_RX;

WiFiClient net;
MQTTClient mqttClient;
IPAddress ipAddr;

unsigned long lastMillis = 0;
unsigned short minutes = 0;

void mqttConnect() {
  Serial1.print("MQTT connecting to ");
  Serial1.print(mqttServer);
  Serial1.print("...");
  while (!mqttClient.connect(mqttClientId, mqttUser, SECRET_MQTT_PASS)) {
    Serial1.print(".");
    delay(1000);
  }
  Serial1.println("connected");
  mqttClient.publish(topicRelayPin, String(relayPin));
  mqttClient.publish(topicIpAddrStatus, ipAddr.toString());

  mqttClient.subscribe(mqttTopic);
}

void mqttMsgReceived(String& topic, String& payload) {
  Serial1.println(topic + ": " + payload);
  if (topic == topicRelayControl) {
    if (payload == "ON") {
      relayOn();
    } else if (payload == "OFF") {
      relayOff();
    }
  }
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial1.print("0");
    }
    Serial1.print(mac[i], HEX);
    if (i > 0) {
      Serial1.print(":");
    }
  }
  Serial1.println();
}

void printIpConfig() {
  ipAddr = WiFi.localIP();
  Serial1.print("IP address: ");
  Serial1.println(ipAddr);

  // Serial1.print("Subnet mask: ");
  // Serial1.println((IPAddress)WiFi.subnetMask());

  // Serial1.print("Gateway IP: ");
  // Serial1.println((IPAddress)WiFi.gatewayIP());

  // print your MAC address:
  // byte mac[6];
  // WiFi.macAddress(mac);
  // Serial1.print("MAC address: ");
  // printMacAddress(mac);
}

void wifiConnect() {
  // Begin WiFi connection
  Serial1.println();
  Serial1.print("WiFi connecting to ");
  Serial1.print(ssid);
  Serial1.print("..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial1.print(".");
    WiFi.begin(ssid, password);
    delay(1000);
  }
  Serial1.println("connected");
  printIpConfig();
}

void setup() {
  // Serial1 is the first UART, not USB
  Serial1.begin(115200);
  delay(100);
  Serial1.println(F("pico-mqtt starting"));
  // Serial1.println(F("IRremote library version " VERSION_IRREMOTE));

  Serial1.print(F("Relay pin: "));
  Serial1.println(RELAY_PIN);
  pinMode(relayPin, OUTPUT);

  Serial1.print(F("IR RX pin: "));
  Serial1.println(irRxPin);
  IrReceiver.begin(irRxPin, ENABLE_LED_FEEDBACK, USE_DEFAULT_FEEDBACK_LED_PIN);

  Serial1.print(F("IR protocols: "));
  printActiveIRProtocols(&Serial1);

  wifiConnect();

  mqttClient.begin(mqttServer, net);
  mqttClient.onMessage(mqttMsgReceived);
  mqttConnect();
}

void relayOn() {
  // Turn on relay
  Serial1.println(F("Relay On."));
  digitalWrite(relayPin, HIGH);
  mqttClient.publish(topicRelayStatus, "ON");
}

void relayOff() {
  // Turn off relay
  Serial1.println(F("Relay Off."));
  digitalWrite(relayPin, LOW);
  mqttClient.publish(topicRelayStatus, "OFF");
}

void publishIrData() {
  JSONVar irPayload;
  irPayload["protocol"] = getProtocolString(IrReceiver.decodedIRData.protocol);
  irPayload["address"] = IrReceiver.decodedIRData.address;
  irPayload["command"] = IrReceiver.decodedIRData.command;

  mqttClient.publish(topicIrRx, JSON.stringify(irPayload));
}

void loop() {
  mqttClient.loop();
  if (!mqttClient.connected()) {
    mqttConnect();
  }

  if (IrReceiver.decode()) {
    IrReceiver.resume();  // Enable receiving of the next value

    // check for specific relay protocol/address/command
    if (IrReceiver.decodedIRData.protocol == relayIrProtocol) {
      if (IrReceiver.decodedIRData.address == relayIrAddr ) {
        if (IrReceiver.decodedIRData.command == relayIrOnCmd) {
          relayOn();
        } else if (IrReceiver.decodedIRData.command == relayIrOffCmd) {
          relayOff();
        }
      }
    }
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      // for known protocols publish to mqtt
      publishIrData();
    } else {
      // for unknown just print to serial
      // IrReceiver.printIRResultMinimal(&Serial1);
      IrReceiver.printIRResultShort(&Serial1);
    }
  }
  // publish a message roughly every minute.
  if (millis() - lastMillis > 60000) {
    lastMillis = millis();
    minutes = lastMillis / 60000;
    mqttClient.publish(topicUptimeStatus, String(minutes));
  }

  delay(100);
}
