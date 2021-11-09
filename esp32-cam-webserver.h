// External variables declared in the esp32-cam-webserver.ino

#include <WiFi.h> // for IPAddress etc.

extern char myName[];
extern char myVer[];
extern char baseVersion[];
extern IPAddress ip;
extern IPAddress net;
extern IPAddress gw;
extern bool accesspoint;
extern char apName[];
extern bool captivePortal;
extern int httpPort;
extern int streamPort;
extern char httpURL[];
extern char streamURL[];
extern char default_index[];
extern int8_t streamCount;
extern unsigned long streamsServed;
extern unsigned long imagesServed;
extern int myRotation;
extern int lampVal;
extern bool autoLamp;
extern int ServoPanDegree;
extern int LastMotorSpeed;
extern bool filesystem;
extern String critERR;
extern bool debugData;
extern bool haveTime;
extern int sketchSize;
extern int sketchSpace;
extern String sketchMD5;
extern bool otaEnabled;
extern char otaPassword[];

// Functions from the main .ino
void flashLED(int flashtime);
void setLamp(int newVal);
void printLocalTime(bool extraData);
