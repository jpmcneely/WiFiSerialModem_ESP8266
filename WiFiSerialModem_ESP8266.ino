/*
   WiFi Serial Modem
   Copyright (C) 2021 JP McNeely (jpmcneely8778@gmail.com)

   This project is intended to support vintage computers participating in the AnachroNet Project.

   Based on these projects:
   - TheOldNet.com RS232 Serial WIFI Modem
     Copyright (C) 2020 Richard Bettridge (@theoldnet)
   - WiFi SIXFOUR - A virtual WiFi modem based on the ESP 8266 chipset
     Copyright (C) 2016 Paul Rickards <rickards@gmail.com>
   - ESP8266 based virtual modem
     Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* AnachroNet Changes: 
 *  
 *  20210322/001
 *  - Change default speed to 9600
 *  - Change ID strings
 *  - Remove some unused code
 *  - Change default speeddial entries
 *  - Remove petscii - not needed for the target machines
 *  - Remove debug
 *  
 *  20210322/002
 *  - Add additional ATIx Commands
 *  - Add /A - "last command" Command
 *  - Add AT%Q - WiFi RSSI Command
 *  - Remove HTTPGet and Hex Commands
 *  - Remove AT$RB Command
 *  - Change speed command to AT$B (instead on AT$SB).
 *  - Change busy msg cmd to AT$M (instead of AT$BM).
 *  
 *  20210322/003
 *  - Change disconnect behavior.
 *  - Additional ATI Commands.
 *  
 *  20210322/004
 *  - Add ati4 - project basis and attribution.
 *  
 *  20210323/005
 *  - Remove web status page code.
 *  - Formatting.
 *  
 *  20210323/006
 *  - Remove version check code - add atnvz command to force rewrite to eeprom
 *  - Add defines for modem lights
 *  
 *  20210323/007
 *  - Modem lights
 *  
 *  20210324/008
 *  - Housekeeping - refactor ati
 *  
 *  20210324/008
 *  - Add 110 baud
 *  - Add lamp check command for hdwr debug
 *  
 */

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#define VERSIONA                0
#define VERSIONB                2
#define VERSION_ADDRESS         0     // EEPROM address
#define VERSION_LEN             2     // Length in bytesF
#define SSID_ADDRESS            2
#define SSID_LEN                32
#define PASS_ADDRESS            34
#define PASS_LEN                63
#define IP_TYPE_ADDRESS         97    // for future use
#define STATIC_IP_ADDRESS       98    // length 4, for future use
#define STATIC_GW               102   // length 4, for future use
#define STATIC_DNS              106   // length 4, for future use
#define STATIC_MASK             110   // length 4, for future use
#define BAUD_ADDRESS            111
#define ECHO_ADDRESS            112
#define SERVER_PORT_ADDRESS     113   // 2 bytes
#define AUTO_ANSWER_ADDRESS     115   // 1 byte
#define TELNET_ADDRESS          116   // 1 byte
#define VERBOSE_ADDRESS         117
#define UNUSED1                 118   // Unused
#define FLOW_CONTROL_ADDRESS    119
#define PIN_POLARITY_ADDRESS    120
#define QUIET_MODE_ADDRESS      121
#define DIAL0_ADDRESS           200
#define DIAL1_ADDRESS           250
#define DIAL2_ADDRESS           300
#define DIAL3_ADDRESS           350
#define DIAL4_ADDRESS           400
#define DIAL5_ADDRESS           450
#define DIAL6_ADDRESS           500
#define DIAL7_ADDRESS           550
#define DIAL8_ADDRESS           600
#define DIAL9_ADDRESS           650
#define BUSY_MSG_ADDRESS        700
#define BUSY_MSG_LEN            80
#define LAST_ADDRESS            780

#define RTS_PIN                 4       // RTS Request to Send, connect to host's CTS pin
#define CTS_PIN                 5       // CTS Clear to Send, connect to host's RTS pin

#define MR_LED                  D2      // Modem Ready
#define CD_LED                  D6      // Carrier Detect
#define RXTX_LED                D5      // Receive/Transmit
#define AA_LED                  D7      // Auto-Answer when Steady - Flash on ring when AA off
#define RESET_SW                D4      // Reset to defaukt speed

// Global variables
String build                    = "20210324/008";
String make                     = "AnachroNet";
String model                    = "WiFi Serial Modem";
String cmd                      = "";                 // Gather a new AT command to this string from serial
String lastCmd                  = cmd;                // String to hold last command sent
bool cmdMode                    = true;               // Are we in AT command mode or connected mode
bool callConnected              = false;              // Are we currently in a call
bool telnet                     = false;              // Is telnet control code handling enabled
bool verboseResults             = false;
#define LISTEN_PORT             23                    // Listen to this if not connected. Set to zero to disable.
int tcpServerPort               = LISTEN_PORT;
#define RING_INTERVAL           3000                  // How often to print RING when having a new incoming connection (ms)
#define RESET_DELAY             3000                  // How long to hold button for reset (ms)
#define DFLT_SPEED              5                     // Default to speed on reset (bauds[x])
unsigned long lastRingMs        = 0;                  // Time of last "RING" message (millis())
#define MAX_CMD_LENGTH          256                   // Maximum length for AT command
char plusCount                  = 0;                  // Go to AT mode at "+++" sequence, that has to be counted
unsigned long plusTime          = 0;                  // When did we last receive a "+++" sequence
#define LED_TIME                10                    // How many ms to keep LED on at activity
unsigned long ledTime           = 0;
#define TX_BUF_SIZE             256                   // Buffer where to read from serial before writing to TCP
uint8_t txBuf[TX_BUF_SIZE];
const int speedDialAddresses[]  = { DIAL0_ADDRESS, DIAL1_ADDRESS, DIAL2_ADDRESS, DIAL3_ADDRESS, DIAL4_ADDRESS, DIAL5_ADDRESS, DIAL6_ADDRESS, DIAL7_ADDRESS, DIAL8_ADDRESS, DIAL9_ADDRESS };
String speedDials[10];
const int bauds[]               = { 110, 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };
byte serialspeed;
bool echo                       = true;
bool autoAnswer                 = false;
String ssid, password, busyMsg;
byte ringCount                  = 0;
String resultCodes[]            = { "OK", "CONNECT", "RING", "NO CARRIER", "ERROR", "", "NO DIALTONE", "BUSY", "NO ANSWER" };
enum resultCodes_t              { R_OK, R_CONNECT, R_RING, R_NOCARRIER, R_ERROR, R_NONE, R_NODIALTONE, R_BUSY, R_NOANSWER };
unsigned long connectTime       = 0;
enum flowControl_t              { F_NONE, F_HARDWARE, F_SOFTWARE };
byte flowControl                = F_SOFTWARE;               // Use flow control
bool txPaused                   = false;                    // Has flow control asked us to pause?
enum pinPolarity_t              { P_INVERTED, P_NORMAL };   // Is LOW (0) or HIGH (1) active?
byte pinPolarity                = P_NORMAL;
bool quietMode                  = false;

// Telnet codes
#define DO    0xfd
#define WONT  0xfc
#define WILL  0xfb
#define DONT  0xfe

WiFiClient tcpClient;
WiFiServer tcpServer(tcpServerPort);

String connectTimeString() {
  unsigned long now = millis();
  int secs = (now - connectTime) / 1000;
  int mins = secs / 60;
  int hours = mins / 60;
  String out = "";
  if (hours < 10) out.concat("0");
  out.concat(String(hours));
  out.concat(":");
  if (mins % 60 < 10) out.concat("0");
  out.concat(String(mins % 60));
  out.concat(":");
  if (secs % 60 < 10) out.concat("0");
  out.concat(String(secs % 60));
  return out;
}

void writeSettings() {
  setEEPROM(ssid, SSID_ADDRESS, SSID_LEN);
  setEEPROM(password, PASS_ADDRESS, PASS_LEN);
  setEEPROM(busyMsg, BUSY_MSG_ADDRESS, BUSY_MSG_LEN);

  EEPROM.write(BAUD_ADDRESS, serialspeed);
  EEPROM.write(ECHO_ADDRESS, byte(echo));
  EEPROM.write(AUTO_ANSWER_ADDRESS, byte(autoAnswer));
  EEPROM.write(SERVER_PORT_ADDRESS, highByte(tcpServerPort));
  EEPROM.write(SERVER_PORT_ADDRESS + 1, lowByte(tcpServerPort));
  EEPROM.write(TELNET_ADDRESS, byte(telnet));
  EEPROM.write(VERBOSE_ADDRESS, byte(verboseResults));
  EEPROM.write(FLOW_CONTROL_ADDRESS, byte(flowControl));
  EEPROM.write(PIN_POLARITY_ADDRESS, byte(pinPolarity));
  EEPROM.write(QUIET_MODE_ADDRESS, byte(quietMode));

  for (int i = 0; i < 10; i++) {
    setEEPROM(speedDials[i], speedDialAddresses[i], 50);
  }
  EEPROM.commit();
}

void readSettings() {
  echo = EEPROM.read(ECHO_ADDRESS);
  autoAnswer = EEPROM.read(AUTO_ANSWER_ADDRESS);
  if (autoAnswer == true) {
    ledAA_on();
  } else {
    ledAA_off();
  }

  ssid = getEEPROM(SSID_ADDRESS, SSID_LEN);
  password = getEEPROM(PASS_ADDRESS, PASS_LEN);
  busyMsg = getEEPROM(BUSY_MSG_ADDRESS, BUSY_MSG_LEN);
  tcpServerPort = word(EEPROM.read(SERVER_PORT_ADDRESS), EEPROM.read(SERVER_PORT_ADDRESS + 1));
  telnet = EEPROM.read(TELNET_ADDRESS);
  verboseResults = EEPROM.read(VERBOSE_ADDRESS);
  flowControl = EEPROM.read(FLOW_CONTROL_ADDRESS);
  pinPolarity = EEPROM.read(PIN_POLARITY_ADDRESS);
  quietMode = EEPROM.read(QUIET_MODE_ADDRESS);

  for (int i = 0; i < 10; i++) {
    speedDials[i] = getEEPROM(speedDialAddresses[i], 50);
  }
}

void defaultEEPROM() {
  EEPROM.write(VERSION_ADDRESS, VERSIONA);
  EEPROM.write(VERSION_ADDRESS + 1, VERSIONB);

  setEEPROM("", SSID_ADDRESS, SSID_LEN);
  setEEPROM("", PASS_ADDRESS, PASS_LEN);
  setEEPROM("d", IP_TYPE_ADDRESS, 1);
  EEPROM.write(SERVER_PORT_ADDRESS, highByte(LISTEN_PORT));
  EEPROM.write(SERVER_PORT_ADDRESS + 1, lowByte(LISTEN_PORT));

  EEPROM.write(BAUD_ADDRESS, 0x05);
  EEPROM.write(ECHO_ADDRESS, 0x01);
  EEPROM.write(AUTO_ANSWER_ADDRESS, 0x01);
  EEPROM.write(TELNET_ADDRESS, 0x00);
  EEPROM.write(VERBOSE_ADDRESS, 0x01);
  EEPROM.write(FLOW_CONTROL_ADDRESS, 0x02);
  EEPROM.write(PIN_POLARITY_ADDRESS, 0x01);
  EEPROM.write(QUIET_MODE_ADDRESS, 0x00);

  setEEPROM("bbs.anachronet.com:23", speedDialAddresses[0], 50);

  for (int i = 1; i < 10; i++) {
    setEEPROM("", speedDialAddresses[i], 50);
  }

  setEEPROM("BUSY : PLEASE TRY AGAIN LATER", BUSY_MSG_ADDRESS, BUSY_MSG_LEN);
  EEPROM.commit();
}

String getEEPROM(int startAddress, int len) {
  String myString;

  for (int i = startAddress; i < startAddress + len; i++) {
    if (EEPROM.read(i) == 0x00) {
      break;
    }
    myString += char(EEPROM.read(i));
  }
  return myString;
}

void setEEPROM(String inString, int startAddress, int maxLen) {
  for (int i = startAddress; i < inString.length() + startAddress; i++) {
    EEPROM.write(i, inString[i - startAddress]);
  }
  // null pad the remainder of the memory space
  for (int i = inString.length() + startAddress; i < maxLen + startAddress; i++) {
    EEPROM.write(i, 0x00);
  }
}

void sendResult(int resultCode) {
  Serial.print("\r\n");
  if (quietMode == 1) {
    return;
  }
  if (verboseResults == 0) {
    Serial.println(resultCode);
    return;
  }
  if (resultCode == R_CONNECT) {
    Serial.print(String(resultCodes[R_CONNECT]) + " " + String(bauds[serialspeed]));
  } else if (resultCode == R_NOCARRIER) {
    sendString("CONNECTED: " + connectTimeString());
    Serial.print(String(resultCodes[R_NOCARRIER]));
  } else {
    Serial.print(String(resultCodes[resultCode]));
  }
  Serial.print("\r\n");
}

void sendString(String msg) {
  Serial.print("\r\n");
  Serial.print(msg);
  Serial.print("\r\n");
}

// Hold button to reset baud to default
// Slow flash: hold switch
// Fast flash: release switch
int checkButton() {
  long time = millis();
  while (digitalRead(RESET_SW) == HIGH && millis() - time < 5000) {
    long remaining = millis() - time;
    delay(250);
    ledRXTX_toggle();
    yield();
  }
  if (millis() - time > RESET_DELAY) {
    char dispSpd[6] = "";
    itoa(bauds[DFLT_SPEED],dispSpd,10);
    Serial.print("\r\nReset to ");
    Serial.print(bauds[DFLT_SPEED]);
    Serial.print(" BPS");
    Serial.flush();
    Serial.end();
    serialspeed = DFLT_SPEED;
    delay(100);
    Serial.begin(bauds[serialspeed]);
    sendResult(R_OK);
    while (digitalRead(RESET_SW) == HIGH) {
      delay(50);
      ledRXTX_toggle();
      yield();
    }
    return 1;
  } else {
    return 0;
  }
}

void connectWiFi() {
  if (ssid == "" || password == "") {
    Serial.println("ERROR : NETWORK CONNECTION PARAMETERS NOT SET");
    Serial.println("TYPE AT? FOR HELP.");
    return;
  }
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("\nCONNECTING TO SSID "); Serial.print(ssid);
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {
    ledMR_off();
    delay(250);
    ledMR_on();
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (i == 21) {
    Serial.print("COULD NOT CONNECT TO "); Serial.println(ssid);
    WiFi.disconnect();
    ledMR_off();
  } else {
    Serial.print("CONNECTED TO "); Serial.println(WiFi.SSID());
    Serial.print("IP ADDRESS: "); Serial.println(WiFi.localIP());
    ledMR_on();
  }
}

void disconnectWiFi() {
  WiFi.disconnect();
  ledMR_off();
}

void setBaudRate(int inSpeed) {
  if (inSpeed == 0) {
    sendResult(R_ERROR);
    return;
  }
  int foundBaud = -1;
  for (int i = 0; i < sizeof(bauds); i++) {
    if (inSpeed == bauds[i]) {
      foundBaud = i;
      break;
    }
  }
  // requested baud rate not found, return error
  if (foundBaud == -1) {
    sendResult(R_ERROR);
    return;
  }
  if (foundBaud == serialspeed) {
    sendResult(R_OK);
    return;
  }
  Serial.print("SWITCHING SERIAL PORT TO ");
  Serial.print(inSpeed);
  Serial.println(" IN 5 SECONDS");
  delay(5000);
  Serial.end();
  delay(200);
  Serial.begin(bauds[foundBaud]);
  serialspeed = foundBaud;
  delay(200);
  sendResult(R_OK);
}

void setCarrier(byte carrier) {
  if (pinPolarity == P_NORMAL) carrier = !carrier;
  digitalWrite(CD_LED, !carrier);
}

void displayNetworkStatus() {
  Serial.println();
  Serial.println(make + " " + model);
  Serial.println(build);
  Serial.println("-----------------------------------------");
  yield();
  Serial.print("WIFI STATUS: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("CONNECTED");
  }
  if (WiFi.status() == WL_IDLE_STATUS) {
    Serial.println("OFFLINE");
  }
  if (WiFi.status() == WL_CONNECT_FAILED) {
    Serial.println("CONNECT FAILED");
  }
  if (WiFi.status() == WL_NO_SSID_AVAIL) {
    Serial.println("SSID UNAVAILABLE");
  }
  if (WiFi.status() == WL_CONNECTION_LOST) {
    Serial.println("CONNECTION LOST");
  }
  if (WiFi.status() == WL_DISCONNECTED) {
    Serial.println("DISCONNECTED");
  }
  if (WiFi.status() == WL_SCAN_COMPLETED) {
    Serial.println("SCAN COMPLETED");
  }
  yield();

  Serial.print("SSID       : "); Serial.println(WiFi.SSID());

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC ADDRESS: ");
  Serial.print(mac[0], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.println(mac[5], HEX);
  yield();

  Serial.print("IP ADDRESS : "); Serial.println(WiFi.localIP()); yield();
  Serial.print("GATEWAY    : "); Serial.println(WiFi.gatewayIP()); yield();
  Serial.print("SUBNET MASK: "); Serial.println(WiFi.subnetMask()); yield();
  Serial.print("SERVER PORT: "); Serial.println(tcpServerPort); yield();
  Serial.print("CALL STATUS: "); yield();
  Serial.println(); yield();
  if (callConnected) {
    Serial.print("CONNECTED TO "); Serial.println(ipToString(tcpClient.remoteIP())); yield();
    Serial.print("CALL LENGTH: "); Serial.println(connectTimeString()); yield();
  } else {
    Serial.println("NOT CONNECTED"); yield();
  }
}

void displayCmdSettings() {
  Serial.print("AT "); yield();
  Serial.print("E"); Serial.print(echo); Serial.print(" "); yield();
  Serial.print("Q"); Serial.print(quietMode); Serial.print(" "); yield();
  Serial.print("V"); Serial.print(verboseResults); Serial.print(" "); yield();
  Serial.print("&K"); Serial.print(flowControl); Serial.print(" "); yield();
  Serial.print("&P"); Serial.print(pinPolarity); Serial.print(" "); yield();
  Serial.print("NET"); Serial.print(telnet); Serial.print(" "); yield();
  Serial.print("S0:"); Serial.print(autoAnswer); yield();
  Serial.println(); yield();
}

void displayCurrentSettings() {
  Serial.println("ACTIVE PROFILE:"); yield();
  Serial.print("BAUD    : "); Serial.println(bauds[serialspeed]); yield();
  Serial.print("SSID    : "); Serial.println(ssid); yield();
  Serial.print("PASS    : "); Serial.println(password); yield();
  Serial.print("BUSY MSG: "); Serial.println(busyMsg); yield();
  Serial.print("E"); Serial.print(echo); Serial.print(" "); yield();
  Serial.print("Q"); Serial.print(quietMode); Serial.print(" "); yield();
  Serial.print("V"); Serial.print(verboseResults); Serial.print(" "); yield();
  Serial.print("&K"); Serial.print(flowControl); Serial.print(" "); yield();
  Serial.print("&P"); Serial.print(pinPolarity); Serial.print(" "); yield();
  Serial.print("NET"); Serial.print(telnet); Serial.print(" "); yield();
  Serial.print("S0:"); Serial.print(autoAnswer); Serial.print(" "); yield();
  Serial.println(); yield();

  Serial.println("SPEED DIAL:");
  for (int i = 0; i < 10; i++) {
    Serial.print(i); Serial.print(": "); Serial.println(speedDials[i]);
    yield();
  }
  Serial.println();
}

void displayStoredSettings() {
  Serial.println("STORED PROFILE:"); yield();
  Serial.print("BAUD    : "); Serial.println(bauds[EEPROM.read(BAUD_ADDRESS)]); yield();
  Serial.print("SSID    : "); Serial.println(getEEPROM(SSID_ADDRESS, SSID_LEN)); yield();
  Serial.print("PASS    : "); Serial.println(getEEPROM(PASS_ADDRESS, PASS_LEN)); yield();
  Serial.print("BUSY MSG: "); Serial.println(getEEPROM(BUSY_MSG_ADDRESS, BUSY_MSG_LEN)); yield();
  Serial.print("E"); Serial.print(EEPROM.read(ECHO_ADDRESS)); Serial.print(" "); yield();
  Serial.print("Q"); Serial.print(EEPROM.read(QUIET_MODE_ADDRESS)); Serial.print(" "); yield();
  Serial.print("V"); Serial.print(EEPROM.read(VERBOSE_ADDRESS)); Serial.print(" "); yield();
  Serial.print("&K"); Serial.print(EEPROM.read(FLOW_CONTROL_ADDRESS)); Serial.print(" "); yield();
  Serial.print("&P"); Serial.print(EEPROM.read(PIN_POLARITY_ADDRESS)); Serial.print(" "); yield();
  Serial.print("NET"); Serial.print(EEPROM.read(TELNET_ADDRESS)); Serial.print(" "); yield();
  Serial.print("S0:"); Serial.print(EEPROM.read(AUTO_ANSWER_ADDRESS)); Serial.print(" "); yield();
  Serial.println(); yield();

  Serial.println("STORED SPEED DIAL:");
  for (int i = 0; i < 10; i++) {
    Serial.print(i); Serial.print(": "); Serial.println(getEEPROM(speedDialAddresses[i], 50));
    yield();
  }
  Serial.println();
}

void waitForSpace() {
  Serial.print("PRESS SPACE");
  char c = 0;
  while (c != 0x20) {
    if (Serial.available() > 0) {
      c = Serial.read();
    }
  }
  Serial.print("\r");
}

void displayHelp() {
  displayPgmBanner();
  Serial.println("AT COMMAND SUMMARY:"); yield();
  Serial.println("DIAL HOST            : ATDTHOST:PORT"); yield();
  Serial.println("SPEED DIAL           : ATDSN (N=0-9)"); yield();
  Serial.println("SET SPEED DIAL       : AT&ZN=HOST:PORT (N=0-9)"); yield();
  Serial.println("HANDLE TELNET        : ATNETN (N=0,1)"); yield();
  Serial.println("NETWORK INFO         : ATI"); yield();
  Serial.println("AUTO ANSWER          : ATS0=N (N=0,1)"); yield();
  Serial.println("SET BUSY MSG         : AT$M=YOUR BUSY MESSAGE"); yield();
  Serial.println("LOAD NVRAM           : ATZ"); yield();
  Serial.println("SAVE TO NVRAM        : AT&W"); yield();
  Serial.println("RESET NVRAM TO DFLTS : ATNVZ"); yield();
  Serial.println("SHOW SETTINGS        : AT&V"); yield();
  Serial.println("FACT. DEFAULTS       : AT&F"); yield();
  Serial.println("PIN POLARITY         : AT&PN (N=0/INV,1/NORM)"); yield();
  Serial.println("ECHO OFF/ON          : ATE0 / ATE1"); yield();
  Serial.println("QUIET MODE OFF/ON    : ATQ0 / ATQ1"); yield();
  Serial.println("VERBOSE OFF/ON       : ATV0 / ATV1"); yield();
  Serial.println("SET SSID             : AT$SSID=WIFISSID"); yield();
  Serial.println("SET PASSWORD         : AT$PASS=WIFIPASSWORD"); yield();
  Serial.println("GET SIGNAL STRENGTH  : AT%Q"); yield();
  Serial.println("CHECK LIGHTS         : AT$LC"); yield();
  waitForSpace();
  Serial.println("SET BAUD RATE        : AT$B=N (110,300,1200,2400,4800,9600"); yield();
  Serial.println("                               19200,38400,57600,115200"); yield();
  Serial.println("FLOW CONTROL         : AT&KN (N=0/N,1/HW,2/SW)"); yield();
  Serial.println("WIFI OFF/ON          : ATC0 / ATC1"); yield();
  Serial.println("HANGUP               : ATH"); yield();
  Serial.println("ENTER CMD MODE       : +++"); yield();
  Serial.println("EXIT CMD MODE        : ATO"); yield();
  Serial.println("REPEAT LAST CMD      : /A (No AT needed)"); yield();
  Serial.println("QUERY MOST COMMANDS FOLLOWED BY '?'"); yield();
}

void storeSpeedDial(byte num, String location) {
  if (num < 0 || num > 9) { 
    sendString(String(resultCodes[R_ERROR]));  
    return;
  }
  speedDials[num] = location;
}

void displayPgmBanner() {
  Serial.println();
  Serial.println(make);
  Serial.println(model);
  Serial.println(build);
  Serial.println("------------------------------------------------------------");
  Serial.println();
}

void ledMR_on() {
  digitalWrite(MR_LED, HIGH);
}

void ledMR_off() {
  digitalWrite(MR_LED, LOW);
}

void ledCD_on() {
  digitalWrite(CD_LED, HIGH);
}

void ledCD_off() {
  digitalWrite(CD_LED, LOW);
}

void ledAA_on() {
  digitalWrite(AA_LED, HIGH);
}

void ledAA_off() {
  digitalWrite(AA_LED, LOW);
}

void ledRXTX_on() {
  digitalWrite(RXTX_LED, HIGH);
}

void ledRXTX_off() {
  digitalWrite(RXTX_LED, LOW);
}

void ledRXTX_toggle() {
  digitalWrite(RXTX_LED, !digitalRead(RXTX_LED));
}

void ledRXTX_timed()
{
  // Turn on the LED and store the time, so the LED will be shortly after turned off
  ledRXTX_on();
  ledTime = millis();
}

void ledAA_flash() {
  for (int i = 0; i <= 10; i++) {
    delay(100);
    digitalWrite(AA_LED, !digitalRead(AA_LED));
  }
}

void lampCheck() {
  // Lamp check
  delay(1000);
  ledMR_on();
  ledCD_off();
  ledAA_off();
  ledRXTX_off();
  delay(1000);
  ledMR_off();
  ledCD_on();
  ledAA_off();
  ledRXTX_off();
  delay(1000);
  ledMR_off();
  ledCD_off();
  ledAA_off();
  ledRXTX_on();
  delay(1000);
  ledMR_off();
  ledCD_off();
  ledAA_on();
  ledRXTX_off();
  delay(1000);
  ledMR_on();
  ledCD_on();
  ledAA_on();
  ledRXTX_on();
  delay(2000);
  ledMR_off();
  ledCD_off();
  ledAA_off();
  ledRXTX_off();
}

//  ----- Initialize -----
void setup() {
  
  pinMode(MR_LED, OUTPUT);
  pinMode(CD_LED, OUTPUT);
  pinMode(AA_LED, OUTPUT);
  pinMode(RXTX_LED, OUTPUT);
  pinMode(RESET_SW, INPUT);
  digitalWrite(RESET_SW, LOW);  // Goes HIGH when switch is pressed
  pinMode(RTS_PIN, OUTPUT);
  digitalWrite(RTS_PIN, HIGH);  // ready to receive data
  pinMode(CTS_PIN, INPUT);

  lampCheck();
  setCarrier(false);

  EEPROM.begin(LAST_ADDRESS + 1);
  delay(10);

  readSettings();
  // Get saved baud rate
  serialspeed = EEPROM.read(BAUD_ADDRESS);
  // Default to DFLT_SPEED if invalid
  if (serialspeed < 0 || serialspeed > sizeof(bauds)) {
    serialspeed = DFLT_SPEED;
  }

  Serial.begin(bauds[serialspeed]);

  char c;
  while (c != 0x0a && c != 0x0d) {
    if (Serial.available() > 0) {
      c = Serial.read();
    }
    if (checkButton() == 1) {
      break; // button pressed - reset to default speed
    }
    yield();
  }

  Serial.flush();
  Serial.println("...");
  Serial.println();
  Serial.flush();
  displayPgmBanner();

  if (tcpServerPort > 0) tcpServer.begin();

  WiFi.mode(WIFI_STA);
  connectWiFi();
  sendResult(R_OK);
  ledMR_on();
}

String ipToString(IPAddress ip) {
  char s[16];
  sprintf(s, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return s;
}

void hangUp() {
  tcpClient.stop();
  callConnected = false;
  setCarrier(callConnected);
  sendResult(R_NOCARRIER);
  connectTime = 0;
  ledCD_off();
}

void answerCall() {
  tcpClient = tcpServer.available();
  tcpClient.setNoDelay(true); // try to disable nagle
  sendResult(R_CONNECT);
  connectTime = millis();
  cmdMode = false;
  callConnected = true;
  setCarrier(callConnected);
  Serial.flush();
}

void handleIncomingConnection() {
  if (callConnected == 1 || (autoAnswer == false && ringCount > 3)) {
    // We're in a call already or didn't answer the call after three rings
    // We didn't answer the call. Notify our party we're busy and disconnect
    ringCount = lastRingMs = 0;
    WiFiClient anotherClient = tcpServer.available();
    anotherClient.print(busyMsg);
    anotherClient.print("\r\n");
    anotherClient.print("CURRENT CALL LENGTH: ");
    anotherClient.print(connectTimeString());
    anotherClient.print("\r\n");
    anotherClient.print("\r\n");
    anotherClient.flush();
    anotherClient.stop();
    return;
  }

  if (autoAnswer == false) {
    if (millis() - lastRingMs > 6000 || lastRingMs == 0) {
      lastRingMs = millis();
      ledAA_flash();
      delay(10);
      sendResult(R_RING);
      ringCount++;
    }
    return;
  }

  if (autoAnswer == true) {
    tcpClient = tcpServer.available();
    if (verboseResults == 1) {
      sendString(String("RING ") + ipToString(tcpClient.remoteIP()));
    }
    delay(1000);
    sendResult(R_CONNECT);
    connectTime = millis();
    cmdMode = false;
    tcpClient.flush();
    callConnected = true;
    setCarrier(callConnected);
  }
}

void dialOut(String upCmd) {
  // Can't place a call while in a call
  if (callConnected) {
    sendResult(R_ERROR);
    return;
  }
  String host, port;
  int portIndex;
  // Dialing a stored number
  if (upCmd.indexOf("ATDS") == 0) {
    byte speedNum = upCmd.substring(4, 5).toInt();
    portIndex = speedDials[speedNum].indexOf(':');
    if (portIndex != -1) {
      host = speedDials[speedNum].substring(0, portIndex);
      port = speedDials[speedNum].substring(portIndex + 1);
    } else {
      port = "23";
    }
  } else {
    // Dialing an ad-hoc number
    int portIndex = cmd.indexOf(":");
    if (portIndex != -1)
    {
      host = cmd.substring(4, portIndex);
      port = cmd.substring(portIndex + 1, cmd.length());
    }
    else
    {
      host = cmd.substring(4, cmd.length());
      port = "23"; // Telnet default
    }
  }
  host.trim(); // remove leading or trailing spaces
  port.trim();
  Serial.print("DIALING "); Serial.print(host); Serial.print(":"); Serial.println(port);
  char *hostChr = new char[host.length() + 1];
  host.toCharArray(hostChr, host.length() + 1);
  int portInt = port.toInt();
  tcpClient.setNoDelay(true); // Try to disable nagle
  if (tcpClient.connect(hostChr, portInt))
  {
    tcpClient.setNoDelay(true); // Try to disable nagle
    sendResult(R_CONNECT);
    connectTime = millis();
    cmdMode = false;
    Serial.flush();
    callConnected = true;
    setCarrier(callConnected);
  }
  else
  {
    sendResult(R_NOANSWER);
    callConnected = false;
    setCarrier(callConnected);
  }
  delete hostChr;
}

// Perform a command given in command mode
void execCommand()
{
  cmd.trim();
  if (cmd == "") return;
  Serial.println();
  String upCmd = cmd;
  upCmd.toUpperCase();

  // Just AT
  if (upCmd == "AT") sendResult(R_OK);

  // Repeat Last Command
  if (upCmd == "/A") {
    if (lastCmd != "") {
      cmd = lastCmd;
      execCommand();
    } else {
      sendResult(R_OK);
    }
  }

  // Dial to host
  else if ((upCmd.indexOf("ATDT") == 0) || (upCmd.indexOf("ATDP") == 0) || (upCmd.indexOf("ATDI") == 0) || (upCmd.indexOf("ATDS") == 0))
  {
    dialOut(upCmd);
  }

  // Change telnet mode
  else if (upCmd == "ATNET0")
  {
    telnet = false;
    sendResult(R_OK);
  }
  else if (upCmd == "ATNET1")
  {
    telnet = true;
    sendResult(R_OK);
  }

  else if (upCmd == "ATNET?") {
    Serial.println(String(telnet));
    sendResult(R_OK);
  }

  // Answer to incoming connection
  else if ((upCmd == "ATA") && tcpServer.hasClient()) {
    answerCall();
  }

  // Display Help
  else if (upCmd == "AT?" || upCmd == "ATHELP") {
    displayHelp();
    sendResult(R_OK);
  }

  // Reset, reload settings from EEPROM
  else if (upCmd == "ATZ") {
    readSettings();
    sendResult(R_OK);
  }

  // Reset, rewrite defaults to EEPROM
  else if (upCmd == "ATNVZ") {
    defaultEEPROM();
    readSettings();
    sendResult(R_OK);
  }

  // Disconnect WiFi
  else if (upCmd == "ATC0") {
    disconnectWiFi();
    sendResult(R_OK);
    ledMR_off();
  }

  // Connect WiFi
  else if (upCmd == "ATC1") {
    connectWiFi();
    sendResult(R_OK);
    ledMR_on();
  }

  // Control local echo in command mode
  else if (upCmd.indexOf("ATE") == 0) {
    if (upCmd.substring(3, 4) == "?") {
      sendString(String(echo));
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "0") {
      echo = 0;
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "1") {
      echo = 1;
      sendResult(R_OK);
    }
    else {
      sendResult(R_ERROR);
    }
  }

  // Control verbosity
  else if (upCmd.indexOf("ATV") == 0) {
    if (upCmd.substring(3, 4) == "?") {
      sendString(String(verboseResults));
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "0") {
      verboseResults = 0;
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "1") {
      verboseResults = 1;
      sendResult(R_OK);
    }
    else {
      sendResult(R_ERROR);
    }
  }

  // Control pin polarity of CTS, RTS, DCD
  else if (upCmd.indexOf("AT&P") == 0) {
    if (upCmd.substring(4, 5) == "?") {
      sendString(String(pinPolarity));
      sendResult(R_OK);
    }
    else if (upCmd.substring(4, 5) == "0") {
      pinPolarity = P_INVERTED;
      sendResult(R_OK);
      setCarrier(callConnected);
    }
    else if (upCmd.substring(4, 5) == "1") {
      pinPolarity = P_NORMAL;
      sendResult(R_OK);
      setCarrier(callConnected);
    }
    else {
      sendResult(R_ERROR);
    }
  }

  // Flow Control
  else if (upCmd.indexOf("AT&K") == 0) {
    if (upCmd.substring(4, 5) == "?") {
      sendString(String(flowControl));
      sendResult(R_OK);
    }
    else if (upCmd.substring(4, 5) == "0") {
      flowControl = 0;
      sendResult(R_OK);
    }
    else if (upCmd.substring(4, 5) == "1") {
      flowControl = 1;
      sendResult(R_OK);
    }
    else if (upCmd.substring(4, 5) == "2") {
      flowControl = 2;
      sendResult(R_OK);
    }
    else {
      sendResult(R_ERROR);
    }
  }

  // Set baud rate
  else if (upCmd.indexOf("AT$B=") == 0) {
    setBaudRate(upCmd.substring(5).toInt());
  }

  // Display current baud rate
  else if (upCmd.indexOf("AT$B?") == 0) {
    sendString(String(bauds[serialspeed]));;
  }

  // Set busy message
  else if (upCmd.indexOf("AT$M=") == 0) {
    busyMsg = cmd.substring(5);
    sendResult(R_OK);
  }

  // Display current busy message
  else if (upCmd.indexOf("AT$M?") == 0) {
    sendString(busyMsg);
    sendResult(R_OK);
  }

  // Display Informationn
  else if (upCmd == "ATI") {
    displayNetworkStatus();
    sendResult(R_OK);
  }

  else if (upCmd == "ATI0") {
    Serial.println(model);
    sendResult(R_OK);
  }

  else if (upCmd == "ATI1") {
    Serial.println(build);
    sendResult(R_OK);
  }

  else if (upCmd == "ATI2") {
    displayCmdSettings();
    sendResult(R_OK);
  }

  else if (upCmd == "ATI3") {
    Serial.println();
    Serial.println(make + " " + model);
    Serial.println("Build: " + build);
    Serial.println("Copyright (C) 2021 - JP McNeely - AnachroNet.com");
    Serial.println("-----------------------------------------------------------------------");
    Serial.println();
    Serial.println("Based on these projects:");
    Serial.println("- TheOldNet.com RS232 Serial WIFI Modem");
    Serial.println("  Copyright (C) 2020 Richard Bettridge (@theoldnet)");
    Serial.println("- WiFi SIXFOUR - A virtual WiFi modem based on the ESP 8266 chipset");
    Serial.println("  Copyright (C) 2016 Paul Rickards <rickards@gmail.com>");
    Serial.println("- ESP8266 based virtual modem");
    Serial.println("  Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>");
    Serial.println();
    Serial.println("This program is free software: you can redistribute it and/or modify");
    Serial.println("it under the terms of the GNU General Public License as published by");
    Serial.println("the Free Software Foundation, either version 3 of the License, or");
    Serial.println("(at your option) any later version.");
    Serial.println();
    waitForSpace();
    Serial.println();
    Serial.println("This program is distributed in the hope that it will be useful,");
    Serial.println("but WITHOUT ANY WARRANTY; without even the implied warranty of");
    Serial.println("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
    Serial.println("GNU General Public License for more details.");
    Serial.println();
    Serial.println("You should have received a copy of the GNU General Public License");
    Serial.println("along with this program.  If not, see <http://www.gnu.org/licenses/>.");
    Serial.println();
    sendResult(R_OK);
  }

  // Display profile settings
  else if (upCmd == "AT&V") {
    displayCurrentSettings();
    waitForSpace();
    displayStoredSettings();
    sendResult(R_OK);
  }

  // Lamp check
  else if (upCmd == "AT$LC") {
    Serial.println("Checking lights...");
    lampCheck();
    if (WiFi.status() == WL_CONNECTED) {
      ledMR_on();
    } else {
      ledMR_off();
    }
    if (callConnected == true) {
      ledCD_on();
    } else {
      ledCD_off();
    }
    if (autoAnswer == true) {
      ledAA_on();
    } else {
      ledAA_off();
    }
    sendResult(R_OK);
  }

  // Write current settings to EEPROM
  else if (upCmd == "AT&W") {
    writeSettings();
    sendResult(R_OK);
  }

  // Set or display a speed dial number
  else if (upCmd.indexOf("AT&Z") == 0) {
    byte speedNum = upCmd.substring(4, 5).toInt();
    if (speedNum >= 0 && speedNum <= 9) {
      if (upCmd.substring(5, 6) == "=") {
        String speedDial = cmd;
        storeSpeedDial(speedNum, speedDial.substring(6));
        sendResult(R_OK);
      }
      if (upCmd.substring(5, 6) == "?") {
        sendString(speedDials[speedNum]);
        sendResult(R_OK);
      }
    } else {
      sendResult(R_ERROR);
    }
  }

  // Set WiFi SSID
  else if (upCmd.indexOf("AT$SSID=") == 0) {
    ssid = cmd.substring(8);
    sendResult(R_OK);
  }

  // Display WiFi SSID
  else if (upCmd == "AT$SSID?") {
    sendString(ssid);
    sendResult(R_OK);
  }

  // Set WiFi Password
  else if (upCmd.indexOf("AT$PASS=") == 0) {
    password = cmd.substring(8);
    sendResult(R_OK);
  }

  // Display WiFi Password
  else if (upCmd == "AT$PASS?") {
    sendString(password);
    sendResult(R_OK);
  }

  // Reset EEPROM and current settings to factory defaults
  else if (upCmd == "AT&F") {
    defaultEEPROM();
    readSettings();
    sendResult(R_OK);
  }

  // Set auto answer off
  else if (upCmd == "ATS0=0") {
    autoAnswer = false;
    ledAA_off();
    sendResult(R_OK);
  }

  // Set auto answer on
  else if (upCmd == "ATS0=1") {
    autoAnswer = true;
    ledAA_on();
    sendResult(R_OK);
  }

  // Display auto answer setting
  else if (upCmd == "ATS0?") {
    sendString(String(autoAnswer));
    sendResult(R_OK);
  }

  // Hang up a call
  else if (upCmd.indexOf("ATH") == 0) {
    hangUp();
  }

  // Get wifi signal strength
  else if (upCmd == "AT%Q") {
    long wifirssi = WiFi.RSSI();
    char outrssi[10];
    ltoa(wifirssi,outrssi,10);
    sendString(outrssi);
    sendResult(R_OK);
  }

  // Exit modem command mode, go online
  else if (upCmd == "ATO") {
    if (callConnected == 1) {
      sendResult(R_CONNECT);
      cmdMode = false;
    } else {
      sendResult(R_ERROR);
    }
  }

  // Set incoming TCP server port
  else if (upCmd.indexOf("AT$SP=") == 0) {
    tcpServerPort = upCmd.substring(6).toInt();
    sendString("CHANGE REQUIRES NVRAM SAVE (AT&W) AND RESTART");
    sendResult(R_OK);
  }

  // Display incoming TCP server port
  else if (upCmd == "AT$SP?") {
    sendString(String(tcpServerPort));
    sendResult(R_OK);
  }

  // Display IP address
  else if (upCmd == "ATIP?")
  {
    Serial.println(WiFi.localIP());
    sendResult(R_OK);
  }

  // Quiet mode
  else if (upCmd.indexOf("ATQ") == 0) {
    if (upCmd.substring(3, 4) == "?") {
      sendString(String(quietMode));
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "0") {
      quietMode = 0;
      sendResult(R_OK);
    }
    else if (upCmd.substring(3, 4) == "1") {
      quietMode = 1;
      sendResult(R_OK);
    }
    else {
      sendResult(R_ERROR);
    }
  }

  // Unknown command
  else sendResult(R_ERROR);

  lastCmd = cmd;
  cmd = "";
}

void handleFlowControl() {
  if (flowControl == F_NONE) return;
  if (flowControl == F_HARDWARE) {
    if (digitalRead(CTS_PIN) == pinPolarity) {
      txPaused = true;
    } else {
      txPaused = false;
    }
  }
  if (flowControl == F_SOFTWARE) {
    // TODO: xon/xoff
  }
}

// ----- Main -----
void loop()
{
  // Check flow control
  handleFlowControl();

  // Check to see if user is requesting rate change to 9600 baud
  checkButton();

  // New unanswered incoming connection on server listen socket
  if (tcpServer.hasClient()) {
    handleIncomingConnection();
  }

  // AT command mode
  if (cmdMode == true)
  {

    // In command mode - don't exchange with TCP but gather characters to a string
    if (Serial.available())
    {
      char chr = Serial.read();

      // Return, enter, new line, carriage return to end command
      if ((chr == '\n') || (chr == '\r')) {
        execCommand();
      }
      // Backspace or delete deletes previous character
      else if ((chr == 8) || (chr == 127) || (chr == 20)) {
        cmd.remove(cmd.length() - 1);
        if (echo == true) {
          Serial.write(chr);
        }
      } else {
        if (cmd.length() < MAX_CMD_LENGTH) {
          cmd.concat(chr);
        }
        if (echo == true) {
          Serial.write(chr);
        }
      }
    }
  }
  // Connected mode
  else
  {
    // Transmit from terminal to TCP
    if (Serial.available())
    {
      ledRXTX_timed();

      // In telnet in worst case we have to escape every byte
      // so leave half of the buffer always free
      int max_buf_size;
      if (telnet == true)
        max_buf_size = TX_BUF_SIZE / 2;
      else
        max_buf_size = TX_BUF_SIZE;

      // Read from serial, the amount available up to
      // maximum size of the buffer
      size_t len = std::min(Serial.available(), max_buf_size);
      Serial.readBytes(&txBuf[0], len);

      // Enter command mode with "+++" sequence
      for (int i = 0; i < (int)len; i++)
      {
        if (txBuf[i] == '+') plusCount++; else plusCount = 0;
        if (plusCount >= 3)
        {
          plusTime = millis();
        }
        if (txBuf[i] != '+')
        {
          plusCount = 0;
        }
      }

      // Double (escape) every 0xff for telnet, shifting the following bytes
      // towards the end of the buffer from that point
      if (telnet == true)
      {
        for (int i = len - 1; i >= 0; i--)
        {
          if (txBuf[i] == 0xff)
          {
            for (int j = TX_BUF_SIZE - 1; j > i; j--)
            {
              txBuf[j] = txBuf[j - 1];
            }
            len++;
          }
        }
      }
      // Write the buffer to TCP finally
      tcpClient.write(&txBuf[0], len);
      yield();
    }

    // Transmit from TCP to terminal
    while (tcpClient.available() && txPaused == false)
    {
      ledRXTX_timed();
      uint8_t rxByte = tcpClient.read();

      // Is a telnet control code starting?
      if ((telnet == true) && (rxByte == 0xff))
      {
        rxByte = tcpClient.read();
        if (rxByte == 0xff)
        {
          // 2 times 0xff is just an escaped real 0xff
          Serial.write(0xff); Serial.flush();
        }
        else
        {
          // rxByte has now the first byte of the actual non-escaped control code
          uint8_t cmdByte1 = rxByte;
          rxByte = tcpClient.read();
          uint8_t cmdByte2 = rxByte;
          // rxByte has now the second byte of the actual non-escaped control code
          // We are asked to do some option, respond we won't
          if (cmdByte1 == DO)
          {
            tcpClient.write((uint8_t)255); tcpClient.write((uint8_t)WONT); tcpClient.write(cmdByte2);
          }
          // Server wants to do any option, allow it
          else if (cmdByte1 == WILL)
          {
            tcpClient.write((uint8_t)255); tcpClient.write((uint8_t)DO); tcpClient.write(cmdByte2);
          }
        }
      }
      else
      {
        // Non-control codes pass through freely
        Serial.write(rxByte); yield(); Serial.flush(); yield();
      }
      handleFlowControl();
    }
  }

  // If we have received "+++" as last bytes from serial port and there
  // has been over a second without any more bytes
  if (plusCount >= 3)
  {
    if (millis() - plusTime > 1000)
    {
      //tcpClient.stop();
      cmdMode = true;
      sendResult(R_OK);
      plusCount = 0;
    }
  }

  // Go to command mode if TCP disconnected and not in command mode
  if ((!tcpClient.connected()) && (cmdMode == false) && callConnected == true)
  {
    cmdMode = true;
    sendResult(R_NOCARRIER);
    connectTime = 0;
    callConnected = false;
    setCarrier(callConnected);
  }

  // Turn off tx/rx led if it has been lit long enough to be visible
  if (millis() - ledTime > LED_TIME) {
    ledRXTX_off();
  }
}

// EOF
