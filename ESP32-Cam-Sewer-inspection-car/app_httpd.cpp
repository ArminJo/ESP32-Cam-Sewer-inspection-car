// Original Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Extensions:
//  Copyright (C) 2024  Armin Joachimsmeyer
//  armin.joachimsmeyer@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>

#include <esp_task_wdt.h>
#include <Arduino.h>
#include <WiFi.h>

#if ESP_ARDUINO_VERSION >= 0x030000 // ESP_ARDUINO_VERSION_VAL(3, 0, 0), ESP_ARDUINO_VERSION_VAL disturbs the auto formatting :-(
#include "esp_private/periph_ctrl.h"
#endif

#include "index_ov2640.h"
#include "index_ov3660.h"
#include <index_small.h>
#include <index_other.h>
#include "css.h"
#include "favicons.h"
#include "logo.h"
#include "storage.h"

#include "esp32-cam-webserver.h"
#include "MotorAndServoControl.h"

typedef struct {
    httpd_req_t *req;
    size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

int64_t sFirstFrameTime = 0;
int64_t sLastFrameTime = 0;
int sFpsFrameCount = 0;

void serialDump() {
    Serial.println();
    // Module
    Serial.printf("Name: %s\r\n", sApplicationName);
    if (haveTime) {
        Serial.print("Time: ");
        printLocalTime(true);
    }
    Serial.printf("Firmware: %s (base: %s)\r\n", sCompileTimestamp, sExampleVersion);
    float sketchPct = 100 * sketchSize / sketchSpace;
    Serial.printf("Sketch Size: %i (total: %i, %.1f%% used)\r\n", sketchSize, sketchSpace, sketchPct);
    Serial.printf("MD5: %s\r\n", sketchMD5.c_str());
    Serial.printf("ESP sdk: %s\r\n", ESP.getSdkVersion());
    if (otaEnabled) {
        if (strlen(otaPassword) != 0) {
            Serial.printf("OTA: Enabled, Password: %s\r\n", otaPassword);
        } else {
            Serial.printf("OTA: Enabled, No Password! (insecure)\r\n");
        }
    } else {
        Serial.printf("OTA: Disabled\r\n");
    }
    // Network
    if (sInAccesspointMode) {
        if (sCaptivePortalEnabled) {
            Serial.printf("WiFi Mode: AccessPoint with captive portal\r\n");
        } else {
            Serial.printf("WiFi Mode: AccessPoint\r\n");
        }
        Serial.printf("WiFi SSID: %s\r\n", apName);
    } else {
        Serial.printf("WiFi Mode: Client\r\n");
        String ssidName = WiFi.SSID();
        Serial.printf("WiFi Ssid: %s\r\n", ssidName.c_str());
        Serial.printf("WiFi Rssi: %i\r\n", WiFi.RSSI());
        String bssid = WiFi.BSSIDstr();
        Serial.printf("WiFi BSSID: %s\r\n", bssid.c_str());
    }
    Serial.printf("WiFi IP address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    if (!sInAccesspointMode) {
        Serial.printf("WiFi Netmask: %d.%d.%d.%d\r\n", net[0], net[1], net[2], net[3]);
        Serial.printf("WiFi Gateway: %d.%d.%d.%d\r\n", gw[0], gw[1], gw[2], gw[3]);
    }
    Serial.printf("WiFi Http port: %i, Stream port: %i\r\n", httpPort, streamPort);
    byte mac[6];
    WiFi.macAddress(mac);
    Serial.printf("WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // System
    int64_t sec = esp_timer_get_time() / 1000000;
    int64_t upDays = int64_t(floor(sec / 86400));
    int upHours = int64_t(floor(sec / 3600)) % 24;
    int upMin = int64_t(floor(sec / 60)) % 60;
    int upSec = sec % 60;
    int McuTc = (temprature_sens_read() - 32) / 1.8; // celsius
    int McuTf = temprature_sens_read(); // fahrenheit
    Serial.printf("System up: %" PRId64 ":%02i:%02i:%02i (d:h:m:s)\r\n", upDays, upHours, upMin, upSec);
    Serial.printf("Active streams: %i, Previous streams: %lu, Images captured: %lu\r\n", streamCount, streamsServed, imagesServed);
    Serial.printf("Freq: %li MHz\r\n", ESP.getCpuFreqMHz());
    Serial.printf("MCU temperature : %i C, %i F  (approximate)\r\n", McuTc, McuTf);
    Serial.printf("Heap: %li, free: %li, min free: %li, max block: %li\r\n", ESP.getHeapSize(), ESP.getFreeHeap(),
            ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    if (psramFound()) {
        Serial.printf("Psram: %li, free: %li, min free: %li, max block: %li\r\n", ESP.getPsramSize(), ESP.getFreePsram(),
                ESP.getMinFreePsram(), ESP.getMaxAllocPsram());
    } else {
        Serial.printf("Psram: Not found; please check your board configuration.\r\n");
        Serial.printf("- High resolution/quality settings will show incomplete frames to low memory.\r\n");
    }
    // Filesystems
    if (filesystem && (SPIFFS.totalBytes() > 0)) {
        Serial.printf("Spiffs: %i, used: %i\r\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
    } else {
        Serial.printf("Spiffs: No filesystem found, please check your board configuration.\r\n");
        Serial.printf("- Saving and restoring camera settings will not function without this.\r\n");
    }
    Serial.println("Preferences file: ");
    dumpPrefs(SPIFFS);
    if (critERR.length() > 0) {
        Serial.printf("\r\n\r\nA critical error has occurred when initialising Camera Hardware, see startup megssages\r\n");
    }
    Serial.println();
    return;
}

static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    sMillisOfLastAction = millis();
    Serial.println("Capture Requested");
    if (autoLampValue && (lampBrightnessPercentage != -1)) {
        setLamp(lampBrightnessPercentage);
    } else {
        flashLED(75); // little flash of status LED
    }

    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();

    /*
     * Skip complete framebuffer
     */
    for (int i = 0; i < sNumberOfFramebuffer; ++i) {
        esp_camera_fb_return(fb); // dispose the buffered image
        fb = esp_camera_fb_get(); // get fresh image
    }

    if (!fb) {
        Serial.println("CAPTURE: failed to acquire frame");
        httpd_resp_send_500(req);
        if (autoLampValue && (lampBrightnessPercentage != -1)) {
            setLamp(0);
        }
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if (fb->format == PIXFORMAT_JPEG) {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char*) fb->buf, fb->len);
    } else {
        res = ESP_FAIL;
        Serial.println("Capture Error: Non-JPEG image returned by camera module");
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    if (debugData) {
        Serial.printf("JPG: %luB %lums\r\n", (uint32_t) (fb_len), (uint32_t) ((fr_end - fr_start) / 1000));
    }
    imagesServed++;
    if (autoLampValue && (lampBrightnessPercentage != -1))
        setLamp(0);
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[64];

    Serial.println("Stream requested");
    if (autoLampValue && (lampBrightnessPercentage != -1))
        setLamp(lampBrightnessPercentage);
    streamCount = 1; // at present we only have one stream handler, so values are 0 or 1..
    flashLED(75); // double flash of status LED
    delay(75);
    flashLED(75);

    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        streamCount = 0;
        if (autoLampValue && (lampBrightnessPercentage != -1))
            setLamp(0);
        Serial.println("STREAM: failed to set HTTP response type");
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("STREAM: failed to acquire frame");
            res = ESP_FAIL;
        } else {
            if (fb->format != PIXFORMAT_JPEG) {
                Serial.println("STREAM: Non-JPEG frame returned by camera module");
                res = ESP_FAIL;
            } else {
                sLastFrameTime = esp_timer_get_time();
                sFpsFrameCount++;
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf((char*) part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char*) part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char*) _jpg_buf, _jpg_buf_len);
        }
        if (fb) {
            esp_camera_fb_return(fb); // release frame buffer
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK) {
            // This is the only exit point from the stream loop.
            // We end the stream here only if a Hard failure has been encountered or the connection has been interrupted.
            break;
        }
        int64_t frame_time = esp_timer_get_time() - last_frame;
        last_frame = esp_timer_get_time();

        frame_time /= 1000;
        if (debugData) {
            Serial.printf("MJPG: %luB %lums (%.1ffps)\r\n", (uint32_t) (_jpg_buf_len), (uint32_t) frame_time,
                    1000.0 / (uint32_t) frame_time);
        }
        sMillisOfLastAction = millis();
    }

    streamsServed++;
    streamCount = 0;
    if (autoLampValue && (lampBrightnessPercentage != -1))
        setLamp(0);
    Serial.println("Stream ended");
    last_frame = 0;
    return res;
}

/*
 * handles request to change a control value
 */
static esp_err_t cmd_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char tCommandString[32] = { 0, };
    char value[32] = { 0, };

    /*
     * Signal processing of a request
     */
    flashLED(75);
    sMillisOfLastAction = millis();

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*) malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", tCommandString, sizeof(tCommandString)) == ESP_OK
                    && httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;
    if (!strcmp(tCommandString, "framesize")) {
        if (s->pixformat == PIXFORMAT_JPEG)
            res = s->set_framesize(s, (framesize_t) val);
    } else if (!strcmp(tCommandString, "quality"))
        res = s->set_quality(s, val);
    else if (!strcmp(tCommandString, "contrast"))
        res = s->set_contrast(s, val);
    else if (!strcmp(tCommandString, "brightness"))
        res = s->set_brightness(s, val);
    else if (!strcmp(tCommandString, "saturation"))
        res = s->set_saturation(s, val);
    else if (!strcmp(tCommandString, "gainceiling"))
        res = s->set_gainceiling(s, (gainceiling_t) val);
    else if (!strcmp(tCommandString, "colorbar"))
        res = s->set_colorbar(s, val);
    else if (!strcmp(tCommandString, "awb"))
        res = s->set_whitebal(s, val);
    else if (!strcmp(tCommandString, "agc"))
        res = s->set_gain_ctrl(s, val);
    else if (!strcmp(tCommandString, "aec"))
        res = s->set_exposure_ctrl(s, val);
    else if (!strcmp(tCommandString, "hmirror"))
        res = s->set_hmirror(s, val);
    else if (!strcmp(tCommandString, "vflip"))
        res = s->set_vflip(s, val);
    else if (!strcmp(tCommandString, "awb_gain"))
        res = s->set_awb_gain(s, val);
    else if (!strcmp(tCommandString, "agc_gain"))
        res = s->set_agc_gain(s, val);
    else if (!strcmp(tCommandString, "aec_value"))
        res = s->set_aec_value(s, val);
    else if (!strcmp(tCommandString, "aec2"))
        res = s->set_aec2(s, val);
    else if (!strcmp(tCommandString, "dcw"))
        res = s->set_dcw(s, val);
    else if (!strcmp(tCommandString, "bpc"))
        res = s->set_bpc(s, val);
    else if (!strcmp(tCommandString, "wpc"))
        res = s->set_wpc(s, val);
    else if (!strcmp(tCommandString, "raw_gma"))
        res = s->set_raw_gma(s, val);
    else if (!strcmp(tCommandString, "lenc"))
        res = s->set_lenc(s, val);
    else if (!strcmp(tCommandString, "special_effect"))
        res = s->set_special_effect(s, val);
    else if (!strcmp(tCommandString, "wb_mode"))
        res = s->set_wb_mode(s, val);
    else if (!strcmp(tCommandString, "ae_level"))
        res = s->set_ae_level(s, val);
    else if (!strcmp(tCommandString, "rotate"))
        myRotation = val;
    else if (!strcmp(tCommandString, "autolamp") && (lampBrightnessPercentage != -1)) {
        autoLampValue = val;
        if (autoLampValue) {
            if (streamCount > 0)
                setLamp(lampBrightnessPercentage);
            else
                setLamp(0);
        } else {
            setLamp(lampBrightnessPercentage);
        }
    } else if (!strcmp(tCommandString, "lamp") && (lampBrightnessPercentage != -1)) {
        lampBrightnessPercentage = constrain(val, 0, 100);
        if (autoLampValue) {
            if (streamCount > 0)
                setLamp(lampBrightnessPercentage);
            else
                setLamp(0);
        } else {
            setLamp(lampBrightnessPercentage);
        }
    } else if (!strcmp(tCommandString, "save_prefs")) {
        if (filesystem)
            savePrefs(SPIFFS);
    } else if (!strcmp(tCommandString, "clear_prefs")) {
        if (filesystem)
            removePrefs(SPIFFS);
    } else if (!strcmp(tCommandString, "reboot")) {
        // If the TWDT was not initialized automatically on startup, manually intialize it now
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        esp_task_wdt_config_t twdt_config = { .timeout_ms = 3000, .idle_core_mask = 0x03,    // Bitmask of all cores
                .trigger_panic = true, };
        esp_task_wdt_init(&twdt_config);  // schedule a a watchdog panic event for 3 seconds in the future
#else
        esp_task_wdt_init(3, true);  // schedule a a watchdog panic event for 3 seconds in the future
#endif

        esp_task_wdt_add(NULL);
        periph_module_disable(PERIPH_I2C0_MODULE);  // try to shut I2C down properly
        periph_module_disable(PERIPH_I2C1_MODULE);
        periph_module_reset(PERIPH_I2C0_MODULE);
        periph_module_reset(PERIPH_I2C1_MODULE);
        Serial.print("REBOOT requested");
        while (true) {
            flashLED(50);
            delay(150);
            Serial.print('.');
        }
    } else if (ServoAndMotorCommandInterpreter(tCommandString, val)) {
        // do nothing here;
    } else {
        res = -1;
        Serial.print("String=");
        Serial.print(tCommandString);
        Serial.print(" value=");
        Serial.println(val);
    }
    if (res) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

/*
 * request to send Application status to GUI
 */
static esp_err_t status_handler(httpd_req_t *req) {
    Serial.println("\r\nStatus requested via Web");
    Serial.print("LastMotorSpeed=");
    Serial.println(LastMotorSpeed);

    static char json_response[1024];
    sensor_t *s = esp_camera_sensor_get();
    char *p = json_response;
    *p++ = '{';
    if (lampBrightnessPercentage != -1) {
        p += sprintf(p, "\"lamp\":%d,", lampBrightnessPercentage);
        p += sprintf(p, "\"autolamp\":%d,", autoLampValue);
    }
    if (sPanServoIsSupported) {
        p += sprintf(p, "\"pan\":%d,", ServoPanDegree);
    }
    if (sOnePWMMotorIsSupported) {
        p += sprintf(p, "\"motor-speed\":%d,", LastMotorSpeed);
    }
    p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p += sprintf(p, "\"quality\":%u,", s->status.quality);
    p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p += sprintf(p, "\"awb\":%u,", s->status.awb);
    p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p += sprintf(p, "\"aec\":%u,", s->status.aec);
    p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p += sprintf(p, "\"agc\":%u,", s->status.agc);
    p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p += sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
    p += sprintf(p, "\"cam_name\":\"%s\",", sApplicationName);
    p += sprintf(p, "\"code_ver\":\"%s\",", sCompileTimestamp);
    p += sprintf(p, "\"rotate\":\"%d\",", myRotation);
    p += sprintf(p, "\"rssi\":\"%d\",", WiFi.RSSI());
    p += sprintf(p, "\"stream_url\":\"%s\"", streamURL);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t fps_info_handler(httpd_req_t *req) {
    float tFPS = 0.0;
    Serial.print("FrameCount=");
    Serial.print(sFpsFrameCount);
    uint32_t tDeltaMillis = (sLastFrameTime - sFirstFrameTime) / 1000;
    if (sFpsFrameCount > 0) {
        tFPS = (1000.0 * sFpsFrameCount) / tDeltaMillis;
        // set for new measurement
        sFirstFrameTime = sLastFrameTime; // set time for first
        sFpsFrameCount = 0;
    }

    Serial.print(" FPS=");
    Serial.println(tFPS);

    static char json_response[64];
    char *p = json_response;
    *p++ = '{';
    p += sprintf(p, "\"fps\":\"%.1f\"", tFPS);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t info_handler(httpd_req_t *req) {
    static char json_response[256];
    char *p = json_response;
    *p++ = '{';
    p += sprintf(p, "\"cam_name\":\"%s\",", sApplicationName);
    p += sprintf(p, "\"rotate\":\"%d\",", myRotation);
    p += sprintf(p, "\"stream_url\":\"%s\"", streamURL);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t favicon_16x16_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) favicon_16x16_png, favicon_16x16_png_len);
}

static esp_err_t favicon_32x32_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) favicon_32x32_png, favicon_32x32_png_len);
}

static esp_err_t favicon_ico_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) favicon_ico, favicon_ico_len);
}

static esp_err_t logo_svg_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) logo_svg, logo_svg_len);
}

static esp_err_t dump_handler(httpd_req_t *req) {
    flashLED(75);
    Serial.println("\r\nDump Requested via Web");
    serialDump();
    static char dumpOut[2000] = "";
    char *d = dumpOut;
// Header
    d += sprintf(d, "<html><head><meta charset=\"utf-8\">\n");
    d += sprintf(d, "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n");
    d += sprintf(d, "<title>%s - Status</title>\n", sApplicationName);
    d += sprintf(d, "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"/favicon-32x32.png\">\n");
    d += sprintf(d, "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"/favicon-16x16.png\">\n");
    d += sprintf(d, "<link rel=\"stylesheet\" type=\"text/css\" href=\"/style.css\">\n");
    d += sprintf(d, "</head>\n");
    d += sprintf(d, "<body>\n");
    d += sprintf(d, "<img src=\"/logo.svg\" style=\"position: relative; float: right;\">\n");
    if (critERR.length() > 0) {
        d += sprintf(d, "<span style=\"color:red;\">%s<hr></span>\n", critERR.c_str());
        d += sprintf(d, "<h2 style=\"color:red;\">(the serial log may give more information)</h2><br>\n");
    }
    d += sprintf(d, "<h1>ESP32 Cam Webserver</h1>\n");
// Module
    d += sprintf(d, "Name: %s<br>\n", sApplicationName);
    d += sprintf(d, "Firmware: %s (base: %s)<br>\n", sCompileTimestamp, sExampleVersion);
    float sketchPct = 100 * sketchSize / sketchSpace;
    d += sprintf(d, "Sketch Size: %i (total: %i, %.1f%% used)<br>\n", sketchSize, sketchSpace, sketchPct);
    d += sprintf(d, "MD5: %s<br>\n", sketchMD5.c_str());
    d += sprintf(d, "ESP sdk: %s<br>\n", ESP.getSdkVersion());
// Network
    d += sprintf(d, "<h2>WiFi</h2>\n");
    if (sInAccesspointMode) {
        if (sCaptivePortalEnabled) {
            d += sprintf(d, "Mode: AccessPoint with captive portal<br>\n");
        } else {
            d += sprintf(d, "Mode: AccessPoint<br>\n");
        }
        d += sprintf(d, "SSID: %s<br>\n", apName);
    } else {
        d += sprintf(d, "Mode: Client<br>\n");
        String ssidName = WiFi.SSID();
        d += sprintf(d, "SSID: %s<br>\n", ssidName.c_str());
        d += sprintf(d, "Rssi: %i<br>\n", WiFi.RSSI());
        String bssid = WiFi.BSSIDstr();
        d += sprintf(d, "BSSID: %s<br>\n", bssid.c_str());
    }
    d += sprintf(d, "IP address: %d.%d.%d.%d<br>\n", ip[0], ip[1], ip[2], ip[3]);
    if (!sInAccesspointMode) {
        d += sprintf(d, "Netmask: %d.%d.%d.%d<br>\n", net[0], net[1], net[2], net[3]);
        d += sprintf(d, "Gateway: %d.%d.%d.%d<br>\n", gw[0], gw[1], gw[2], gw[3]);
    }
    d += sprintf(d, "Http port: %i, Stream port: %i<br>\n", httpPort, streamPort);
    byte mac[6];
    WiFi.macAddress(mac);
    d += sprintf(d, "MAC: %02X:%02X:%02X:%02X:%02X:%02X<br>\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

// System
    d += sprintf(d, "<h2>System</h2>\n");
    if (haveTime) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStringBuff[50]; //50 chars should be enough
            strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S, %A, %B %d %Y", &timeinfo);
            //print like "const char*"
            d += sprintf(d, "Time: %s<br>\n", timeStringBuff);
        }
    }
    int64_t sec = esp_timer_get_time() / 1000000;
    int64_t upDays = int64_t(floor(sec / 86400));
    int upHours = int64_t(floor(sec / 3600)) % 24;
    int upMin = int64_t(floor(sec / 60)) % 60;
    int upSec = sec % 60;
    int McuTc = (temprature_sens_read() - 32) / 1.8; // celsius
    int McuTf = temprature_sens_read(); // fahrenheit

    d += sprintf(d, "Up: %" PRId64 ":%02i:%02i:%02i (d:h:m:s)<br>\n", upDays, upHours, upMin, upSec);
    d += sprintf(d, "Active streams: %i, Previous streams: %lu, Images captured: %lu<br>\n", streamCount, streamsServed,
            imagesServed);
    d += sprintf(d, "Freq: %li MHz<br>\n", ESP.getCpuFreqMHz());
    d +=
            sprintf(d,
                    "<span title=\"NOTE: Internal temperature sensor readings can be innacurate on the ESP32-c1 chipset, and may vary significantly between devices!\">");
    d += sprintf(d, "MCU temperature : %i &deg;C, %i &deg;F</span>\n<br>", McuTc, McuTf);
    d += sprintf(d, "Heap: %li, free: %li, min free: %li, max block: %li<br>\n", ESP.getHeapSize(), ESP.getFreeHeap(),
            ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
    if (psramFound()) {
        d += sprintf(d, "Psram: %li, free: %li, min free: %li, max block: %li<br>\n", ESP.getPsramSize(), ESP.getFreePsram(),
                ESP.getMinFreePsram(), ESP.getMaxAllocPsram());
    } else {
        d += sprintf(d, "Psram: <span style=\"color:red;\">Not found</span>, please check your board configuration.<br>\n");
        d += sprintf(d, "- High resolution/quality images & streams will show incomplete frames due to low memory.<br>\n");
    }
    if (filesystem && (SPIFFS.totalBytes() > 0)) {
        d += sprintf(d, "Spiffs: %i, used: %i<br>\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
    } else {
        d += sprintf(d,
                "Spiffs: <span style=\"color:red;\">No filesystem found</span>, please check your board configuration.<br>\n");
        d += sprintf(d, "- saving and restoring camera settings will not function without this.<br>\n");
    }

// Footer
    d += sprintf(d, "<br><div class=\"input-group\">\n");
    d +=
            sprintf(d,
                    "<button title=\"Instant Refresh; the page reloads every minute anyway\" onclick=\"location.replace(document.URL)\">Refresh</button>\n");
    d += sprintf(d, "<button title=\"Close this page\" onclick=\"javascript:window.close()\">Close</button>\n");
    d += sprintf(d, "</div>\n</body>\n");
// A javascript timer to refresh the page every minute.
    d += sprintf(d, "<script>\nsetTimeout(function(){\nlocation.replace(document.URL);\n}, 60000);\n");
    d += sprintf(d, "</script>\n</html>\n");
    *d++ = 0;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, dumpOut, strlen(dumpOut));
}

static esp_err_t style_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) style_css, style_css_len);
}

static esp_err_t streamviewer_handler(httpd_req_t *req) {
    flashLED(75);
    Serial.println("Stream Viewer requested");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) streamviewer_html, streamviewer_html_len);
}

static esp_err_t error_handler(httpd_req_t *req) {
    flashLED(75);
    Serial.println("Sending Error page");
    std::string s(error_html);
    size_t index;
    while ((index = s.find("<APPURL>")) != std::string::npos)
        s.replace(index, strlen("<APPURL>"), httpURL);
    while ((index = s.find("<CAMNAME>")) != std::string::npos)
        s.replace(index, strlen("<CAMNAME>"), sApplicationName);
    while ((index = s.find("<ERRORTEXT>")) != std::string::npos)
        s.replace(index, strlen("<ERRORTEXT>"), critERR.c_str());
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, (const char*) s.c_str(), s.length());
}

static esp_err_t index_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    char view[32] = { 0, };

    flashLED(75);
// See if we have a specific target (full/simple/portal) and serve as appropriate
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*) malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "view", view, sizeof(view)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        // no target specified; default.
        strcpy(view, default_index);
        // If captive portal is active send that instead
        if (sCaptivePortalEnabled) {
            strcpy(view, "portal");
        }
    }

    if (strncmp(view, "simple", sizeof(view)) == 0) {
        Serial.println("Simple index page requested");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        return httpd_resp_send(req, (const char*) index_simple_html, index_simple_html_len);
    } else if (strncmp(view, "full", sizeof(view)) == 0) {
        Serial.println("Full index page requested");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        sensor_t *s = esp_camera_sensor_get();
        if (s->id.PID == OV3660_PID) {
            return httpd_resp_send(req, (const char*) index_ov3660_html, index_ov3660_html_len);
        }
        return httpd_resp_send(req, (const char*) index_ov2640_html, index_ov2640_html_len);
    } else if (strncmp(view, "portal", sizeof(view)) == 0) {
        //Prototype captive portal landing page.
        Serial.println("Portal page requested");
        std::string s(portal_html);
        size_t index;
        while ((index = s.find("<APPURL>")) != std::string::npos)
            s.replace(index, strlen("<APPURL>"), httpURL);
        while ((index = s.find("<STREAMURL>")) != std::string::npos)
            s.replace(index, strlen("<STREAMURL>"), streamURL);
        while ((index = s.find("<CAMNAME>")) != std::string::npos)
            s.replace(index, strlen("<CAMNAME>"), sApplicationName);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        return httpd_resp_send(req, (const char*) s.c_str(), s.length());
    } else {
        Serial.print("Unknown page requested: ");
        Serial.println(view);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
}

void startCameraServer(int hPort, int sPort) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12; // we use more than the default 8 (on port 80)

    httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL, .is_websocket =
            false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t cmd_uri = { .uri = "/control", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t capture_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL, .is_websocket =
            false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t style_uri = { .uri = "/style.css", .method = HTTP_GET, .handler = style_handler, .user_ctx = NULL, .is_websocket =
            false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t favicon_16x16_uri = { .uri = "/favicon-16x16.png", .method = HTTP_GET, .handler = favicon_16x16_handler, .user_ctx =
    NULL, .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t favicon_32x32_uri = { .uri = "/favicon-32x32.png", .method = HTTP_GET, .handler = favicon_32x32_handler, .user_ctx =
    NULL, .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t favicon_ico_uri = { .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_ico_handler, .user_ctx = NULL,
            .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t logo_svg_uri = { .uri = "/logo.svg", .method = HTTP_GET, .handler = logo_svg_handler, .user_ctx = NULL,
            .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t dump_uri = { .uri = "/dump", .method = HTTP_GET, .handler = dump_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t stream_uri = { .uri = "/", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t streamviewer_uri = { .uri = "/view", .method = HTTP_GET, .handler = streamviewer_handler, .user_ctx = NULL,
            .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t fps_info_uri = { .uri = "/fps_info", .method = HTTP_GET, .handler = fps_info_handler, .user_ctx = NULL,
            .is_websocket = false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t info_uri = { .uri = "/info", .method = HTTP_GET, .handler = info_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t error_uri = { .uri = "/", .method = HTTP_GET, .handler = error_handler, .user_ctx = NULL, .is_websocket = false,
            .handle_ws_control_frames = false, .supported_subprotocol = NULL };
    httpd_uri_t viewerror_uri = { .uri = "/view", .method = HTTP_GET, .handler = error_handler, .user_ctx = NULL, .is_websocket =
            false, .handle_ws_control_frames = false, .supported_subprotocol = NULL };

// Request Handlers; config.max_uri_handlers (above) must be >= the number of handlers
    config.server_port = hPort;
    config.ctrl_port = hPort;
    Serial.printf("Starting web server on port: '%d'\r\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        if (critERR.length() > 0) {
            httpd_register_uri_handler(camera_httpd, &error_uri);
        } else {
            httpd_register_uri_handler(camera_httpd, &index_uri);
            httpd_register_uri_handler(camera_httpd, &cmd_uri);
            httpd_register_uri_handler(camera_httpd, &status_uri);
            httpd_register_uri_handler(camera_httpd, &fps_info_uri);
            httpd_register_uri_handler(camera_httpd, &capture_uri);
        }
        httpd_register_uri_handler(camera_httpd, &style_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_16x16_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_32x32_uri);
        httpd_register_uri_handler(camera_httpd, &favicon_ico_uri);
        httpd_register_uri_handler(camera_httpd, &logo_svg_uri);
        httpd_register_uri_handler(camera_httpd, &dump_uri);
    }

    config.server_port = sPort;
    config.ctrl_port = sPort;
    Serial.printf("Starting stream server on port: '%d'\r\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        if (critERR.length() > 0) {
            httpd_register_uri_handler(camera_httpd, &error_uri);
            httpd_register_uri_handler(camera_httpd, &viewerror_uri);
        } else {
            httpd_register_uri_handler(stream_httpd, &stream_uri);
            httpd_register_uri_handler(stream_httpd, &info_uri);
            httpd_register_uri_handler(stream_httpd, &streamviewer_uri);
        }
        httpd_register_uri_handler(stream_httpd, &favicon_16x16_uri);
        httpd_register_uri_handler(stream_httpd, &favicon_32x32_uri);
        httpd_register_uri_handler(stream_httpd, &favicon_ico_uri);
    }
}
