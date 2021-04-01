# WiFiSerialModem_ESP8266
Modem emulator using wifi networking based on the ESP8266 microcontroller.

Copyright (C) 2021 J.P. McNeely under GPLv3 license - AnachroNet.com

This program is based on the following projects:

https://github.com/ssshake/vintage-computer-wifi-modem

Copyright (C) 2020 Richard Bettridge under GPLv3 license.

https://github.com/RolandJuno/esp8266_modem

Copyright (C) 2016 Paul Rickards <rickards@gmail.com> under GPLv3 license.

https://github.com/jsalin/esp8266_modem

Copyright (C) 2015 Jussi Salin <salinjus@gmail.com> under GPLv3 license.

## Overview

The WiFi Serial Modem attaches to a standard RS-232 device to provide the
functionality of a modem using telnet to connect to other machines or 
services.

The modem supports most standard Hayes AT commands, as well as some commands
that are specific to the implementation.

## Why?

The machines I'm working on are largely older hardware, and most do not
support ethernet networking.  Since nearly all machines going back to the
late 1970s have (or can be coerced to have) some form of serial port and
some available dial-up telecommunications software, this is a reasonable
way to manage software on the machines.

## Alternatives

There are a few different projects that do much the same as this project.
The reason I chose to build this was that the implementations were either
lacking a feature I wanted, or had a number of features I did not need.
My goal was to keep the sketch as simple as possible, while providing all
of the things I needed it to do.  For me, this implementation has hit the
'sweet spot'.

## Loading the Firmware

The sketch can be loaded using the Arduino IDE.  I have found that the
configuration represented in the fritzing files will fail to connect to
the ESP8266 modules I have on hand unless the serial module is disconnected.

You will need to set up the Arduino IDE to use the ESP8266 boards.  There
are a number of resources on the internet that can walk you through the 
process.

## Supported Features

- Hayes-style AT Command Set
- All 'standard' baud rates from 110 to 115200
- Single-sketch simplicity
- LED Indicators
- 'Panic Button' : A momentary switch will change the baud
  rate back to the default (9600 as written).
- Minimal component count (1 microcontroller, 1 serial module, 5 resistors,
  4 LEDs, and a momentary SPDT switch).
- Minimal component cost - All the parts are available for less than US$20.

## Command Set
```
  DIAL HOST            : ATDTHOST:PORT
  SPEED DIAL           : ATDSN (N=0-9)
  SET SPEED DIAL       : AT&ZN=HOST:PORT (N=0-9)
  HANDLE TELNET        : ATNETN (N=0,1)
  NETWORK INFO         : ATI
  AUTO ANSWER          : ATS0=N (N=0,1)
  SET BUSY MSG         : AT$M=YOUR BUSY MESSAGE
  LOAD NVRAM           : ATZ
  SAVE TO NVRAM        : AT&W
  RESET NVRAM TO DFLTS : ATNVZ
  SHOW SETTINGS        : AT&V
  FACT. DEFAULTS       : AT&F
  PIN POLARITY         : AT&PN (N=0/INV,1/NORM)
  ECHO OFF/ON          : ATE0 / ATE1
  QUIET MODE OFF/ON    : ATQ0 / ATQ1
  VERBOSE OFF/ON       : ATV0 / ATV1
  SET SSID             : AT$SSID=WIFISSID
  SET PASSWORD         : AT$PASS=WIFIPASSWORD
  GET SIGNAL STRENGTH  : AT%Q
  CHECK LIGHTS         : AT$LC
  SET BAUD RATE        : AT$B=N (110,300,1200,2400,4800,9600
                                 19200,38400,57600,115200
  FLOW CONTROL         : AT&KN (N=0/N,1/HW,2/SW)
  WIFI OFF/ON          : ATC0 / ATC1
  HANGUP               : ATH
  ENTER CMD MODE       : +++
  EXIT CMD MODE        : ATO
  REPEAT LAST CMD      : /A (No AT needed)
```
## Getting Started

After the hardware is put together, and the sketch is loaded,
follow these steps:

- Configure the serial port on the computer or terminal to 9600 baud.
- If attached to a computer, run your terminal software
- In terminal mode, set up your wifi connection using:

  AT$SSID=YourWifiSSID

  AT$PASS=YourWifiPassword

- After a successful connection, you can telnet to other machines or
  services using the Hayes dial commands, as follows:

  ATDT bbs.anachronet.com:2300

  Using the site name and telnet port, separated by a colon (:).

## Where to Buy

You can't - I am not now, nor do I intend to sell the modem.  However, I have
provided a Fritzing file that you can use to order circuit boards from
Aisler, or another board maker of your choice.  The R1V2 design uses the
micro USB port on the ESP8266 to power the circuit.  I suggest using female
headers to attach the ESP8266 and serial module so that they can be 
removed/replaced easily. 

If you want a fully-constructed board using the firmware that I based this
project on, I would suggest the following:

TheOldNet Store

https://www.tindie.com/products/theoldnet/rs232-serial-wifi-modem-for-vintage-computers-v3/

His product is similar in many ways from a software perspective, and has
some consideration for PET/C64 functionality.  I have not included any
functionality for them since this project is meant to support a group of
machines that does not include any hardware that needs Petscii.

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


