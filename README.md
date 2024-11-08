<div align = center>
# [ESP32-Cam Sewer inspection car](https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car)

A simple and small robot car using the well known yellow gear motors and wheels.
It carries an ESP32-Cam monted on a pan servo.
I used it to inspect my sewer pipes under the basement and up to the curb.

Based on [esp32-cam-webserver](https://github.com/easytarget/esp32-cam-webserver/tree/master/src) by Owen Carter.

[![Badge License: GPLv3](https://img.shields.io/badge/License-GPLv3-brightgreen.svg)](https://www.gnu.org/licenses/gpl-3.0)
 &nbsp; &nbsp;
 [![Badge Version](https://img.shields.io/github/v/release/ArminJo/ESP32-Cam-Sewer-inspection-car?include_prereleases&color=yellow&logo=DocuSign&logoColor=white)](https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car/releases/latest)
 &nbsp; &nbsp;
[![Badge Commits since latest](https://img.shields.io/github/commits-since/ArminJo/ESP32-Cam-Sewer-inspection-car/latest?color=yellow)](https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car/commits/master)
 &nbsp; &nbsp;
[![Badge Build Status](https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car/workflows/TestCompile/badge.svg)](https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car/actions)
 &nbsp; &nbsp;
![Badge Hit Counter](https://visitor-badge.laobi.icu/badge?page_id=ArminJo_ESP32-Cam-Sewer-inspection-car)
<br/>
<br/>
[![Stand With Ukraine](https://raw.githubusercontent.com/vshymanskyy/StandWithUkraine/main/badges/StandWithUkraine.svg)](https://stand-with-ukraine.pp.ua)
</div>

#### If you find this library useful, please give it a star.

&#x1F30E; [Google Translate](https://translate.google.com/translate?sl=en&u=https://github.com/ArminJo/ESP32-Cam-Sewer-inspection-car)

#### YouTube video of inspection car in action

[![Inspection car in action](https://i.ytimg.com/vi/aUz2TU6FpmA/hqdefault.jpg)](https://www.youtube.com/watch?v=aUz2TU6FpmA)

# Additional Features
- Support a pan servo for the ESP32-Cam.
- Support motor control with adjustable speed / power and going fixed distances.
- Support of missing PSRAM - with restricted resolution / quality.
- Display of fps stream rate in Browser.
- Improved documentation.

### Features of Owen Carters version
- More options for default network and camera settings
- Save and restore settings
- Control of on-board lamps, rotate the view in the browser
- Dedicated standalone stream viewer
- Over The Air firmware updates
- Lots of minor fixes and tweaks, documentation etc.
- And 'reduced' by removing the Face Recognition features


# Pictures of the robot car
| | |
|-|-|
| ![The car](pictures/Complete.jpg) | ![Front detail](pictures/FrontDetail.jpg) |
| ![Top view](pictures/Top.jpg) | ![Front detail](pictures/2Wheels.jpg) |
| ![Front right](pictures/FrontRight.jpg) | ![Front left](pictures/FrontLeft.jpg) |
| ![Front carrier](pictures/FrontCarrier.jpg) | ![MosFet bridge](pictures/MosFetBridge.jpg) |
| Simple carrier glued to the servo | Connections at the TB6612FNG MosFet bridge breakout board |
| ![3.3V LDO](pictures/TS2940Detail.jpg) | ![Details of the antenna](pictures/BackWithAntenna.jpg) |
| Details of the 3.3 volt LDO regulator assembly | Details of the antenna (from an old laptop) |

# Pictures of my sewer inspection
| | |
|-|-|
| ![The situation](pictures/TheSituation.jpg) | ![Gravel](pictures/TheReason.jpg) |
| The situation | The reason for this project (gravel in the sewer) |
| ![The discovery](pictures/JunctionWithTennissball.jpg) | ![After cleaning](pictures/JunctionWithoutTennisball.jpg) |
| The discovery | After cleaning |
| ![Impression](pictures/Impression.jpg) | ![Impression1](pictures/Impression1.jpg) |
| Impressions | Impressions |
| ![End of the 125 mm sewer](pictures/EndOf125mm.jpg) | ![End of the WiFi range outside](pictures/2_6m_Outside.jpg) |
| End of the 125 mm sewer after 6 m | End of the WiFi range outside after 2.6 m |

# Revision History
### Version 1.0.0
- ESP32 core 3.x support.
- Support for missing PSRAM.
- Removed annoying buffering of stills.
- Browser display of fps of running stream.

### Version 0.9.0
- Initial version.
