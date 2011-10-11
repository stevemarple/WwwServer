#if defined(ARDUINO) && ARDUINO < 100
#error You need Arduino-1.0-rc1 or better
#endif

#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>

#include <IniFile.h>
#include <WwwServer.h>


// Replace this with your assigned MAC address
//uint8_t mac[] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab };
uint8_t mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x00, 0x00 };

IPAddress ip(192,168,1,123);

// Adjust these pin numbers to suit your ethernet and SD devices.
const uint8_t ethernetSelect = 10;
const uint8_t sdSelect = 4;

uint16_t port = 80; // default port for HTTP

 // Config file for the web server, this must exist in the top-level
 // directory on your SD card. See included file for an example
WwwServer www("/www.ini", port);



// Buffer must be long enough to hold the GET request for the longest
// URL (including any query parameters you might want). It must also
// be long enough to hold the longest line in the ini file.
const int bufferLen = 80; 
char buffer[bufferLen];

void setup(void)
{
  Serial.begin(9600);
  
  // Configure all of the SPI select pins as outputs and make SPI
  // devices inactive, otherwise the earlier init routines may fail
  // for devices which have not yet been configured. If you have other
  // SD devices you must do the same for those too.
  pinMode(sdSelect, OUTPUT);
  digitalWrite(sdSelect, HIGH); // disable SD card
  
  pinMode(ethernetSelect, OUTPUT);
  digitalWrite(ethernetSelect, HIGH); // disable Ethernet
    
  if (!SD.begin(sdSelect))
    Serial.println("SD initialization failed");

  Serial.println("Configuring ethernet with DHCP...");

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


  // Process any web requests. In a time-critical system this can be
  // conditional on having enough time available before the next task
  // must start. Use the status information to gauge how long the
  // longest webserver task takes. Optimise this by keeping the
  // inifile short, with few comments and by structuring the SD file
  // system to avoid too many files in one direcotry, whilst
  // minimising the number of directories which must be searched.
  www.processRequest(buffer,  bufferLen);

  // print some statistics to the serial console every 20s
  unsigned long now = millis();
  if (now > lastStatsTime + 20000) {
    lastStatsTime = now;
    WwwServer::stats_t stats;
    const WwwServer::stats_t *sp = www.getStats();
    Serial.print("Statistics for to http://");
    Serial.print(Ethernet.localIP());
    if (port != 80) {
      Serial.print(':');
      Serial.print(port, DEC);
    }
    Serial.println('/');

    Serial.print("Number of requests: ");
    Serial.println(sp->requestCount);
    Serial.print("Longest duration of task: ");
    Serial.print(sp->requestTimeWorstCase);
    Serial.println(" us");
  }
}
