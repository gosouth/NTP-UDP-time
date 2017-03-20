/**********************************************************************

Get NTP time for Patagonia (or any place...)

(c) 2017 gosouth.cl, Ricardo Timmermann

// http://support.ntp.org/bin/view/Servers/PublicTimeServer000277
// http://support.ntp.org/bin/view/Main/DocumentationIndex
// https://www.eecis.udel.edu/~mills/time.html#intro
 
Note: SoftwareSerial will not work for HIGHER BAUDRATES, so reprogram ESP8266 for 9600 baud.
      This is done simple trough AT commandos (AT+CIOBAUD=9600) and using a simple terminal 
      and the 3.3V serial cable: IoT - First Test of ESP8266 WiFi Module · Hannes Lehmann  

    TODO:
    
    * Jumper to IO13 for summer/winter time. Chile has no clear date to switch. Depends of politicians. 
    * Nicer code with arrays for I/O pins
 
**********************************************************************/

//  #define SERIAL_RX_BUFFER_SIZE 256
//  #define SERIAL_TX_BUFFER_SIZE 128

#include "ESP8266.h"
#include "TimerOne.h"

#define SSID            "Quilaco"
#define PASSWORD        ""
#define HOST_NAME       "129.6.15.28"   // time-a.nist.gov, 
#define HOST_PORT       (123) 
#define MY_TIMEZONE     (3600*3)        // chilean time zone


#define TIME_SCAN (10*60000)            // read TIME each 10 minutes
#define MINUTE 59500                    // arduino millis in one minute, not exact

SoftwareSerial ESPserial(2, 3);     // RX | TX
ESP8266 wifi(ESPserial);            // wifi initializes SoftwareSerial()

#define NTP_PACKET_SIZE 48          // 48
byte NTPpacket[ NTP_PACKET_SIZE];

// == diplay defines =================================================

int digit1 = 7; //PWM Display pin 1
int digit2 = 6; //PWM Display pin 2
int digit3 = 5; //PWM Display pin 6
int digit4 = 4; //PWM Display pin 8

int segA = 8; //Display pin 14
int segB = 9; //Display pin 16    
int segC = 10; //Display pin 13
int segD = 11; //Display pin 3
int segE = 12; //Display pin 5
int segF = A2; //Display pin 11
int segG = A0; //Display pin 15

#define DP A1       // decimal point

#define DIPSW_SUMMERTIME 13

void getNTPTime(void);
unsigned long epoch;

bool noDisturb;

volatile unsigned long tm, minits, NTP_scan;

int brightness = 100;

bool dpSecond = false;

// == interrupt settings ====================================================

void displayTime() 
{

  //  Serial.println( epoch );
    if( noDisturb ) return;
    
    // == display the hour: minute  =======================
     
    int hour = (epoch  % 86400L) / 3600;
    int minutes = (epoch  % 3600) / 60;
    displayNumber( hour * 100 + minutes ); 
}

// == setup ==========================================================
//
//  * Connect ESP8266 wifi and setup in single TCP/UDP
//  * Build NTP time request packet

void setup(void)
{
    Serial.begin(9600);
    delay(1000);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // == display =========================

    pinMode( segA, OUTPUT );
    pinMode( segB, OUTPUT );
    pinMode( segC, OUTPUT );
    pinMode( segD, OUTPUT );
    pinMode( segE, OUTPUT );
    pinMode( segF, OUTPUT );
    pinMode( segG, OUTPUT );
    pinMode( DP, OUTPUT );
    
    pinMode( digit1, OUTPUT );
    pinMode( digit2, OUTPUT );
    pinMode( digit3, OUTPUT );
    pinMode( digit4, OUTPUT );
    
    pinMode( DIPSW_SUMMERTIME, INPUT_PULLUP );
    
    Serial.println("== SETUP START ========\n\r");

    Serial.println("FW Version:");
    Serial.println(wifi.getVersion().c_str());

    if (wifi.setOprToStationSoftAP()) Serial.print("to station + softap ok\r\n");
    else Serial.print("to station + softap err\r\n");

    // == connect to WLAN ============================
     
    if (wifi.joinAP(SSID, PASSWORD)) {
        Serial.print("IP:");
        Serial.println( wifi.getLocalIP().c_str());     
        } 
    else Serial.print("Join AP failure\r\n");

    // == disable MUX, only one TCP or UDP communication can be builded 
    
    if (wifi.disableMUX()) Serial.println("single ok\r\n");
    else Serial.println("single err\r\n");

    // == build NTP time request packet ==============================
    // http://www.networksorcery.com/enp/protocol/ntp.htm#Poll_interval
    
    memset(NTPpacket, 0, NTP_PACKET_SIZE);
    
    NTPpacket[0] = 0b11100011;   // LI, Version, Mode
    NTPpacket[1] = 0;            // Stratum, or type of clock
    NTPpacket[2] = 6;            // Polling Interval
    NTPpacket[3] = 0xEC;         // Peer Clock Precision
    
    // .. 8 bytes of zero for Root Delay & Root Dispersion
    
    NTPpacket[12]  = 49;
    NTPpacket[13]  = 0x4E;
    NTPpacket[14]  = 49;
    NTPpacket[15]  = 52;
 
    Serial.println("== SETUP END =========\n\r");
    
    // == timer for display multiplexing ============

    noDisturb = false;
    
    Timer1.initialize( 20000 );                 // 20 ms
    Timer1.attachInterrupt( displayTime );
    
    tm = millis();
    NTP_scan = TIME_SCAN/20;
}

// == main loop ============================================================
//
//
//  NTP allows open access for up to 20 queries per hour, I use 1 / minute = 60 

void loop(void)
{

    // == call NTP Time nly each 10 mins, needed only to synchronize my time.
/*
    Serial.print( tm );
    Serial.print( " - " );
    Serial.println( millis() );


    String s = wifi.getLocalIP();
    Serial.println( s );    
    
  */        

    
    // call only if we have wifi
    
   if( millis() - tm > NTP_scan ) { 
        tm = millis();
        getNTPTime();
    }
    
    // == increment minute in epoch, for when NPT is not called. Adjust clock around 60000 
    
    if( millis() - minits > MINUTE ) { 
        epoch += 60;                        
        minits = millis();
    }

    brightness = 1024 - analogRead(A3);
    
  //  Serial.println( brightness );
    brightness = max( 10, brightness );
    
    dpSecond = !dpSecond;
          
    delay( 1000 );
        
/*
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
 
    Serial.print( hour );       // print the hour (86400 secs / day)
    
    // print minutes
    Serial.print(':');
    if (minutes) 
        Serial.print('0');
    Serial.print( minutes );     // print the minute (3600  secs / minute)
    
    // print seconds
    Serial.print(':');
    if ((epoch % 60) < 10) Serial.print('0');  
    Serial.print(epoch % 60); 
  
    Serial.print("  =  ");
    Serial.println( hour * 100 + minutes );
*/

}

// == getNTPTime =======================================================
//
//  get NTP time from a UTP host
//

#define SECS_70YEARS 2208988800UL

void getNTPTime()
{
    
    noDisturb = true;                               // disable interrupt
    
    uint8_t buffer[NTP_PACKET_SIZE] = {0};          // uint8_t

    if (wifi.registerUDP(HOST_NAME, HOST_PORT)) Serial.print("register UDP ok\r\n");
    else { Serial.print("register UDP err\r\n"); return; }
  
    // == sent NTP time request packet 
    wifi.send( NTPpacket, NTP_PACKET_SIZE );
        
    // == read from time server
    uint32_t len = wifi.recv(buffer, NTP_PACKET_SIZE, 2000);

    Serial.print("NTP_PACKET_SIZE = ");
    Serial.println(len);
        
    if( len == NTP_PACKET_SIZE ) {
            
        NTP_scan = TIME_SCAN;
        
        digitalWrite(LED_BUILTIN, HIGH);
             
        // the timestamp starts at byte 40 and is 4 byte long
        unsigned long highWord = word(buffer[40], buffer[41]);
        unsigned long lowWord = word(buffer[42], buffer[43]);
        
        // join the two words into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        
        Serial.print("Seconds since Jan 1 1900 = ");
        Serial.println(secsSince1900);
        
        // now convert NTP time into Unix time 
        // Unix time starts on Jan 1 1970
        // https://www.epochconverter.com
   
        epoch = secsSince1900 - SECS_70YEARS;      
        epoch -= MY_TIMEZONE;
        if( digitalRead( DIPSW_SUMMERTIME) )
            epoch -= 3600;
    }
    
    else {
        Serial.println("wrong NTP_PACKET_SIZE");
    }
    
    if (wifi.unregisterUDP()) Serial.print("unregister udp ok\r\n");
    else Serial.print("unregister udp err\r\n");

    digitalWrite(LED_BUILTIN, LOW);
    noDisturb = false;

}

/*-----------------------------------------------------------------------
;
;   Display number on 4x7 Segment display, SW multiplexed
;
;-----------------------------------------------------------------------*/

void displayNumber( int toDisplay ) {
    
#define DISPLAY_BRIGHTNESS  400

#define DIGIT_ON  HIGH
#define DIGIT_OFF  LOW

    long beginTime = millis();
    
    for( int digit=4 ; digit>0 ; digit-- ) {

        digitalWrite(DP, LOW);
            
        //Turn on a digit for a short amount of time
        switch(digit) {
            case 1:
              digitalWrite(digit1, DIGIT_ON);
              break;
            case 2:
              digitalWrite(digit2, DIGIT_ON);
              if( dpSecond ) digitalWrite(DP, HIGH);
              break;
            case 3:
              digitalWrite(digit3, DIGIT_ON);
              break;
            case 4:
              digitalWrite(digit4, DIGIT_ON);
              break;
            }

        //Turn on the right segments for this digit
        showDigit(toDisplay % 10);
        toDisplay /= 10;

        //Display digit for fraction of a second (1us to 5000us, 500 is pretty good)
        delayMicroseconds( brightness ); 
  
        //Turn off all segments
        showDigit(10); 

        //Turn off all digits
        digitalWrite(digit1, DIGIT_OFF);
        digitalWrite(digit2, DIGIT_OFF);
        digitalWrite(digit3, DIGIT_OFF);
        digitalWrite(digit4, DIGIT_OFF);
     }
}

/*-----------------------------------------------------------------------
;
;   Display a single selected digit
;
;   could be done much nicer...
;   If number == 10, then turn off number
;
;-----------------------------------------------------------------------*/

void showDigit(int number) 
{

#define SEGMENT_ON  HIGH
#define SEGMENT_OFF LOW

  switch (number){

      case 0:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_OFF);
        break;
    
      case 1:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;
    
      case 2:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_OFF);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 3:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 4:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 5:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 6:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 7:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;
    
      case 8:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_ON);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 9:
        digitalWrite(segA, SEGMENT_ON);
        digitalWrite(segB, SEGMENT_ON);
        digitalWrite(segC, SEGMENT_ON);
        digitalWrite(segD, SEGMENT_ON);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_ON);
        digitalWrite(segG, SEGMENT_ON);
        break;
    
      case 10:
        digitalWrite(segA, SEGMENT_OFF);
        digitalWrite(segB, SEGMENT_OFF);
        digitalWrite(segC, SEGMENT_OFF);
        digitalWrite(segD, SEGMENT_OFF);
        digitalWrite(segE, SEGMENT_OFF);
        digitalWrite(segF, SEGMENT_OFF);
        digitalWrite(segG, SEGMENT_OFF);
        break;
      }
}

