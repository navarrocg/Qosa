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
#include "Qosa.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// Public
//
////////////////////////////////////////////////////////////////////////////////////////


Qosa * Qosa::gimmeAQosa(String deviceName, String mqttPrefix, String otaPassword)
{
    if ( singleton_ == NULL )
    {
        singleton_=  new Qosa(deviceName, mqttPrefix,otaPassword);
        return singleton_;
    }
    return singleton_;
}


void Qosa::init()
{
    Serial.begin(115200);

    configureWifiManager();

    Serial.print ( "IP address: " );
    Serial.println ( WiFi.localIP() );

    MDNS.begin(deviceName_.c_str());

    generateMQTTTopics();

    startWebServer();

    if ( ! connectMQTT() )
    {
        Serial.println("Could not connect MQTT");
    }

    // OTA Code
    ArduinoOTA.setPassword((const char *)otaPassword_.c_str());
    ArduinoOTA.setHostname((const char *) deviceName_.c_str());

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

void Qosa::processLoop()
{
    char msg[100];
    loopNumber_++;

    ArduinoOTA.handle();

    webServer_.handleClient();

    if ( ! mqttClient_.connected() )
    {
        Serial.println("Connection lost, reconnecting");
        if ( ! connectMQTT() )
        {
            ++noMqttLoops_;

            if ( noMqttLoops_ % 60  == 0 )
            {
                Serial.println("Trying to reconnect wifi by rebooting");
                String mess="Rebooted due to mqtt lost ";
                mess += noMqttLoops_;
                mess += " cycles. Total cycles ";
                mess += loopNumber_;
                rebootQosa(mess.c_str());            }

            Serial.print("Skipping loop ");
            Serial.println(noMqttLoops_);
            return;
        }
        else
        {
            Serial.println("Connection recovered");
        }
    }


    mqttClient_.loop();

    if ( loopNumber_ % 100  == 0 )
    {
        getTimeStampString(millis(), msg);
        mqttClient_.publish(outAliveTopic_.c_str(), msg);
    }

    if ( pingRequested_ )
    {
        ++pingCounter_;
        snprintf (msg, 99, "Hello #%ld, %s mqtt:%s", pingCounter_, WiFi.localIP().toString().c_str(), mqttServer_.toString().c_str() );
        Serial.print("Publish message [pong]: ");
        Serial.println(msg);
        mqttClient_.publish(outPongTopic_.c_str(), msg);
        pingRequested_ = false;
    }
}

long Qosa::getLoopNumber()
{
    return loopNumber_;
}

PubSubClient * Qosa::getMqttClient()
{
    return & mqttClient_;
}
////////////////////////////////////////////////////////////////////////////////////////
//
// Friends
//
////////////////////////////////////////////////////////////////////////////////////////

static void c_mqtt_callback(char* topic, byte* payload, unsigned int length)
{
    if ( Qosa::singleton_ )
    {
        Qosa::singleton_->mQTTcallback(topic, payload, length);
    }
}

static void c_handle_not_found()
{
    if ( Qosa::singleton_ )
    {
        Qosa::singleton_->handleNotFound();
    }
}

static void c_handle_root()
{
    if ( Qosa::singleton_ )
    {
        Qosa::singleton_->handleRoot();
    }
}

static void c_handle_force_config()
{
    if ( Qosa::singleton_ )
    {
        Qosa::singleton_->handleForceConfig();
    }
}


////////////////////////////////////////////////////////////////////////////////////////
//
// Private
//
////////////////////////////////////////////////////////////////////////////////////////

Qosa * Qosa::singleton_ = NULL;

Qosa::Qosa(String deviceName, String mqttPrefix, String otaPassword):loopNumber_(0), noMqttLoops_(0),
    espClient_(),mqttClient_(espClient_),webServer_(80),mqttServer_(), mqttPort_(0),
    deviceName_(deviceName),mqttPrefix_(mqttPrefix), otaPassword_(otaPassword),
    pingRequested_(false), pingCounter_(0),
    outInfoTopic_("info"), outPongTopic_("pong"),outAliveTopic_("alive"),inPingTopic_("ping"),
    numLoopsWithoutMqttTimeout_(20)
{
}

void Qosa::rebootQosa( const char * reason )
{
    File touch = SPIFFS.open("/reboot.info", "w");
    if (!touch)
    {
        Serial.println("failed to create touch file");
    }
    touch.println(reason);
    touch.close();
    ESP.restart();
}

void Qosa::publishRebootInfo()
{
    if (SPIFFS.exists("/reboot.info"))
    {
        File file = SPIFFS.open("/reboot.info", "r");
        file.setTimeout(20);
        String s = file.readString();

        mqttClient_.publish(outInfoTopic_.c_str(), s.c_str());

        file.close();
        SPIFFS.remove("/reboot.info");
    }
}


////////////////////////////////////////////////////////////////////////////////////////
//
//  Wifi manager related code
//
////////////////////////////////////////////////////////////////////////////////////////

bool Qosa::forcedConfigMode()
{
    if (SPIFFS.exists("/force.config"))
    {
        SPIFFS.remove("/force.config");

        return true;
    }

    return false;
}

void Qosa::mountFileSystem()
{
    if (SPIFFS.begin())
    {
        Serial.println("mounted file system");
    }
    else
    {
        Serial.println("failed to mount FS");
    }
}


void Qosa::configureWifiManager()
{
    String ssid = deviceName_ + String(ESP.getChipId());

    //Local intialization. Once its business is done, there is no need to keep it around
    //
    WiFiManager wifiManager;

    wifiManager.setConnectTimeout(60);
    //wifiManager.setConfigPortalTimeout(180);

    // mount filesystem
    //
    mountFileSystem();

    if ( ! forcedConfigMode() )
    {
        if ( ! wifiManager.autoConnect(ssid.c_str() ) )
        {
            Serial.println("failed to connect and hit timeout");
            delay(3000);

            //WiFi.mode(WIFI_STA);

            //reset and try again
            //
            rebootQosa("Rebooted due to autoconnect timeout");
        }
    }
    else
    {
        if ( !wifiManager.startConfigPortal(ssid.c_str(), NULL) )
        {
            Serial.println("failed to connect and hit timeout");
            delay(3000);

            //WiFi.mode(WIFI_STA);

            //reset and try again
            //
            rebootQosa("Rebooted due to startConfigPortal timeout");
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////
//
//  Run time web related code
//
////////////////////////////////////////////////////////////////////////////////////////

void Qosa::getTimeStampString(long milliseconds, char buffer[10])
{
    int sec = milliseconds / 1000;
    int min = sec / 60;
    int hr = min / 60;
    snprintf(buffer, 9, "%02d:%02d:%02d", hr, min % 60, sec % 60);
}

void Qosa::handleRoot()
{
    char temp[1500];
    char currTimeBuff[10];

    getTimeStampString(millis(), currTimeBuff);

    // change body using https://html-online.com/editor/
    //
    snprintf ( temp, 1500,

               "<html>\
               <head>\
               <meta http-equiv='refresh' content='5'/>\
            <title>Home Presence Sensor</title>\
            <style>\
            body  { background-color: #FFFFFF; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
            table { border-collapse: collapse;}\
            th, td { text-align: left; padding: 8px;}\
               tr:nth-child(even){background-color: #f2f2f2}\
               </style>\
               </head>\
               <body>\
               <h1 style='color: #5e9ca0;'>Qosa %s</h1><h2>Status</h2><p>&nbsp;</p><table><tbody><tr><td>MQTT server:</td><td>%s</td></tr><tr><td>MQTT port:</td><td>%d</td></tr><tr><td>MQTT connected:</td><td>%s</td></tr><tr><td>Topic prefix:</td><td>%s</td></tr></tbody></table><p>&nbsp;</p><h2>Statistics</h2><table><tbody><tr><td>UpTime</td><td>%s</td></tr></tbody></table><h2>Topics</h2><table><tbody><tr><td>%s</td></tr><tr><td>%s</td></tr><tr><td>%s</td></tr><tr><td>%s</td></tr></tbody></table><hr /><p><a href='/config.html'>Change config</a></p>\
            </body>\
            </html>",
            deviceName_.c_str(),
            mqttServer_.toString().c_str(),
            mqttPort_,
            (mqttClient_.connected()) ? "Yes" : "No",
            mqttPrefix_.c_str(),
            currTimeBuff,
            inPingTopic_.c_str(),
            outInfoTopic_.c_str(),
            outPongTopic_.c_str(),
            outAliveTopic_.c_str()
            );

    webServer_.send ( 200, "text/html", temp );
}

void Qosa::handleForceConfig()
{
    File touch = SPIFFS.open("/force.config", "w");
    if (!touch)
    {
        Serial.println("failed to create touch file");
    }
    touch.close();
    ESP.reset();
}

void Qosa::handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += webServer_.uri();
    message += "\nMethod: ";
    message += ( webServer_.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += webServer_.args();
    message += "\n";

    for ( uint8_t i = 0; i < webServer_.args(); i++ )
    {
        message += " " + webServer_.argName ( i ) + ": " + webServer_.arg ( i ) + "\n";
    }

    webServer_.send ( 404, "text/plain", message );
}

void Qosa::startWebServer()
{
    webServer_.on ( "/", c_handle_root);
    webServer_.on ( "/config.html", c_handle_force_config );
    webServer_.onNotFound ( c_handle_not_found );
    webServer_.begin();
    Serial.println ( "HTTP server started" );
}


////////////////////////////////////////////////////////////////////////////////////////
//
// MQTT related
//
////////////////////////////////////////////////////////////////////////////////////////

void Qosa::mQTTcallback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    if ( strcmp(topic, inPingTopic_.c_str()) == 0 )
    {
        pingRequested_ = true;
    }
}

bool Qosa::connectMQTT()
{
    Serial.println("Attempting MQTT identify server with mDNS...");

    int n = MDNS.queryService("mqtt", "tcp");
    if (n == 0)
    {
        Serial.println("No service found");
        return false;
    }

    Serial.println(MDNS.IP(0));
    Serial.println(MDNS.port(0));

    mqttClient_.setServer(MDNS.IP(0), MDNS.port(0));
    mqttClient_.setCallback(c_mqtt_callback);

    mqttServer_ = MDNS.IP(0);
    mqttPort_ = MDNS.port(0);

    // Attempt to connectMQT
    String ssid = deviceName_.c_str() + String(ESP.getChipId());
    
    if (!mqttClient_.connect(ssid.c_str()))
    {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(mqttClient_.state());
        Serial.println("");
        return false;
    }

    Serial.println("MQTT connected");

    // publish reboot info if any
    //
    publishRebootInfo();

    // ... and resubscribe
    //
    mqttClient_.subscribe(inPingTopic_.c_str());

    // now connected loops without mqtt are 0
    //
    noMqttLoops_ = 0;

    return true;
}

void Qosa::generateMQTTTopics()
{
    outInfoTopic_ = std::string(mqttPrefix_.c_str()) +"/" + outInfoTopic_;
    outPongTopic_ = std::string(mqttPrefix_.c_str()) + "/" + outPongTopic_;
    outAliveTopic_ = std::string(mqttPrefix_.c_str()) + "/" + outAliveTopic_;
    inPingTopic_ = std::string(mqttPrefix_.c_str()) + "/" + inPingTopic_;
}
