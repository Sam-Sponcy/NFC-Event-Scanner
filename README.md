# NFC-Event-Scanner
This project enables an Arduino-based NFC event scanner that interacts with a web application over WiFi. It reads NFC tags, sends their IDs to a server, and receives responses to control RGB LEDs and a buzzer.

![IMG_9150](https://github.com/Sam-Sponcy/NFC-Event-Scanner/assets/93118296/4aae0c81-3671-4d49-bdbf-9e628063a435)

- Features

NFC Reading: Reads NFC tags using the Adafruit PN532 NFC module.
WiFi Connectivity: Connects to a WiFi network using the WiFiNINA library.
Server Communication: Sends and receives JSON data to/from a server over HTTPS.
Dynamic Mode Control: Switches between event modes using a button interrupt.

- Requirements
 
Arduino board with WiFi capability (e.g., Arduino Nano 33 IoT)
Adafruit PN532 NFC breakout board
WiFiNINA library for Arduino
ArduinoJson library for JSON parsing
LiquidCrystal library for controlling LCD displays


Installation

- Hardware Setup:
  
Connect the PN532 NFC module and RGB LEDs to specified pins on your Arduino board.
Ensure proper wiring and power supply for all components.

- Software Setup:
  
Install the necessary libraries mentioned in the #include section of the code.
Set up your Arduino IDE to include these libraries and upload the code to your board.


- Configuration:

Edit arduino_secrets.h to include your WiFi credentials (SECRET_SSID and SECRET_PASS).
Modify IPADDRESS_SERVER, HOST, PORT, SCAN_URL, INIT_URL, and TOKEN_POST according to your server configuration.
Usage
Upload the code to your Arduino board using the Arduino IDE.
Open the Serial Monitor to view debug information (baud rate: 9600).
Scan NFC tags to trigger events and observe responses on the connected RGB LEDs and buzzer.
