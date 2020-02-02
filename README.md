# Chrysler-CANBUS-RES-Radio-Interface

This is part of a reverse engineering project where I am documenting the CANBUS messages from the following vehicles:

* 2006 Jeep Grand Cherokee
* 2008 Dodge Durango
* 2010 Jeep Wrangler

I’m documenting both the CAN-C and CAN-IHS bus on these vehicles, and found a lot of similarity between the CAN-C messages. The CAN-IHS messages between the 2008 Durango and 2010 Wrangler are completely different. This work will be published at a later date.  
  
**Project summary:**  
This project utilizes a Chrysler RES radio from a 2008 Dodge Durango, other Chrysler radios from this era should be similar. The 2008 radio connects to a 83.333Kbps CANBUS (the 2010 Jeep Wrangler uses a 125Kbps CANBUS with different CAN message identifiers).  
  
The point of this project is to activate the radio so it will turn on and remotely change radio stations and the volume by simulating the steering wheel buttons.

![Radio ON](https://github.com/BiggRanger/Chrysler-CANBUS-RES-Radio-Interface/blob/master/RadioON.jpg)
  
  
**Project hardware:**  
* 2008 Dodge Durango RES model radio with wire harness.
* Arduino UNO CAN module (CAN-BUS Shield V1.2 10/10/2013 By: ElecFreaks Sourced from amazon.com)
* 120Ohm 1/4W resistor
* 12V 10A power supply

![Hardware Setup](https://github.com/BiggRanger/Chrysler-CANBUS-RES-Radio-Interface/blob/master/RadioWiring.jpg)
  
  
**Project Software:**  
In addition to the Arduino IDE you will also need to install the following CAN library:
 CAN Library by Sandeep Mistry V 0.3.1
 https://github.com/sandeepmistry/arduino-CAN

This library will also need to be modified since it (curently V 0.3.1) does not support the 83.333Kbps speed.
See the notes in the CANBUS-ChryslerRadio.ino source file for updating the CAN library.
  
  
**Connection Notes:**  
The interior 83.333Kbps CAN is a lowspeed fault tolerant bus, and must be connected in the following way to work with the Arduino CAN module.
   
1. From radio leave CAN-L (white) disconnected, connect CAN-H (White/Red) to CAN-H on Arduino.
2. On Arduino connect a 120 Ohm resistor from CAN-L to ground.
3. Connect radio ground to Arduino ground.

![CAN Setup](https://github.com/BiggRanger/Chrysler-CANBUS-RES-Radio-Interface/blob/master/Radio-CANBUS.jpg)
  
  
**Interesting Findings:**  
1. The radio does not keep the time, it only sets the time and displays the time. The time is actually kept on the SKREEM module and broadcast every second with the message ID 0x3EC. When setting the time the radio sends a message over CAN ID 0x0F0 which the SKREEM module is listening for. In this project the Arduino takes the place of the SKREEM module to receive the time settings and broadcast the current time.  
2. In addition to the 0x000 key state message, the radio needs to receive several other messages in order to operate properly.   
  
  
**Controllong the radio from the Arduino UI:**  
Note, the BAUD rate for the serial port is set to 1000000  
Commands:  
* I - Power on (project defaults to a powered on state)
* O - Power Off
* L - Brighten the radio display, turn off button backlighting
* K - Turn on button backlighting, dim display
* \> - Increase backlighting intensity
* \< - Decrease backlighting intensity
* \+ - Increase volume
* \- - Decrease volume
* U - Scan channels up
* D - Scan channels down
* B - Switch bands (AM/FM/CD)
* P - Select next station preset
* R - Select previous station preset
  
![Arduino Terminal](https://github.com/BiggRanger/Chrysler-CANBUS-RES-Radio-Interface/blob/master/Selection_001.png)








