# Arduino RTU w/ Flask Poller

This is a simple Arduino RTU (Remote Telemetry Unit) that can be polled using a Flask Server and Ethernet, the Arduino and Flask server also use the DCP protocal to verify the packets recieved and sent. The server also catches any standing alarms. On the device itself you may change the IP, Subnet, Gateway, Delete Temperature History. There was also a history of temperature saved to the EEPROM on the arduino.  


# Parts
```
Arduino Uno
DHT22
RTC DS3231
LCD w/ I2C Backpack
Analog 5 Button Keypad
WIZnet W5500 Ethernet
RGB LED (Strip of 3)
```
