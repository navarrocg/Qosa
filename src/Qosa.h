/*    This file is part of Qosa library.
*
*    Created by Carlos Navarro, navarrocg@github nov 25th, 2017
*
*    Qosa is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Lesser General Public License as
*    published by the Free Software Foundation, either version 3 of
*    the License, or (at your option) any later version.
*
*    Qosa is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with Qosa.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef QOSA_LIB_H
#define QOSA_LIB_H

#include "Arduino.h"
#include <FS.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <string>

/*
 * chandlers for webserver and mqtt callbacks
 */
static void c_mqtt_callback(char* topic, byte* payload, unsigned int length);
static void c_handle_not_found();
static void c_handle_root();
static void c_handle_force_config();

/**
 * @brief The Qosa class is an objet in the nternet of things capable of dsicovering mqtt servers.
 * It has embedded OTA, web server support and an alive mechanism with the server
 */
class Qosa
{
public:

    /**
     * @brief gimmeAQosa factory to construct the  Qosa. Just a Qosa is valid per sketch
     * @param deviceName Name of the device, it will be used for OTA, mdns, and mqtt
     * @param mqttPrefix used to prefix the alive, ping and pong mqtt messages
     * @param otaPassword password used to authorize OTA upgrades
     * @return  a Qosa!
     */
    static Qosa * gimmeAQosa(String deviceName, String mqttPrefix, String otaPassword);

    /**
     * @brief init call it in sketch setup after creating the Qosa
     */
    void init();

    /**
     * @brief processLoop call it in the process loop.
     */
    void processLoop();

    /**
     * @brief getLoopNumber
     * @return the loop number of the Qosa
     */
    long getLoopNumber();

    /**
     * @brief getMqttClient
     * @return get the mqtt client to subscribe to other topics or publish new
     */
    PubSubClient *getMqttClient();

private:

    void rebootQosa(const char *reason );

    void publishRebootInfo();

    bool forcedConfigMode();

    void mountFileSystem();

    void configureWifiManager();

    void getTimeStampString(long milliseconds, char buffer[10]);

    void handleRoot();


    void handleForceConfig();

    void handleNotFound();

    void startWebServer();

    void mQTTcallback(char* topic, byte* payload, unsigned int length);

    bool connectMQTT();

    void generateMQTTTopics();

    Qosa(String deviceName, String mqttPrefix, String otaPassword);

private:

    long loopNumber_;           // loop number to send alive
    long noMqttLoops_ ;          // loop number without mqtt

    WiFiClient espClient_;
    PubSubClient mqttClient_;
    ESP8266WebServer webServer_;
    IPAddress mqttServer_;
    int mqttPort_;
    String deviceName_;
    String mqttPrefix_;
    String otaPassword_;
    bool pingRequested_;
    long pingCounter_;

    std::string outInfoTopic_;
    std::string outPongTopic_;
    std::string outAliveTopic_;
    std::string inPingTopic_;

    /**
     * @brief numLoopsWithoutMqttTimeout_ number of loops that Qosa will wait for mqtt before trying a reboot
     */
    long numLoopsWithoutMqttTimeout_;

    /**
     * @brief singleton_ just one cosa per sketch
     */
    static Qosa * singleton_;

    /*
     * chandlers for webserver and mqtt callbacks
     */
    friend void c_mqtt_callback(char* topic, byte* payload, unsigned int length);
    friend void c_handle_not_found();
    friend void c_handle_root();
    friend void c_handle_force_config();

};
#endif
