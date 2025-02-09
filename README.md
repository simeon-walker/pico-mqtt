# pico-mqtt

Arduino project for interfacing between MQTT and IR sending/receiving, and 5v triggered devices.

Uses:

- <https://github.com/Arduino-IRremote/Arduino-IRremote>
- <https://github.com/256dpi/arduino-mqtt>
- <https://arduinojson.org/>

Requires `arduino_secrets.h` with your values:

```C
#define SECRET_SSID "wifi ssid"
#define SECRET_WIFI_PASS "wifi password"
#define SECRET_MQTT_PASS "your mqtt password"
#define MQTT_SERVER "your mqtt server address"
#define MQTT_USER "your mqtt user"
```

Tested on the Pi Pico W and Pico 2 W but should work on other hardware.

