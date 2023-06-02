# CIS 350 Final Project: Security Lock System

## Description
Safe and secure home entry without the hassle of keys requires the development of a deadbolt whose locking and unlocking is controlled via PIN entry. The convenience of personalized PIN numbers for all users and the possibility of control through a mobile app would make home security easy.

The objective of the product is to provide convenience without sacrificing security. The scope of the proposed solution includes a deadbolt control mechanism, method of confirming user identity via personal identification number, and possible development of wireless connectivity to provide remote access to the lock. The code relating to the mobile application development of the project can be viewed [here](https://github.com/CIS-350/lock_app).

This project is an internet of things (IoT) deadbolt. Containing a LCD display, keypad, and mobile application for additional features. This will allow the user to enter the pin through the onboard keyboard or send the pin through wifi using Message Queuing Telemetry Transport (MQTT). Allowing the user to unlock the deadbolt from anywhere in the world along with entering the pin on the device. 


## Current State
https://github.com/CIS-350/lock/assets/107201610/6131cbc2-6563-4f6d-a5b9-eebca7f5f955


## System Hardware
Hardware used:
- [ESP32S Development Kit](https://www.amazon.com/dp/B09J95SMG7?psc=1&ref=ppx_yo2ov_dt_b_product_details)
- [Adafruit 3x4 Matrix Keypad](https://www.amazon.com/dp/B00QSHPCO8?psc=1&ref=ppx_yo2ov_dt_b_product_details)
- [1604A 16x4 LCD Display w/HD44780](https://www.just4funelectronics.com/product-page/16x4-lcd-display)
- [FS90 180° Continuous Servo](https://www.just4funelectronics.com/product-page/fitech-fs90-120-degree-servo-motor)


## IDE Setup for Development
1. Follow https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html when installing the idf tools install the 5.0.2 offline installer.

2. Add the espressif idf extension to VScode.

3. Press control+shift+p and search esp-idf: welcome

4. On this page select configure extension and select use existing extension.

5. Use the default settings.

6. Next clone the repository to your system.

7. In VScode, click the gear box on the bottom left of the window's toolbar.
    - Select "Example Connection Configuration"
    - Verify that "connect using WiFi interface" and "Provide wifi connect commands" are checked 
    - Type in the name of your wifi or hotspot 
    - Type in the password 
    - Click "save" NOTE: if save is in white then the changes haven't been saved. 
  
8. On the bottom toolbar, click on the flame icon to build, flash, and monitor.
    - Important: Press and hold onto the IOO button on the ESP module 
