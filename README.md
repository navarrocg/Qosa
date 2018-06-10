# Qosa
Mashup library to build MQTT Wemos Mini D1 connected devices

It allows you to create a device that can be connected to wifi and sends info to an MQTT server.
It supports a configuration web and self discovers MQTT server with mDns

See in example how a web connected weather sensor can be created with 50 lines of code

This lib depends on

* https://github.com/esp8266/Arduino
* https://github.com/knolleary/pubsubclient
* https://github.com/tzapu/WiFiManager

#include "Arduino.h"
#include <FS.h> https://github.com/esp8266/Arduino
#include <ESP8266mDNS.h> https://github.com/esp8266/Arduino
#include <WiFiUdp.h> https://github.com/esp8266/Arduino
#include <ArduinoOTA.h> https://github.com/esp8266/Arduino
#include <ESP8266WiFi.h> https://github.com/esp8266/Arduino
#include <PubSubClient.h> https://github.com/knolleary/pubsubclient
#include <DNSServer.h> https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h> https://github.com/esp8266/Arduino
#include <WiFiManager.h> https://github.com/tzapu/WiFiManager
