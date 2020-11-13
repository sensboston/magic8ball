# "Magic 8 Ball" toy project
Arduino (ESP32-based) project for digital implementation of "Magic 8 ball" toy

![Ball](/images/ball.jpg)

## Features
This digital toy project offers many nice features unlike similar ones:
* multiple languages support
* synthetic voices
* realistic (or "realistic" :wink: ) animations
* gesture-based user interface
* "deep sleep" functionality (for better power consumption)

## Parts
Name | Link
------------ | -------------
**ESP32 development board** | [eBay](https://www.ebay.com/itm/Wireless-module-NodeMcu-v3-v2-ESP8266-D1MINI-Lua-WIFI-development-board-Rs4/283956013684?ssPageName=STRK%3AMEBIDX%3AIT)
**TFT IPS Display 1.3'' 240x240** | [Amazon](https://www.amazon.com/gp/product/B088CQ4GPT/)
**DFPlayer Mini**| [eBay](https://www.ebay.com/itm/Useful-Mini-Mp3-Player-Module-DFPlayer-Micro-SD-TF-U-disk-for-Arduino-US/152513919098)
**16GB micro sdcard**| [Amazon](https://www.amazon.com/PNY-Performance-microSD-Memory-P-SDU16G4X5-MP/dp/B083VMR3PL/)
**MPU-6050 Accelerometer & Gyroscope Sensor** | [Amazon](https://www.amazon.com/Ximimark-MPU-6050-Accelerometer-Gyroscope-Converter/dp/B07M98PKT4)
**AAA battery holder** | [AliExpress](https://www.aliexpress.com/item/32719302709.html?spm=a2g0s.9042311.0.0.27424c4dsAHXA0)
**Short micro USB cable** | [eBay](https://www.ebay.com/itm/1M-Micro-USB-Extension-Charging-Data-Cable-Charger-Type-A-Male-To-Female/302277483939)
**Push button** | [Amazon](https://www.amazon.com/OFNMY-Self-Locking-Latching-Button-Switch/dp/B07NX7S9VV/)
**Original "Magic 8 ball" toy (for case)** | [Amazon](https://www.amazon.com/Mattel-Games-Magic-Ball-Retro/dp/B0149MC426/)

_Note: I believe, you can find better parts pricing on Ali/eBay etc. And I already had many of these parts before starting this project._

## Circuit
![Circuit](/images/Magic8Ball_bb.png)
### Zoomed ESP32 <-> Display connection (please note, different display type might require different connection scheme)
![Display](/images/ESP32_disp.png)
Also, here is [YouTube video](https://www.youtube.com/watch?v=HoZhgNcJjNA) for connecting ST7789-based displays to ESP32

_Note: you may want to add some resitors (for pulldown serial communication) or capacitors (for VCC). However in my case everything works "as is" :wink:_

## Building Arduino sketch
You should add a few extra libraries to Arduino IDE to build this project (see project includes)

For voice files generation I provide a small console utility **GenerateVoices**. Build it using VisualStudio (or download from Release section), and run from command prompt:

`
GenerateVoices.exe [full_path_to_Arduino_sketch]`

i.e.

`GenerateVoices.exe C:\repos\magic8ball\magic8ball.ino
`

then copy generated mp3 files to the root of sdcard.
