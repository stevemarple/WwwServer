#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

#include <IniFile.h>
#include <WwwServer.h>

#if defined(ARDUINO) && ARDUINO < 100
#error You need Arduino-1.0-beta1 or better
#endif

// Replace this with your assigned MAC address
uint8_t mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x45, 0xE5 };
IPAddress ip(192,168,1,207);

uint16_t port = 80;
WwwServer www("/www.ini", port);

// Buffer must be long enough to hold the GET request for the longest
// URL (including any query parameters you might want). It must also
// be long enough to hold the longest line in the ini file.
const int bufferLen = 64; 
char buffer[bufferLen];

void setup(void)
{
  delay(1000);
  Serial.begin(9600);
  Serial.println("--------");
  
  if (!SD.begin(4))
    Serial.println("SD initialization failed");
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP failed");
    Ethernet.begin(mac, ip);
  }
  
  Serial.print("My IP address is ");
  Serial.println(Ethernet.localIP());

  Serial.print("Connect to http://");
  Serial.print(Ethernet.localIP());
  if (port != 80) {
    Serial.print(':');
    Serial.print(port, DEC);
  }
  Serial.println('/');

  if (!www.begin(buffer,  bufferLen))
    Serial.println("www.begin() failed");

}

unsigned long lastStatsTime = 0;
void loop(void)
{
  // Do the real work/data logging here
  // delay(500);

  www.processRequest(buffer,  bufferLen);

  int8_t worstCaseState;
  unsigned long timeTakenWorstCase, timeTakenTotal, processRequestCount;

  /*
  if (millis() > lastStatsTime + 30000UL) {
    lastStatsTime = millis();
    const WwwServer::stats_t *sp = www.getStats();
    Serial.print("Web server statistics\n    Total requests: ");
    Serial.println(sp->requestCount, DEC);
    Serial.print("    Worst case request time: ");
    Serial.print(sp->requestTimeWorstCase, DEC);
    Serial.print("uS\n    Worst case task time: ");
    Serial.print(sp->taskTimeWorstCase, DEC);
    Serial.print("uS\n    Worst case task state: #");
    Serial.println(sp->taskWorstCaseState, DEC);
  }
  */
}
