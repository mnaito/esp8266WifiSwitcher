// -*- lsst-c++ -*-

/*
 * This file is part of ESP8266 switcher
 *
 * See the COPYRIGHT file at the top-level directory of this distribution
 * for details of code ownership.
 *
 * Copyright (c) 2020 Mitsutaka Naito
 * Released under the MIT license
 * https://github.com/mnaito/esp8266WifiSwitcher/blob/master/LICENSE
 */

#include <string.h>
#include <ctype.h>

#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

int outPin = 5;
int configPin = 12;    // when voltage for this pin HIGH, system works under configuration mode.

/* AP name for configuration mode */
const char *APssid = "esp8266WifiSwitcher";

// buffer for telnet input
#define MAX_INPUT_BUF 128

// declare telnet server (do NOT put in setup())
WiFiServer TelnetServer(23);
WiFiClient Telnet;

/**
 * EEPROM data structure for config
 */
typedef struct eeprom_t{ 
  char ssid[64];
  char passwd[64];
};


/**
 * soft reset
 *@return void
 */
void(* resetDevice) (void) = 0;


/**
 * handle telnet
 *@return void
 */
void handleTelnet(){
  
  if (TelnetServer.hasClient()){
    
    if (!Telnet || !Telnet.connected()){
      
      // client disconnected
      if (Telnet) {
        Telnet.stop();
      }
      
      Telnet = TelnetServer.available(); // ready for new client
      
    } else {
      
      // have a client, it should block new conections
      TelnetServer.available().stop();

       // flush input buffer
      TelnetServer.flush();
    }
  }

  // while connection is active
  if (Telnet && Telnet.connected() && Telnet.available()){   
    // client input processing
    char buf[MAX_INPUT_BUF+1];
    memset(buf, 0, sizeof(buf));
    
    // store incoming packet to buffer
    // data more than the buffersize will be ignored 
    for (byte i=0; i < MAX_INPUT_BUF && Telnet.available(); i++) {
      char r = Telnet.read();
      if (!isascii(r)) break;
      buf[i] = r;
    }
    
    Serial.println(buf);

    // parse input and 
    if (strlen(buf) > 0) {
      String cmd = String(buf);
      
      // help
      if ( cmd.indexOf("help") == 0 ) {
        
        Telnet.println("usage: set ap [SSID] [PASSWD]");
        
      }
      // AP setup
      else if ( cmd.indexOf("set ap") == 0 ) {
        
        eeprom_t eeprom = {};

        // read from telnet buffer
        char * pch = strtok(buf, " \r\n");
        for (int i=0; pch != NULL; i++, pch = strtok(NULL," \r\n"))
        {
          // ignore the first 2 words
          if (i < 2) continue;
          Serial.println(String(pch));

          switch (i) {
            // SSID
            case 2:
              strcpy(eeprom.ssid, pch);
              break;
            // Password for the AP
            case 3:
              strcpy(eeprom.passwd, pch);
              break;
          }
        }

        // replace values in byte-array cache with modified data
        // no changes made to flash, all in local byte-array cache
        EEPROM.put(0, eeprom);

        // actually write the content of byte-array cache to
        // hardware flash.  flash write occurs if and only if one or more byte
        // in byte-array cache has been changed, but if so, ALL 512 bytes are 
        // written to flash
        EEPROM.commit(); 

        // print OK and reset the device
        Telnet.println("OK");
        delay(100);
        Telnet.stop();
        delay(100);
        
        resetDevice();
      }
    }
  } 
}

/**
 * initialize WiFi network
 *@return void
 */
void initWifi(){
  
  IPAddress myIP;
  
  //AP mode
  if (digitalRead(configPin) == HIGH) {
    
    Serial.println("setup AP");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid /*,APpassword*/ );
    myIP = WiFi.softAPIP();
    
  } else {
    // read config from EEPROM
    eeprom_t eeprom;
    EEPROM.get(0, eeprom);
    
    // client mode
    Serial.println("setup client");
    Serial.println("SSID: "+ String(eeprom.ssid) + ", Passwd: "+String(eeprom.passwd));

    // disable soft AP first
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    // connects Wi-Fi and wait it becomes available
    WiFi.begin(eeprom.ssid, eeprom.passwd);
    
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    
    Serial.println("");
    myIP = WiFi.localIP();
  }
  
  // print the IP
  Serial.print("My IP address: ");
  Serial.println(myIP);
}

/**
 * setup
 * @return void
 */
void setup() {
  Serial.begin(115200);

  // EEPROM for SSID/Password configuration
  EEPROM.begin(sizeof(eeprom_t));

  // initialize GPIO 2 as an output.
  pinMode(outPin, OUTPUT);
  pinMode(configPin, INPUT);

  // Serial.setDebugOutput(true);
  delay(1000); // serial delay

  Serial.println();
  initWifi();  // startup is async ?
  delay(4000); // ap delay

  // mDNS init
  if (!MDNS.begin("esp8266")) {            // Start the mDNS responder for esp8266.local
    Serial.println("ERR: mDNS");
  }
  Serial.println("mDNS OK");
  
  // init TELNET
  TelnetServer.begin();
  Serial.println("telnet OK");
  TelnetServer.setNoDelay(true); // ESP BUG ?
  Serial.println("");
  delay(100);
}


/**
 * main loop
 * @return void
 */
void loop() {
  handleTelnet();

  delay(100); // to fast might crash terminals
}
