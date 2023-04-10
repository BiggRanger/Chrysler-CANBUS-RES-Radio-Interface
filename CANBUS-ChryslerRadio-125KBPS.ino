#include <CAN.h>

/*
 * Darren Clark - BiggRanger@gmail.com
 * 2021.12.05
 * Project to experiment with the Chrysler high speed CAN RES radio
 * This radio uses CAN-B which is 125 kbps.
 * 
 * Dependencies:
 * CAN Library by Sandeep Mistry V 0.3.1
 * https://github.com/sandeepmistry/arduino-CAN
 * 
 * 
 * 1. On Arduino connect a 120 Ohm resistor from CAN-L to CAN-H.
 *    The CAN bus needs to be terminated, and a 120 Ohm resistor is good enough.
 * 2. Connect radio ground to Arduino ground.
 */


volatile uint8_t timeH = 0, timeM = 0, timeS = 0;  //The radio does not keep time, it only sets and displays time.

uint8_t keyState = 0x41;                  //initial state = key-in, accessory on
uint8_t lightsDriving = 0x02;             //initial state = dash illuminated
uint8_t lightsDashIntensity = 0xC8;       //initial state = max illimunation

String SerialRXBuffer = "";
bool SerialRXSpecial = false;

void setup()
{
  //Set timer 1 to fire interrupt at 1HZ interval
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = 62500;            // compare match register 16MHz/256
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();
  
  Serial.begin(1000000);

  CAN.setPins(10, 2);
  CAN.setClockFrequency(8E6);
 
  if (!CAN.begin(125E3))      //start the CAN bus at 125 kbps
  {
    Serial.println("Starting CAN0 failed!");
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

  if ( keyState != 0x00 )
  {
    canSend(0x20B, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);                         //Key Position - 00 = no key, 01 = key in, 21 = accessory, 81 = run, A1 = start
    canSend(0x219, 3, 131, 0, 192, 16, 44, 8, 0); delay(5);                                   //VIN - This needs to be here to keep the radio on or else it will shutoff after ~15 seconds
    canSend(0x208, lightsDriving, lightsDashIntensity, 0x00, 0x00, 0x00, 0x00); delay(5);     //Illumination information B0 = driving lights (0,1,2,3), B1 = dash intensity (00-C8)
    canSend(0x3E6, timeH, timeM, timeS); delay(5);                                            //Clock data, if this isn't here radio returns "no clock"
    canSend(0x3A3,0x0,0x0,0x0,0x0,0x0,0x0);                                                   //Steering wheel buttons for radio
    canSend(0x21F,0x1,0x0,0x50,0x0,0x0,0x0,0x0,0x40);                                         //Unknonw, but necessary
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
    case 0x293:
      Serial.print("0x293 Display1 - Mode: ");
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
  
    case 0x291:
      Serial.print("0x291 Display2 - Mode: ");
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
      
    case 0x3D9:
      Serial.print("0x3D9 Settings - Volume: "); Serial.print(parameters[0]);
      Serial.print(" Balance: "); Serial.print(parameters[1]);
      Serial.print(" Fader: "); Serial.print(parameters[2]);
      Serial.print(" Bass: "); Serial.print(parameters[3]);
      Serial.print(" Mid: "); Serial.print(parameters[4]);
      Serial.print(" Treble: "); Serial.print(parameters[5]);
      Serial.print(" UNK: "); Serial.println(parameters[6]);
      break;
      
    case 0x2E9:
      if ( (parameters[0] & 0x0F) == 0x03 )
      {
        timeH = parameters[1];
        timeM = parameters[2];
        timeS = 0;
        Serial.print("0x2E9 B[0]= "); Serial.print(parameters[0], HEX); Serial.print(" SetTime - H: "); Serial.print(timeH); Serial.print(" M: "); Serial.print(timeM); Serial.print(" S: "); Serial.println(timeS); 
      }
      break;

    case 0x416:
      Serial.print("0x416 Status: "); Serial.print(parameters[0]);
      Serial.print(" : "); Serial.print(parameters[1]);
      Serial.print(" : "); Serial.print(parameters[2]);
      Serial.print(" : "); Serial.print(parameters[3]);
      Serial.print(" : "); Serial.print(parameters[4]);
      Serial.print(" : "); Serial.print(parameters[5]);
      Serial.print(" : "); Serial.print(parameters[6]);
      Serial.print(" : "); Serial.println(parameters[7]);
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
        canSend(0x20B, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'O' || RX == 'o' ) //power off
      {
        keyState = 0x00;
        canSend(0x20B, keyState, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if (RX == '+')  //volume up
      {
        canSend(0x3A3, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if (RX == '-')  //volume down
      {
        canSend(0x3A3, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'U' || RX == 'u' ) //scan up
      {
        canSend(0x3A3, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'D' || RX == 'd' ) //scan down
      {
        canSend(0x3A3, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'B' || RX == 'b' ) //switch bands
      {
        canSend(0x3A3, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'P' || RX == 'p' ) //preset increase
      {
        canSend(0x3A3, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
      }
      if ( RX == 'R' || RX == 'r' ) //preset decrease
      {
        canSend(0x3A3, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00); delay(50);
        canSend(0x3A3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); delay(5);
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
        timeH = atol(tempArray);
        tempVal = SerialRXBuffer.substring(3,5);
        tempVal.toCharArray(tempArray,sizeof(tempArray));
        timeM = atol(tempArray);
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
