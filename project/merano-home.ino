#include "RelayShield.h"
#include "PietteTech_DHT.h"

#define DHTPIN D2     // D2 / D1
#define DHTTYPE DHT22		// DHT 22 (AM2302)
#define HEATERRELAY 1
#define CORR 0.5

PietteTech_DHT DHT(DHTPIN, DHTTYPE);
RelayShield myRelays;

int setTargetTemp(String command);
void readTempHum(double *temp, double *hum, PietteTech_DHT dht);

#define DEBUG
//#define FASTLOOP

#ifdef FASTLOOP
#define POLLTIME 15000
#define POLLTEMP 1
#else
#define POLLTIME 60000
#define POLLTEMP 60
#endif

double inTemp = 0;
double inHum = 0;
double targetTemp;
int heaterOn;
int countertemp = POLLTEMP;
// FIXME: check also system thread
ApplicationWatchdog wd(300000, System.reset);

int eepromaddr = 10;
struct LocalConfig {
  uint8_t version;
  double targetTemp;
};
LocalConfig myConfig;


void setup() {
  myRelays.begin();
    
  EEPROM.get(eepromaddr, myConfig);
  if (myConfig.version != 1) {
    LocalConfig defaultConfig = {1, 0};
    myConfig = defaultConfig;
  }
  targetTemp = myConfig.targetTemp;
    
  Particle.function("setTTemp", setTargetTemp);
  Particle.variable("inTemp", inTemp);
  Particle.variable("inHum", inHum);
  Particle.variable("tTemp", targetTemp);
  Particle.variable("heaterOn", heaterOn);
}

void readTempHum(double *temp, double *hum, PietteTech_DHT dht) {
  int result = dht.acquireAndWait();
  switch (result) {
  case DHTLIB_OK:
    break;
  case DHTLIB_ERROR_CHECKSUM:
    Particle.publish("Error\n\r\tChecksum error");
    return;
  case DHTLIB_ERROR_ISR_TIMEOUT:
    Particle.publish("Error\n\r\tISR time out error");
    return;
  case DHTLIB_ERROR_RESPONSE_TIMEOUT:
    Particle.publish("Error\n\r\tResponse time out error");
    return;
  case DHTLIB_ERROR_DATA_TIMEOUT:
    Particle.publish("Error\n\r\tData time out error");
    return;
  case DHTLIB_ERROR_ACQUIRING:
    Particle.publish("Error\n\r\tAcquiring");
    return;
  case DHTLIB_ERROR_DELTA:
    Particle.publish("Error\n\r\tDelta time to small");
    return;
  case DHTLIB_ERROR_NOTSTARTED:
    Particle.publish("Error\n\r\tNot started");
    return;
  default:
    Particle.publish("Unknown error");
    return;
  }
  *temp = dht.getCelsius();
  *hum = dht.getHumidity();
#ifdef DEBUG
  Particle.publish(String(*temp));
  Particle.publish(String(*hum));
#endif
}

void heaterCheck() {
  heaterOn = myRelays.isOn(HEATERRELAY);
  if (targetTemp == 0) {
    if (heaterOn == TRUE) myRelays.off(HEATERRELAY);
    return ;
  }
  if ((inTemp < (targetTemp - CORR)) && (heaterOn == FALSE)) {
    Particle.publish("Heater ON");
    myRelays.on(HEATERRELAY);
  }
  if ((inTemp > (targetTemp + CORR)) && (heaterOn == TRUE)) {
    Particle.publish("Heater OFF");
    myRelays.off(HEATERRELAY);
  }
  heaterOn = myRelays.isOn(HEATERRELAY);
}

void loop() {
#ifdef DEBUG
  Particle.publish("loop", String(countertemp));
  //Particle.publish(countertemp);
#endif
  // delay will check cloud and call particle.process each second
  delay(POLLTIME);
  if (countertemp == 0) {
    readTempHum(&inTemp, &inHum, DHT);
    countertemp = POLLTEMP;
  }else
    countertemp --;
  heaterCheck();
}

int setTargetTemp(String command){
  char inputStr[64];
  command.toCharArray(inputStr,64);
  targetTemp = atof(inputStr);
  Particle.publish("setTTemp", String(targetTemp));
  myConfig.targetTemp = targetTemp;
  EEPROM.put(eepromaddr, myConfig);
  return 0;
}
