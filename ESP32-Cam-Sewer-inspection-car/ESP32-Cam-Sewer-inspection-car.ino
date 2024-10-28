/*
 * esp32-cam-webserver.cpp
 *
 * This sketch is based on esp32-cam-webserver by Owen Carter.
 * This in turn is based on the 'official' ESP32 Camera example sketch from Expressif:
 *  https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer
 *
 *  Extensions by me:
 *  - Support a pan servo for the ESP32-Cam.
 *  - Support motor control with adjustable speed / power and going fixed distances.
 *  - ESP32 core 3.x adaption.
 *  - Support of missing PSRAM - with restricted resolution / quality.
 *  - Display of fps stream rate in Browser.
 *  - Improved documentation.
 *
 *  Features of Owen Carters version
 *
 *  - More options for default network and camera settings
 *  - Save and restore settings
 *  - Control of on-board lamps, rotate the view in the browser
 *  - Dedicated standalone stream viewer
 *  - Over The Air firmware updates
 *  - Lots of minor fixes and tweaks, documentation etc.
 *  - And 'reduced' by removing the Face Recognition features
 *
 *  Copyright (C) 2021-2024  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is available at https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car.
 *
 *  ESP32-Cam-Sewer-inspection-car is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/gpl.html>.
 */

/* 
 *  FOR NETWORK AND HARDWARE SETTINGS COPY OR RENAME 'myconfig.sample.h' TO 'myconfig.h' AND EDIT THAT.
 *
 * By default this sketch will assume an AI-THINKER ESP-CAM and create
 * an accesspoint called "ESP32-CAM-CONNECT" (password: "InsecurePassword")
 *
 */

#include <Arduino.h>

#include "esp32-cam-webserver.h"

#include <esp_camera.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include "parsebytes.h"
#include "time.h"
#if ESP_ARDUINO_VERSION >= 0x030000 // ESP_ARDUINO_VERSION_VAL(3, 0, 0), ESP_ARDUINO_VERSION_VAL disturbs the auto formatting :-(
#include "esp_private/periph_ctrl.h"
#endif

/*
 * Enable / disable extensions
 * Definitions are in MotorAndServoControl.hpp
 */
#define PAN_SERVO_SUPPORT   // on pin 12
#if defined(PAN_SERVO_SUPPORT)
const bool sPanServoIsSupported = true;
#else
const bool sPanServoIsSupported = false;
#endif

#define ONE_PWM_MOTOR_SUPPORT   // on pin 13
#if defined(ONE_PWM_MOTOR_SUPPORT)
const bool sOnePWMMotorIsSupported = true;
#else
const bool sOnePWMMotorIsSupported = false;
#endif
#if defined(PAN_SERVO_SUPPORT) || defined(ONE_PWM_MOTOR_SUPPORT)
#define DO_NOT_SUPPORT_RAMP // Ramps are anyway not used if drive speed voltage (default 2.0 V) is below 2.3 V. Saves 378 bytes program memory.
#include "MotorAndServoControl.h"
#else
/*
 * Helper macro for getting a macro definition as string
 */
#  if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#  endif
#endif

#define VERSION_EXAMPLE 1.0

/*
 * Include myconfig.h if available, otherwise we set defaults below
 */
// Primary config, or defaults.
#if __has_include("myconfig.h")
struct station {
    const char ssid[65];
    const char password[65];
    const bool dhcp;
};
// do no edit
#include "myconfig.h"
#else
#warning "Using Defaults: Copy myconfig.sample.h to myconfig.h and edit that to use your own settings"
#define WIFI_AP_ENABLE
#define CAMERA_MODEL_AI_THINKER
struct station {
    const char ssid[65];
    const char password[65];
    const bool dhcp;
} stationList[] = { { "ESP32-CAM-CONNECT", "InsecurePassword", true } };

#endif

// Pin Mappings
#include "camera_pins.h"

int sNumberOfFramebuffer = 2; // use 2 framebuffers if PSRAM is available, more than 2 does not increase the fps
/*
 * Configuration also contained in myconfig.h
 */
//#define LED_DISABLE       // Uncomment to disable the notification LED on the module
//#define LAMP_DISABLE      // Uncomment to disable the illumination lamp features
//#define LAMP_DEFAULT 0    // Define the startup lamp power setting (as a percentage, defaults to 0%)
//#define DEFAULT_INDEX_FULL // Default Page: uncomment to make the full control page the default, otherwise show simple viewer
//#define WIFI_AP_ENABLE    // Uncomment to enable AP mode
//#define AP_ADDRESS 192,168,4,2 // Change the AccessPoint ip address (default = 192.168.4.1)
//#define NO_OTA
//#define OTA_PASSWORD "SuperVisor"
//#define CAM_ROTATION 0 // one of 0, 90, -90
//#define CAM_NAME "ESP32-CAM-Robot-Car" // The camera name for the web interface
// Illumination LAMP and status LED
#if defined(LAMP_DISABLE)
int lampBrightnessPercentage = -1;  // lamp is disabled in config
#elif defined(LAMP_PIN)
#  if defined(LAMP_DEFAULT)
int lampBrightnessPercentage = constrain(LAMP_DEFAULT,0,100); // initial lamp value, range 0-100
#  else
int lampBrightnessPercentage = 0;   //default to off
#  endif
#else
int lampBrightnessPercentage = -1;  // no lamp pin assigned
#endif

bool autoLampValue = false;         // Start value of Auto Lamp Switch on GUI (auto on while camera running)

#if defined(LED_DISABLE)
#undef LED_PIN    // undefining this disables the notification LED
#endif

#if defined(CAM_NAME)
const char sApplicationName[] = CAM_NAME;
#else
const char sApplicationName[] = "ESP32 robot camera";
#endif
// initial rotation
// can be set in myconfig.h
#if !defined(CAM_ROTATION)
#define CAM_ROTATION 0
#endif
int myRotation = CAM_ROTATION;

/*
 * Setting values and defaults for defines included in myconfig.h
 */
// Ports for http and stream (override in myconfig.h)
#if defined(HTTP_PORT)
int httpPort = HTTP_PORT;
#else
int httpPort = 80;
#endif
#if defined(STREAM_PORT)
int streamPort = STREAM_PORT;
#else
int streamPort = 81;
#endif
#if !defined(WIFI_WATCHDOG)
#define WIFI_WATCHDOG 8000
#endif

// Select between full and simple index as the default.
#if defined(DEFAULT_INDEX_FULL)
char default_index[] = "full";
#else
char default_index[] = "simple";
#endif

#if defined(NO_FS)
bool filesystem = false;
#else
bool filesystem = true;
#endif
#if defined(NO_OTA)
bool otaEnabled = false;
#else
bool otaEnabled = true;
#endif
#if defined(OTA_PASSWORD)
char otaPassword[] = OTA_PASSWORD;
#else
char otaPassword[] = "";
#endif
#if defined(NTPSERVER)
bool haveTime = true;
const char* ntpServer = NTPSERVER;
const long  gmtOffset_sec = NTP_GMT_OFFSET;
const int   daylightOffset_sec = NTP_DST_OFFSET;
#else
bool haveTime = false;
const char *ntpServer = "";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
#endif

// Internal filesystem (SPIFFS)
// used for non-volatile camera settings
#include "storage.h"

// Sketch Info
int sketchSize;
int sketchSpace;
String sketchMD5;

// Number of known networks in stationList[] which is defined in myconfig.h
int stationCount = sizeof(stationList) / sizeof(stationList[0]);

// Start with accesspoint mode disabled, wifi setup will activate it if
// no known networks are found, and WIFI_AP_ENABLE has been defined
bool sInAccesspointMode = false;
// If we have AP mode enabled, ignore first entry in the stationList[]
#if defined(WIFI_AP_ENABLE) // 4 simultaneous connections in AP mode
int firstStation = 1;
#else
int firstStation = 0;
#endif

// IP address, Netmask and Gateway, populated when connected
IPAddress ip;
IPAddress net;
IPAddress gw;

// Declare external function from app_httpd.cpp
extern void startCameraServer(int hPort, int sPort);
extern void serialDump();

const char sExampleVersion[] = STR(VERSION_EXAMPLE);
const char sCompileTimestamp[] = __DATE__ " " __TIME__; // for serialDump() etc.

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
bool sCaptivePortalEnabled = false; // 4 simultaneous connections in AP mode
char apName[64] = "Undefined";

// The app and stream URLs
char httpURL[64] = { "Undefined" };
char streamURL[64] = { "Undefined" };

// Counters for info screens and debug
int8_t streamCount = 0;          // Number of currently active streams
unsigned long streamsServed = 0; // Total completed streams
unsigned long imagesServed = 0;  // Total image requests

// Camera module bus communications frequency.
// Originally: config.xclk_freq_hz = 20000000, but this lead to visual artifacts on many modules.
// See https://github.com/espressif/esp32-camera/issues/150#issuecomment-726473652 et al.
#if !defined (XCLK_FREQ_HZ)
#define XCLK_FREQ_HZ 16500000
#endif

const int lampChannel = 2; // a free PWM channel (channel 0 used by Servo, channel 4 used by PWMMotor and maybe some channels used by camera)
const int lampPWMFrequency = 1000;  // 1K pwm frequency
const int lampPWMResolution = 9;    // duty cycle bit range for lamp
const int lampPWMMax = pow(2, lampPWMResolution) - 1;

// Critical error string; if set during init (camera hardware failure) it
// will be returned for all http requests
String critERR = "";

// Debug flag for stream and capture data
bool debugData;

void debugOn() {
    debugData = true;
    Serial.println("Camera debug data is enabled (send 'd' for status dump, or any other char to disable debug)");
}

void debugOff() {
    debugData = false;
    Serial.println("Camera debug data is disabled (send 'd' for status dump, or any other char to enable debug)");
}

// Serial input (debugging controls)
void handleSerial() {
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'd') {
            serialDump();
        } else {
            if (debugData)
                debugOff();
            else
                debugOn();
        }
    }
    while (Serial.available())
        Serial.read();  // chomp the buffer
}

// Notification LED 
void flashLED(int flashtime) {
#if defined(LED_PIN )               // If we have it; flash it.
    digitalWrite(LED_PIN, LED_ON);  // On at full power.
    delay(flashtime);               // delay
    digitalWrite(LED_PIN, LED_OFF); // turn Off
#else
    return;                         // No notifcation LED, do nothing, no delay
#endif
}

// Lamp Control
void setLamp(int aNewPercent) {
#if defined(LAMP_PIN)
    if (aNewPercent != -1) {
        // Apply a logarithmic function to the scale.
        int brightness = pow(aNewPercent / 100.0, 2.6) * lampPWMMax + 0.5; // 2.6 is gamma - can be also 3.3...
//        Serial.print("newVal / 100.0 = ");
//        Serial.print(aNewPercent / 100.0);
//        Serial.print(" pow=");
//        Serial.print(pow(aNewPercent / 100.0, 2.6));
//        Serial.print(" pwmMax=");
//        Serial.println(lampPWMMax);
#  if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        ledcWrite(LAMP_PIN, brightness);
#  else
        ledcWrite(lampChannel, brightness);
#  endif
        Serial.print("Lamp: ");
        Serial.print(aNewPercent);
        Serial.print("%, pwm = ");
        Serial.println(brightness);
    }
#endif
}

void printLocalTime(bool extraData = false) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
    } else {
        Serial.println(&timeinfo, "%H:%M:%S, %A, %B %d %Y");
    }
    if (extraData) {
        Serial.printf("NTP Server: %s, GMT Offset: %li(s), DST Offset: %i(s)\r\n", ntpServer, gmtOffset_sec, daylightOffset_sec);
    }
}

void calcURLs() {
    // Set the URL's
#if defined(URL_HOSTNAME)
    if (httpPort != 80) {
        sprintf(httpURL, "http://%s:%d/", URL_HOSTNAME, httpPort);
    } else {
        sprintf(httpURL, "http://%s/", URL_HOSTNAME);
    }
    sprintf(streamURL, "http://%s:%d/", URL_HOSTNAME, streamPort);
#else
    Serial.println("Setting httpURL");
    if (httpPort != 80) {
        sprintf(httpURL, "http://%d.%d.%d.%d:%d/", ip[0], ip[1], ip[2], ip[3], httpPort);
    } else {
        sprintf(httpURL, "http://%d.%d.%d.%d/", ip[0], ip[1], ip[2], ip[3]);
    }
    sprintf(streamURL, "http://%d.%d.%d.%d:%d/", ip[0], ip[1], ip[2], ip[3], streamPort);
#endif
}

void WifiSetup() {
    // Feedback that we are now attempting to connect
    flashLED(300);
    delay(100);
    flashLED(300);
    Serial.println("Starting WiFi");

    // Disable power saving on WiFi to improve responsiveness 
    // (https://github.com/espressif/arduino-esp32/issues/1484)
    WiFi.setSleep(false);

    Serial.print("Known external SSIDs: ");
    if (stationCount > firstStation) {
        for (int i = firstStation; i < stationCount; i++)
            Serial.printf(" '%s'", stationList[i].ssid);
    } else {
        Serial.print("None");
    }
    Serial.println();
    byte mac[6] = { 0, 0, 0, 0, 0, 0 };
    WiFi.macAddress(mac);
    Serial.printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    int bestStation = -1;
    long bestRSSI = -1024;
    char bestSSID[65] = "";
    uint8_t bestBSSID[6];
    if (stationCount > firstStation) {
        // We have a list to scan 
        Serial.printf("Scanning local Wifi Networks\r\n");
        int stationsFound = WiFi.scanNetworks();
        Serial.printf("%i networks found\r\n", stationsFound);
        if (stationsFound > 0) {
            for (int i = 0; i < stationsFound; ++i) {
                // Print SSID and RSSI for each network found
                String thisSSID = WiFi.SSID(i); // call method of WiFiScanClass
                int thisRSSI = WiFi.RSSI(i);
                String thisBSSID = WiFi.BSSIDstr(i);
                Serial.printf("%3i : [%s] %s (%i)", i + 1, thisBSSID.c_str(), thisSSID.c_str(), thisRSSI);
                // Scan our list of known external stations
                for (int sta = firstStation; sta < stationCount; sta++) {
                    if ((strcmp(stationList[sta].ssid, thisSSID.c_str()) == 0)
                            || (strcmp(stationList[sta].ssid, thisBSSID.c_str()) == 0)) {
                        Serial.print("  -  Known!");
                        // Chose the strongest RSSI seen
                        if (thisRSSI > bestRSSI) {
                            bestStation = sta;
                            strncpy(bestSSID, thisSSID.c_str(), 64);
                            // Convert char bssid[] to a byte array
                            parseBytes(thisBSSID.c_str(), ':', bestBSSID, 6, 16);
                            bestRSSI = thisRSSI;
                        }
                    }
                }
                Serial.println();
            }
        }
    } else {
        // No list to scan, therefore we are an accesspoint
        sInAccesspointMode = true;
    }

    if (bestStation == -1) {
        if (!sInAccesspointMode) {
#if defined(WIFI_AP_ENABLE)
            Serial.println("No known networks found, entering AccessPoint fallback mode");
            sInAccesspointMode = true;
#else
            Serial.println("No known networks found");
#endif
        } else {
            Serial.println("AccessPoint mode selected in config");
        }
    } else {
        Serial.printf("Connecting to Wifi Network %d: [%02X:%02X:%02X:%02X:%02X:%02X] %s \r\n", bestStation, bestBSSID[0],
                bestBSSID[1], bestBSSID[2], bestBSSID[3], bestBSSID[4], bestBSSID[5], bestSSID);
        // Apply static settings if necessary
        if (stationList[bestStation].dhcp == false) {
#if defined(ST_IP)
            Serial.println("Applying static IP settings");
#  if !defined (ST_GATEWAY)  || !defined (ST_NETMASK)
#  error "You must supply both Gateway and NetMask when specifying a static IP address"
#  endif
            IPAddress staticIP(ST_IP);
            IPAddress gateway(ST_GATEWAY);
            IPAddress subnet(ST_NETMASK);
#  if !defined(ST_DNS1)
            WiFi.config(staticIP, gateway, subnet);
#  else
            IPAddress dns1(ST_DNS1);
#    if !defined(ST_DNS2)
            WiFi.config(staticIP, gateway, subnet, dns1);
#    else
            IPAddress dns2(ST_DNS2);
            WiFi.config(staticIP, gateway, subnet, dns1, dns2);
#    endif
#  endif
#else
            Serial.println("Static IP settings requested but not defined in config, falling back to dhcp");
#endif // defined(ST_IP)
        }

#if defined(HOSTNAME)
        WiFi.setHostname(HOSTNAME);
#endif

        // Initiate network connection request (3rd argument, channel = 0 is 'auto')
        WiFi.begin(bestSSID, stationList[bestStation].password, 0, bestBSSID);

        // Wait to connect, or timeout
        unsigned long start = millis();
        while ((millis() - start <= WIFI_WATCHDOG) && (WiFi.status() != WL_CONNECTED)) {
            delay(500);
            Serial.print('.');
        }
        // If we have connected, inform user
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Client connection succeeded");
            sInAccesspointMode = false;
            // Note IP details
            ip = WiFi.localIP();
            net = WiFi.subnetMask();
            gw = WiFi.gatewayIP();
            Serial.printf("IP address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("Netmask   : %d.%d.%d.%d\r\n", net[0], net[1], net[2], net[3]);
            Serial.printf("Gateway   : %d.%d.%d.%d\r\n", gw[0], gw[1], gw[2], gw[3]);
            calcURLs();
            // Flash the LED 4 times to show we are connected
            for (int i = 0; i < 4; i++) {
                flashLED(50);
                delay(250);
            }
        } else {
            Serial.println("Client connection Failed");
            WiFi.disconnect();   // (resets the WiFi scan)
        }
    }

    if (sInAccesspointMode && (WiFi.status() != WL_CONNECTED)) {
        // The accesspoint has been enabled, and we have not connected to any existing networks
#if defined(AP_CHAN)
        Serial.println("Setting up Fixed Channel AccessPoint");
        Serial.print("  SSID     : ");
        Serial.println(stationList[0].ssid);
        Serial.print("  Password : ");
        Serial.println(stationList[0].password);
        Serial.print("  Channel  : ");
        Serial.println(AP_CHAN);
        WiFi.softAP(stationList[0].ssid, stationList[0].password, AP_CHAN);
#else
        Serial.println("Setting up AccessPoint");
        Serial.print("  SSID     : ");
        Serial.println(stationList[0].ssid);
        Serial.print("  Password : ");
        Serial.println(stationList[0].password);
        if (strlen(stationList[0].password) == 0) {
            WiFi.softAP(stationList[0].ssid, NULL);
        } else {
            WiFi.softAP(stationList[0].ssid, stationList[0].password);
        }
#endif
#if defined(AP_ADDRESS)
        // User has specified the AP details; apply them after a short delay
        // (https://github.com/espressif/arduino-esp32/issues/985#issuecomment-359157428)
        delay(100);
        IPAddress local_IP(AP_ADDRESS);
        IPAddress gateway(AP_ADDRESS);
        IPAddress subnet(255,255,255,0);
        WiFi.softAPConfig(local_IP, gateway, subnet);
#endif
        // Note AP details
        ip = WiFi.softAPIP();
        net = WiFi.subnetMask();
        gw = WiFi.gatewayIP();
//        strcpy(apName, WiFi.SSID().c_str());  // is empty :-(
        strcpy(apName, stationList[0].ssid);
        Serial.printf("IP address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
        calcURLs();
        // Flash the LED to show we are connected
        for (int i = 0; i < 5; i++) {
            flashLED(150);
            delay(50);
        }
        // Start the DNS captive portal if requested
        if (stationList[0].dhcp == true) {
            Serial.println("Starting Captive Portal");
            dnsServer.start(DNS_PORT, "*", ip);
            sCaptivePortalEnabled = true;
        }
    }
}
void setup() {
#if defined(LED_PIN)  // If we have a notification LED, set it to output
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_ON);
#endif
#if defined(LAMP_PIN)
    // let LED flash
    pinMode(LAMP_PIN, OUTPUT);
    digitalWrite(LAMP_PIN, HIGH);
#endif

    Serial.begin(115200);
//    Serial.setDebugOutput(true);
    Serial.println("START " __FILE__ "\r\nVersion " STR(VERSION_EXAMPLE) " from " __DATE__ " " __TIME__);

#if defined(LAMP_PIN)
    // end of lamp flash
    digitalWrite(LAMP_PIN, LOW);
#endif

    if (stationCount == 0) {
        Serial.println("\r\nFatal Error; Halting");
        while (true) {
            Serial.println(
                    "No wifi details have been configured; we cannot connect to existing WiFi or start our own AccessPoint, there is no point in proceeding.");
            delay(5000);
        }
    }

#if defined(LED_PIN)  // If we have a notification LED, set it to output
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_ON);
#endif

    // Create camera config structure; and populate with hardware and other defaults 
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
#  if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
#else
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
#endif
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.pixel_format = PIXFORMAT_JPEG;
    // Pre-allocate large buffers
    if (psramFound()) {
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10; //0-63 lower number means higher quality
        /*
         * Frame buffer count also used for stills :-(
         */
        config.fb_count = sNumberOfFramebuffer; // two are required to double the frame rate, more makes no sense
        Serial.printf("PSRAM detected. Set %u framebuffer for UXGA and quality %u", config.fb_count, config.jpeg_quality);
    } else {
        /*
         * Maximum quality without PSRAM: SVGA: 6 (Maximum), XGA: 9, HD: 10, SXGA: 16, UXGA: 27
         * This depends on the content of the picture, which determine its size, and thus may not fit into our buffer with these quality settings..
         * Then try the next lower setting.
         */
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 6;
        config.fb_count = 1;
        sNumberOfFramebuffer = 1;
        Serial.printf("No PSRAM found. Set %u framebuffer for VGA and quality to %u.\r\n", config.fb_count, config.jpeg_quality);
        Serial.printf("High resolution/quality images & streams will show incomplete frames due to low memory.\r\n");
    }

#if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
#endif

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        delay(100);  // need a delay here or the next serial o/p gets missed
        Serial.printf("\r\n\r\nCRITICAL FAILURE: Camera sensor failed to initialise.\r\n\r\n");
        Serial.printf("A full (hard, power off/on) reboot will probably be needed to recover from this.\r\n");
        Serial.printf("Meanwhile; this unit will reboot in 1 minute since these errors sometime clear automatically\r\n");
        // Reset the I2C bus.. may help when rebooting. See https://esp32.com/viewtopic.php?t=3152
        periph_module_disable(PERIPH_I2C0_MODULE); // try to shut I2C down properly in case that is the problem
        periph_module_disable(PERIPH_I2C1_MODULE);
        periph_module_reset(PERIPH_I2C0_MODULE);
        periph_module_reset(PERIPH_I2C1_MODULE);
        // And set the error text for the UI
        critERR = "<h1>Error!</h1><hr><p>Camera module failed to initialise!</p><p>Please reset (power off/on) the camera.</p>";
        critERR += "<p>We will continue to reboot once per minute since this error sometimes clears automatically.</p>";
        // Start a 60 second watchdog timer
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        esp_task_wdt_config_t twdt_config = { .timeout_ms = 60000, .idle_core_mask = 0x03,    // Bitmask of all cores
                .trigger_panic = true, };
        esp_task_wdt_init(&twdt_config);  // schedule a a watchdog panic event for 3 seconds in the future
#else
        esp_task_wdt_init(60, true);
#endif
        esp_task_wdt_add(NULL);
    } else {
        Serial.println("Camera init succeeded");

        // Get a reference to the sensor
        sensor_t *s = esp_camera_sensor_get();

        // Dump camera module, warn for unsupported modules.
        switch (s->id.PID) {
        case OV9650_PID:
            Serial.println("WARNING: OV9650 camera module is not properly supported, will fallback to OV2640 operation");
            break;
        case OV7725_PID:
            Serial.println("WARNING: OV7725 camera module is not properly supported, will fallback to OV2640 operation");
            break;
        case OV2640_PID:
            Serial.println("OV2640 camera module detected");
            break;
        case OV3660_PID:
            Serial.println("OV3660 camera module detected");
            break;
        default:
            Serial.println("WARNING: Camera module is unknown and not properly supported, will fallback to OV2640 operation");
        }

        // OV3660 initial sensors are flipped vertically and colors are a bit saturated
        if (s->id.PID == OV3660_PID) {
            s->set_vflip(s, 1);  //flip it back
            s->set_brightness(s, 1);  //up the blightness just a bit
            s->set_saturation(s, -2);  //lower the saturation
        }
#if defined(PAN_SERVO_SUPPORT)
        if (s->id.PID == OV2640_PID) {
            s->set_hmirror(s, 1);  //flip it top down
            s->set_vflip(s, 1);  //flip it back
        }
#endif
        // M5 Stack Wide has special needs
#if defined(CAMERA_MODEL_M5STACK_WIDE)
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
#endif

        // Config can override mirror and flip
#if defined(H_MIRROR)
        s->set_hmirror(s, H_MIRROR);
#endif
#if defined(V_FLIP)
        s->set_vflip(s, V_FLIP);
#endif

        // set initial frame rate
#if defined(DEFAULT_RESOLUTION)
        s->set_framesize(s, DEFAULT_RESOLUTION);
#else
        s->set_framesize(s, FRAMESIZE_SVGA);
#endif

        /*
         * Add any other defaults you want to apply at startup here:
         * uncomment the line and set the value as desired (see the comments)
         *
         * these are defined in the esp headers here:
         * https://github.com/espressif/esp32-camera/blob/master/driver/include/sensor.h#L149
         */

        //s->set_framesize(s, FRAMESIZE_SVGA); // FRAMESIZE_[QQVGA|HQVGA|QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA|QXGA(ov3660)]);
        //s->set_quality(s, val);       // 10 to 63
        //s->set_brightness(s, 0);      // -2 to 2
        //s->set_contrast(s, 0);        // -2 to 2
        //s->set_saturation(s, 0);      // -2 to 2
        //s->set_special_effect(s, 0);  // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
        //s->set_whitebal(s, 1);        // aka 'awb' in the UI; 0 = disable , 1 = enable
        //s->set_awb_gain(s, 1);        // 0 = disable , 1 = enable
        //s->set_wb_mode(s, 0);         // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
        //s->set_exposure_ctrl(s, 1);   // 0 = disable , 1 = enable
        //s->set_aec2(s, 0);            // 0 = disable , 1 = enable
        //s->set_ae_level(s, 0);        // -2 to 2
        //s->set_aec_value(s, 300);     // 0 to 1200
        //s->set_gain_ctrl(s, 1);       // 0 = disable , 1 = enable
        //s->set_agc_gain(s, 0);        // 0 to 30
        //s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        //s->set_bpc(s, 0);             // 0 = disable , 1 = enable
        //s->set_wpc(s, 1);             // 0 = disable , 1 = enable
        //s->set_raw_gma(s, 1);         // 0 = disable , 1 = enable
        //s->set_lenc(s, 1);            // 0 = disable , 1 = enable
        //s->set_hmirror(s, 0);         // 0 = disable , 1 = enable
        //s->set_vflip(s, 0);           // 0 = disable , 1 = enable
        //s->set_dcw(s, 1);             // 0 = disable , 1 = enable
        //s->set_colorbar(s, 0);        // 0 = disable , 1 = enable
        // We now have camera with default init
        // check for saved preferences and apply them
        if (filesystem) {
            filesystemStart();
            loadPrefs(SPIFFS);
        } else {
            Serial.println("No Internal Filesystem, cannot load or save preferences");
        }
    }

    /*
     * Camera setup complete; initialise the rest of the hardware.
     */

    // Initialise and set the lamp
    if (lampBrightnessPercentage != -1) {
#if defined(LAMP_PIN)
#  if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        ledcAttachChannel(LAMP_PIN, lampPWMFrequency, lampPWMResolution, lampChannel); // New API - 2 channels share same timer and resolution
#  else
        ledcSetup(lampChannel, lampPWMFrequency, lampPWMResolution);  // configure LED PWM channel
        ledcAttachPin(LAMP_PIN, lampChannel);            // attach the GPIO pin to the channel
#  endif
#endif
        if (autoLampValue) {
            setLamp(0);                        // set default value
        } else {
            setLamp(lampBrightnessPercentage);
        }

    } else {
        Serial.println("No lamp, or lamp disabled in config");
    }

    // Having got this far; start Wifi and loop until we are connected or have started an AccessPoint
    while ((WiFi.status() != WL_CONNECTED) && !sInAccesspointMode) {
        WifiSetup();
        delay(1000);
    }

    if (otaEnabled) {
        // Start OTA once connected
        Serial.println("Setting up OTA");
        // Port defaults to 3232
        // ArduinoOTA.setPort(3232); 
        // Hostname defaults to esp3232-[MAC]
        ArduinoOTA.setHostname(sApplicationName);
        // No authentication by default
        ArduinoOTA.setPort(3232); // Required for Sloeber
        if (strlen(otaPassword) != 0) {
            ArduinoOTA.setPassword(otaPassword);
            Serial.printf("OTA Password: %s\n\r", otaPassword);
        } else {
            Serial.printf("\r\nNo OTA password has been set! (insecure)\r\n\r\n");
        }
        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "sketch";
            } else {
                // U_SPIFFS
                type = "filesystem";
            }
            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        });
        ArduinoOTA.onEnd([]() {
            Serial.println("\r\nEnd");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            unsigned int tPercent = progress / (total / 100);
            static int sLastPercent;
            if (sLastPercent != tPercent) {
                sLastPercent = tPercent;
                Serial.printf("Progress: %u%%\r\n", tPercent);
            }
        });
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });
        ArduinoOTA.begin();
    } else {
        Serial.println("OTA is disabled");
    }

    // Set time via NTP server when enabled
    if (haveTime) {
        Serial.print("Time: ");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        printLocalTime(true);
    } else {
        Serial.println("Time functions disabled");
    }

    // Now we have a network we can start the two http handlers for the UI and Stream.
    startCameraServer(httpPort, streamPort);

    if (critERR.length() == 0) {
        Serial.printf("\r\nCamera Ready!\r\nUse '%s' to connect\r\n", httpURL);
        Serial.printf("Stream viewer available at '%sview'\r\n", streamURL);
        Serial.printf("Raw stream URL is '%s'\r\n", streamURL);
#if defined(DEBUG_DEFAULT_ON)
        debugOn();
#else
        debugOff();
#endif
    } else {
        Serial.printf("\r\nCamera unavailable due to initialization errors.\r\n\r\n");
    }

    // Used when dumping status; these are slow functions, so just do them once during startup
    sketchSize = ESP.getSketchSize();
    sketchSpace = ESP.getFreeSketchSpace();
    sketchMD5 = ESP.getSketchMD5();

    // As a final init step chomp out the serial buffer in case we have recieved mis-keys or garbage during startup
    while (Serial.available()) {
        Serial.read();
    }

    initServoAndMotorPinsAndChannels(sInAccesspointMode);

    // While in Beta; Warn!
    Serial.print("\r\nThis is the 4.0 alpha\r\n - Face detection has been removed!\r\n");
}

void loop() {
    /* 
     *  Just loop forever, reconnecting Wifi As necessary in client mode
     * The stream and URI handler processes initiated by the startCameraServer() call at the
     * end of setup() will handle the camera and UI processing from now on.
     */
    if (sInAccesspointMode) {
        // Accespoint is permanently up, so just loop, servicing the captive portal as needed
        // Rather than loop forever, follow the watchdog, in case we later add auto re-scan.
        unsigned long start = millis();
        while (millis() - start < WIFI_WATCHDOG) {
            delay(100);
            if (otaEnabled) {
                ArduinoOTA.handle();
            }
            handleSerial();
            updateMotor();
            checkForAttention();
            if (sCaptivePortalEnabled) {
                dnsServer.processNextRequest();
            }
        }
    } else {
        // client mode can fail; so reconnect as appropriate 
        static bool warned = false;
        if (WiFi.status() == WL_CONNECTED) {
            // We are connected, wait a bit and re-check
            if (warned) {
                // Tell the user if we have just reconnected 
                Serial.println("WiFi reconnected");
                warned = false;
            }
            // loop here for WIFI_WATCHDOG, turning debugData true/false depending on serial input..
            unsigned long start = millis();
            while (millis() - start < WIFI_WATCHDOG) {
                delay(100);
                if (otaEnabled) {
                    ArduinoOTA.handle();
                }
                handleSerial();
                updateMotor();
                checkForAttention();
            }
        } else {
            /*
             * disconnected; attempt to reconnect
             */
            if (!warned) {
                // Tell the user if we just disconnected
                WiFi.disconnect();  // ensures disconnect is complete, wifi scan cleared
                Serial.println("WiFi disconnected, retrying");
                warned = true;
            }
            WifiSetup();
        }
    }
}
