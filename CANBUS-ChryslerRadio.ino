#include <CAN.h>

/*#ifndef ARDUINO_AVR_UNO
#error "This file is for the Arduino UNO only"
#endif*/

/*
 * Darren Clark - BiggRanger@tds.net
 * 2019.12.29
 * Project to experiment with the Chrysler RES radio from a 2008 Dodge Durango
 * This radio uses CAN-IHS which is 83.333 kbps and low speed.
 * Newer radios use CAN-B which is 125 kbps.
 * 
 * Dependencies:
 * CAN Library by Sandeep Mistry V 0.3.1
 * https://github.com/sandeepmistry/arduino-CAN
 * 
 * 
 * Notes: 83.333K is not part of the CAN library.
 *    edit MCP2515.cpp and change the following:
 * 
  const struct 
  {
    long clockFrequency;
    long baudRate;
    uint8_t cnf[3];
  } CNF_MAPPER[] = { {  (long)8E6,  (long)500E3, { 0x00, 0x90, 0x02 } },
                     {  (long)8E6,  (long)125E3, { 0x01, 0xb1, 0x05 } },
                     {  (long)8E6,   (long)83E3, { 0x01, 0xbe, 0x07 } },
                     {  (long)8E6,   (long)80E3, { 0x01, 0xbf, 0x07 } },
                 
                     { (long)16E6,  (long)500E3, { 0x00, 0xf0, 0x06 } },
                     { (long)16E6,  (long)125E3, { 0x03, 0xf0, 0x06 } },
                     { (long)16E6,   (long)83E3, { 0x03, 0xfe, 0x07 } },
                     { (long)16E6,   (long)80E3, { 0x03, 0xff, 0x07 } }, };

 * 
 * 
 * 
 * Notes: 83.333K is fault tolerant and low speed.
 * 1. From radio leave CAN-L disconnected, connect CAN-H to CAN-H on Arduino
 * 2. On Arduino connect a 120 Ohm resistor from CAN-L to ground.
 * 3. Connect radio ground to Arduino ground.
 */


volatile uint8_t timeH = 0, timeM = 0, timeS = 0;  //The radio does not keep time, it only sets and displays time.

uint8_t keyState = 0x41;                  //initial state = key-in, accessory on
uint8_t lightsDriving = 0x02;             //initial state = dash illuminated
uint8_t lightsDashIntensity = 0xC8;       //initial state = max illimunation

String SerialRXBuffer = "";
bool SerialRXSpecial = false;

void setup()
{
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 62500;            // compare match register 16MHz/256
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();
  
  Serial.begin(1000000);

  //these settings are dependant on the CAN module being used. CS could be pin 9 or 10
  //the clockk can be 8 or 16 MHz
  CAN.setPins(10, 2);
  //CAN.setPins(9, 2);
  CAN.setClockFrequency(8E6);
  //CAN.setClockFrequency(16E6);
 
  if (!CAN.begin(83E3))      //start the CAN bus at 83.333 kbps
  {
    Serial.println("Starting CAN0 failed! See notes in source about 83.333 kbps");
    while (1);
  }
  
  //CAN.filter(0x3D0); //turn off filter and capture everything.

  CAN.onReceive(onCANReceive);

  Serial.println("FCA Radio Tool");
}

ISR(TIMER1_COMPA_vect)
{
  timeS++;
  if (timeS > 59)
  {
    timeS = 0;
    timeM++;
  }
  if (timeM > 59)
  {
    timeM = 0;
    timeH++;
  }
  if (timeH > 23)
    timeH = 0;
}

void loop()
{
  for (uint16_t y = 0; y < 900; y++)  //~900mS delay while checking serial.
  {
    delay(1);
    checkSerial();
  }

  if ( keyState == 0x00 )
    delay(30);
  else
  {
    canSend(0x000, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);                         //key position 00 = no key, 01 = key in, 41 = accessory, 81 = run, 21 = start
    canSend(0x015, 85, 121, 6, 255, 0,0); delay(5);                                           //this needs to be here to turn the radio on initially. ECM data (voltage, + 2 other plots, FF, 00, 00)
    canSend(0x1AF, 3, 131, 0, 192, 16, 44, 8, 0); delay(5);                                   //this needs to be here to keep the radio on or else it will shutoff after ~15 seconds (B0 changes from 3 to 1 to 3, the rest is static)
    canSend(0x210, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);     //illumination information B0 = driving lights (0,1,2,3), B1 = dash intensity (00-C8)
    canSend(0x3EC, timeH, timeM, timeS); delay(5);                                            //clock data, if this isn't here radio returns "no clock"
    canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);                             //steering wheel button states from cluster
  }
}

void onCANReceive(int packetSize) 
{
  if (CAN.packetRtr()) 
  {
    Serial.print("RTR ID 0x");
    Serial.print(CAN.packetId(), HEX);
    Serial.print(" Requested Length ");
    Serial.println(CAN.packetDlc());
    return;
  }

  uint8_t parameters[8];
  uint32_t packetID = CAN.packetId();
  for (uint8_t x = 0; x < packetSize; x++)
    parameters[x] = CAN.read();
        
  switch (packetID)
  {
    case 0x18C:
      Serial.print("0x18C Display1 - Mode: ");
      if (parameters[0] == 0) Serial.print("ON");
      if (parameters[0] == 0x10) Serial.print("Standby");
      Serial.print(" Frequency/Track: "); Serial.print((float)(parameters[1] * 256.0 + parameters[2]) / 10.0, 1);
      Serial.print(" Preset: "); Serial.print( 0x0F & parameters[3]);
      if (parameters[4] == 0x02)
        Serial.print(" CD: Inserted ");
      else
        Serial.print(" CD: Empty ");
      Serial.print(" CD-H: "); Serial.print(parameters[5]);
      Serial.print(" CD-M: "); Serial.print(parameters[6]);
      Serial.print(" CD-S: "); Serial.println(parameters[7]);
      break;
  
    case 0x190:
      Serial.print("0x190 Display2 - Mode: ");
      if (parameters[0] == 0x00) Serial.print("AM ");
      if (parameters[0] == 0x01) Serial.print("FM ");
      if (parameters[0] == 0x1D) Serial.print("Standby ");
      if (parameters[0] == 0x04) Serial.print("CD ");
      if (parameters[0] == 0x06) Serial.print("AUX ");
      Serial.print("CD: ");
      if (parameters[1] == 0x01) Serial.print("Standby ");
      if (parameters[1] == 0x03) Serial.print("Playing ");
      if (parameters[1] == 0x0D) Serial.print("Empty ");
      Serial.print("Source: ");
      if (parameters[2] == 0x03) Serial.print("AUX ");
      if (parameters[2] == 0x05) Serial.print("Radio ");
      Serial.print("AMP: ");
      if (parameters[4] == 0x00) Serial.println("OFF ");
      if (parameters[4] == 0x10) Serial.println("ON ");
      if (parameters[4] == 0x80) Serial.println("Mute ");
      break;
      
    case 0x3D0:
      Serial.print("0x3D0 Settings - Volume: "); Serial.print(parameters[0]);
      Serial.print(" Balance: "); Serial.print(parameters[1]);
      Serial.print(" Fader: "); Serial.print(parameters[2]);
      Serial.print(" Bass: "); Serial.print(parameters[3]);
      Serial.print(" Mid: "); Serial.print(parameters[4]);
      Serial.print(" Treble: "); Serial.print(parameters[5]);
      Serial.print(" UNK: "); Serial.println(parameters[6]);
      break;
      
    case 0x0F0:
      if ( (parameters[0] & 0x0F) == 0x03 )
      {
        timeH = parameters[1];
        timeM = parameters[2];
        timeS = 0;
        Serial.print("0x0F0 B[0]= "); Serial.print(parameters[0], HEX); Serial.print(" SetTime - H: "); Serial.print(timeH); Serial.print(" M: "); Serial.print(timeM); Serial.print(" S: "); Serial.println(timeS); 
      }
      break;

    case 0x0EC:
      //this is part of the HVAC contorl panel
      Serial.print("0x0EC Settings - HVAC A/C-Defost: "); 
      Serial.print("A/C 1: ");
      Serial.print(0x1 & parameters[0]);
      Serial.print(" A/C 2: ");
      Serial.print((0x40 & parameters[0]) >> 6);
      Serial.print(" Rear Defrost: ");
      Serial.println((0x80 & parameters[0]) >> 7);
      break;
     
    case 0x1F8:
      //this is part of the HVAC contorl panel
      Serial.print("0x1F8 Settings - HVAC rear Wiper: Speed ");
      Serial.print(0x0F & parameters[0]);
      Serial.print(" Washer: ");
      Serial.println((0x10 & parameters[0]) >> 4);
      break;

    case 0x411:
    case 0x416:
      break;
      
    default:
      //Output information from unexpected packets
      Serial.print("0x");
      Serial.print(packetID, HEX);
      Serial.print(" defaulted-ID size: ");
      Serial.print(packetSize);
      for (uint8_t x = 0; x < packetSize; x++)
      {
        Serial.print(" 0x"); Serial.print(parameters[x], HEX);
      }
      Serial.println();
  }
}

void checkSerial()
{
  if (Serial.available())
  {
    char RX = Serial.read();
    if (!SerialRXSpecial)
    {
      if ( RX == 'I' || RX == 'i' ) //power on
      {
        keyState = 0x41;
        canSend(0x000, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'O' || RX == 'o' ) //power off
      {
        keyState = 0x00;
        canSend(0x000, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
  
      if ( RX == 'L' || RX == 'l' ) //lights
      {
        lightsDriving = 0x03;
        lightsDashIntensity = 0x00;      
        canSend(0x210, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
  
      if ( RX == 'K' || RX == 'k' ) //dash lights
      {
        lightsDriving = 0x02;
        lightsDashIntensity = 0xC8;      
        canSend(0x210, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
  
      if ( RX == '>') //dash light intensity up
      {
        lightsDashIntensity += 5;
        if ( lightsDashIntensity > 0xC8 )
          lightsDashIntensity = 0xC8;
        canSend(0x210, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
  
      if (RX == '<') //dash light intensity down
      {
        lightsDashIntensity -= 5;
        if ( lightsDashIntensity < 0x10 )
          lightsDashIntensity = 0x10;
        canSend(0x210, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
  
      if (RX == '+')  //volume up
      {
        canSend(0x3A0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if (RX == '-')  //volume down
      {
        canSend(0x3A0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'U' || RX == 'u' ) //scan up
      {
        canSend(0x3A0, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'D' || RX == 'd' ) //scan down
      {
        canSend(0x3A0, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'B' || RX == 'b' ) //switch bands
      {
        canSend(0x3A0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'P' || RX == 'p' ) //preset increase
      {
        canSend(0x3A0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'R' || RX == 'r' ) //preset decrease
      {
        canSend(0x3A0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'T' || RX == 't' ) //set time
      {
        SerialRXBuffer = RX;
        SerialRXSpecial = true;
      }
    }
    else
    {
      SerialRXBuffer += RX;
      if (SerialRXBuffer.length() >= 5)
      {
        String tempVal = "";
        char tempArray[8];
        tempVal = SerialRXBuffer.substring(1,3);
        tempVal.toCharArray(tempArray,sizeof(tempArray));
        timeH = strtol(tempArray, 0, 0);
        tempVal = SerialRXBuffer.substring(3,5);
        tempVal.toCharArray(tempArray,sizeof(tempArray));
        timeM = strtol(tempArray, 0, 0);
        SerialRXBuffer = "";
        SerialRXSpecial = false;
      }
    }
  }
}


//helper functions...

void canSend(uint32_t ID, uint8_t b0)
{
  uint8_t b[1];
  b[0] = b0;
  canTX(1, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1)
{
  uint8_t b[2];
  b[0] = b0; b[1] = b1;
  canTX(2, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2)
{
  uint8_t b[3];
  b[0] = b0; b[1] = b1; b[2] = b2;
  canTX(3, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
  uint8_t b[4];
  b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3;
  canTX(4, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
  uint8_t b[5];
  b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; b[4] = b4;
  canTX(5, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
  uint8_t b[6];
  b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; b[4] = b4; b[5] = b5;
  canTX(6, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6)
{
  uint8_t b[7];
  b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; b[4] = b4; b[5] = b5; b[6] = b6;
  canTX(7, ID, b);
}
void canSend(uint32_t ID, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
{
  uint8_t b[8];
  b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; b[4] = b4; b[5] = b5; b[6] = b6; b[7] = b7; 
  canTX(8, ID, b);
}

void canTX(uint8_t packetSize, uint32_t ID, uint8_t b[])
{
  CAN.beginPacket(ID, packetSize);
  CAN.write(b, packetSize);
  CAN.endPacket();
}
