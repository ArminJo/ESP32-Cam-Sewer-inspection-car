// External variables declared in the esp32-cam-webserver.ino

#include <WiFi.h> // for IPAddress etc.

extern const char sApplicationName[];
extern const char sCompileTimestamp[];
extern const char sExampleVersion[];
extern IPAddress ip;
extern IPAddress net;
extern IPAddress gw;
extern bool sInAccesspointMode;
extern char apName[];
extern bool sCaptivePortalEnabled;
extern int httpPort;
extern int streamPort;
extern char httpURL[];
extern char streamURL[];
extern char default_index[];
extern int8_t streamCount;
extern unsigned long streamsServed;
extern unsigned long imagesServed;
extern int myRotation;
extern int lampBrightnessPercentage;
extern bool autoLampValue;
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

extern const bool sPanServoIsSupported;
extern const bool sOnePWMMotorIsSupported;
extern int sNumberOfFramebuffer;


// Functions from the main .ino
void flashLED(int flashtime);
void setLamp(int newVal);
void printLocalTime(bool extraData);
