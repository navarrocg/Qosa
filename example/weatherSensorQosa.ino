#include <Qosa.h>
#include <WEMOS_SHT3X.h>

#define WHOAMI "weatherSensor"
#define PREFIX "home/multiRoom/weatherSensor"

static Qosa * qosa;
static String outTempTopic = PREFIX"/temp";
static String outHumidityTopic = PREFIX"/hum";
static SHT3X sht30(0x45);

void setup() {
  qosa = Qosa::gimmeAQosa(WHOAMI,PREFIX,"d5j77f70");
  qosa->init();
}

void loop() 
{
  char msg[100];
  qosa->processLoop();
  
  float accTemp = 0.0;
  float accHum = 0.0;      
  int samples = 5;

  for ( int i = 0; i < samples; i++ )
  {
    sht30.get();
    accTemp += sht30.cTemp;
    accHum += sht30.humidity;
    delay(20);
  }
  accTemp = accTemp / (float) samples;
  accHum = accHum / (float) samples;

  dtostrf( accTemp,3,2,msg);
  Serial.println(msg);
  qosa->getMqttClient()->publish(outTempTopic.c_str(), msg);

  dtostrf(accHum,3,2,msg);
  Serial.println(msg);
  qosa->getMqttClient()->publish(outHumidityTopic.c_str(), msg);      

  int sleepSeconds = 120;
  // convert to microseconds
  ESP.deepSleep(sleepSeconds * 1000000);
}
