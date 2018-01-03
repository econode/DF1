#include <DF1.h>

#define DF1destination 1
#define DF1baud 9600
#define DF1format SERIAL_8N1
#define DF1timeout 1000


enum
{
  DF1PACKET1,
  DF1PACKET2,
  DF1TOTAL_NO_OF_PACKETS
};
DF1Packet DF1packets[DF1TOTAL_NO_OF_PACKETS];

DF1packetPointer DF1packet1 = &DF1packets[DF1PACKET1];
DF1packetPointer DF1packet2 = &DF1packets[DF1PACKET2];

unsigned int DF1readRegs[1];

unsigned int DF1writeRegs[1];

#define AnalogOutputPin 9


void setup() {
DF1_construct(DF1packet1, DF1destination, DF1_READ_N7, 0, 1, DF1readRegs);
  
DF1_construct(DF1packet2, DF1destination, DF1_WRITE_N7, 1, 1, DF1writeRegs);

DF1_configure(&Serial, DF1baud, DF1format, DF1timeout, DF1packets, DF1TOTAL_NO_OF_PACKETS);

}

void loop() {
  DF1_update();

  //wrire N7:1
  int sensorValue = analogRead(A0);
  int outputValue = map(sensorValue, 0, 1023, 0, 32767);
  DF1writeRegs[0] = outputValue;

  //Read N7:0
  unsigned int N7_0 = DF1readRegs[0];
  N7_0 = map(N7_0, 0, 32000, 0, 255); 
  analogWrite(AnalogOutputPin, N7_0);

}


