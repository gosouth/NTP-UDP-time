# NTP-UDP-time

Pulls NTP time form internet (UDP) and displays it on a 4x7 segment display.
Hardware used are Arduino mini-pro and ESP8266 

* Arduino mini-pro controlling ESP8266 by naive AT command
* Multiplexing a 4 digit 7 segment display in 20ms Timer 
* ESP8266 AT controlled from Arduino
* UDP register and retrieve time from time-a.nist.gov

