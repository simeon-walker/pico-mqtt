#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include "arduino_secrets.h"
#include "local_config.h"

#include <MQTT.h>
#include <MQTTClient.h>
#include <WiFi.h>

// IRremote defines MUST come before include
#define EXCLUDE_EXOTIC_PROTOCOLS
#define IR_SEND_PIN 27
#define IR_RECEIVE_PIN 28
#include <IRremote.hpp>

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
const char* mqttPrefix = MQTT_PREFIX;

const char* topicRelay = TOPIC_RELAY;
const char* topicStatus = TOPIC_STATUS;
const char* topicIrSend = TOPIC_IR_SEND;
const char* topicIrReceive = TOPIC_IR_RECEIVE;

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

  JsonDocument status;
  status["username"] = mqttUser;
  status["ip_address"] = ipAddr.toString();
  status["relay_pin"] = RELAY_PIN;
  status["ir_send_pin"] = IR_SEND_PIN;
  status["ir_receive_pin"] = IR_RECEIVE_PIN;
  String payload;
  serializeJson(status, payload);
  mqttClient.publish(topicStatus, payload);

  mqttClient.subscribe(mqttPrefix);
}

void mqttMsgReceived(String& topic, String& payload) {
  Serial1.println(topic + ": " + payload);

  if (topic == topicRelay) {
    if (payload == "on") {
      relayOn();
    } else if (payload == "off") {
      relayOff();
    }
  } else if (topic == topicIrSend) {
    sendIr(payload);
  }
}

void sendIr(String payload) {
  JsonDocument irMessage;
  DeserializationError error = deserializeJson(irMessage, payload);

  if (error) {
    Serial1.print("deserializeJson() failed: ");
    Serial1.println(error.c_str());
    return;
  }

  const char* protocol = irMessage["protocol"];   // "Samsung"
  const uint16_t address = irMessage["address"];  // 7
  const uint16_t command = irMessage["command"];  // 15
  const uint8_t repeats = irMessage["repeats"];

  if (protocol == nullptr) {
    Serial.println("irMessage payload missing protocol");
    return;
  }

  // Serial1.print("IR sending protocol ");
  // Serial1.print(protocol);
  // Serial1.print(" address ");
  // Serial1.print(address);
  // Serial1.print(" command ");
  // Serial1.print(command);
  // Serial1.print(" repeats ");
  // Serial1.println(repeats);

  IrReceiver.disableIRIn();

  if (strcmp(protocol, "Samsung") == 0) {
    IrSender.sendSamsung(address, command, repeats);

  } else if (strcmp(protocol, "NEC") == 0) {
    IrSender.sendNEC(address, command, repeats);

  } else if (strcmp(protocol, "RC5") == 0) {
    IrSender.sendRC5(address, command, repeats);

  } else if (strcmp(protocol, "RC6") == 0) {
    IrSender.sendRC6(address, command, repeats);

  } else if (strcmp(protocol, "RC6A") == 0) {
    IrSender.sendRC6A(address, command, repeats, 0);

  } else {
    Serial1.print(protocol);
    Serial1.println(" not handled");
  }
  delay(10);
  IrReceiver.enableIRIn();
}

void wifiConnect() {
  // Begin WiFi connection
  Serial1.println();
  Serial1.print("WiFi connecting to ");
  Serial1.print(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial1.print(".");
    WiFi.begin(ssid, password);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
  Serial1.println("connected");
  Serial1.print("IP address: ");
  ipAddr = WiFi.localIP();
  Serial1.println(ipAddr.toString());
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Serial1 is the first UART, not USB
  Serial1.begin(115200);
  delay(100);
  Serial1.println("pico-mqtt starting");

  Serial1.print("Relay pin: ");
  Serial1.println(RELAY_PIN);
  pinMode(RELAY_PIN, OUTPUT);

  IrSender.begin();
  Serial1.print("IR Send pin: ");
  Serial1.println(IR_SEND_PIN);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial1.print("IR Receive pin: ");
  Serial1.println(IR_RECEIVE_PIN);
  Serial1.print("IR protocols: ");
  printActiveIRProtocols(&Serial1);

  wifiConnect();

  mqttClient.begin(mqttServer, net);
  mqttClient.onMessage(mqttMsgReceived);
  mqttConnect();
}

void relayOn() {
  digitalWrite(RELAY_PIN, HIGH);
  mqttClient.publish(topicStatus, "{\"relay\": \"on\"}");
}

void relayOff() {
  digitalWrite(RELAY_PIN, LOW);
  mqttClient.publish(topicStatus, "{\"relay\": \"off\"}");
}

void publishIrData() {
  JsonDocument irPayload;
  irPayload["protocol"] = getProtocolString(IrReceiver.decodedIRData.protocol);
  irPayload["address"] = IrReceiver.decodedIRData.address;
  irPayload["command"] = IrReceiver.decodedIRData.command;

  String payload;
  serializeJson(irPayload, payload);
  mqttClient.publish(topicIrReceive, payload);
}

void publishUptime(unsigned short mins) {
  JsonDocument status;
  status["uptime"] = mins;

  String statusPayload;
  serializeJson(status, statusPayload);
  mqttClient.publish(topicStatus, statusPayload);
}

void loop() {
  mqttClient.loop();
  if (!mqttClient.connected()) {
    mqttConnect();
  }

  if (IrReceiver.decode()) {

    // check for specific relay protocol/address/command
    if (IrReceiver.decodedIRData.protocol == relayIrProtocol) {
      if (IrReceiver.decodedIRData.address == relayIrAddr) {
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
      IrReceiver.printIRResultMinimal(&Serial1);
      Serial1.println();
      // IrReceiver.printIRSendUsage(&Serial1);
    } else {
      // for unknown just print to serial
      IrReceiver.printIRResultRawFormatted(&Serial1);
    }

    IrReceiver.resume();  // Enable receiving of the next value
  }

  // publish a message roughly every minute.
  if (millis() - lastMillis > 60000) {
    lastMillis = millis();
    minutes = lastMillis / 60000;
    publishUptime(minutes);
  }

  delay(100);
}
