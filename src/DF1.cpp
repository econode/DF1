#include "DF1.h"
#include "HardwareSerial.h"


#define DF1_IDLE 1
#define DF1_WAITING_FOR_REPLY 2
#define DF1_WAITING_FOR_TURNAROUND 3

#define DF1_BUFFER_SIZE 128

unsigned char DF1state;


unsigned char msgframe[DF1_BUFFER_SIZE]; 
unsigned char msgbuffer;
unsigned int DF1timeout; 
unsigned int DF1_T1; 
unsigned long DF1delayStart; 
unsigned int DF1_total_no_of_packets; 
DF1Packet* DF1packetArray; 
DF1Packet* DF1packet; 
HardwareSerial* DF1Port;


void DF1idle();
void DF1constructPacket();
unsigned char construct_WRITE_N7();
void DF1waiting_for_reply();
void DF1processReply();
void DF1waiting_for_turnaround();
void process_READ_N7();
void process_WRITE_N7();
void DF1processError();
void DF1processSuccess();
void DF1sendPacket(unsigned char bufferSize);
unsigned int CalculateCRC16(unsigned char n);
unsigned char Add_0X_10(unsigned char frameSize);


unsigned int TNS;
void ACKsend();
void STXsend();
void ENQsend();
void NAKsend();



void DF1_update() 
{
	switch (DF1state)
	{
		case DF1_IDLE:
		DF1idle();
		break;
		case DF1_WAITING_FOR_REPLY:
		DF1waiting_for_reply();
		break;
		case DF1_WAITING_FOR_TURNAROUND:
		DF1waiting_for_turnaround();
		break;
	}
}

void DF1idle()
{
    static unsigned int packet_index;	
	
	unsigned int failed_connections = 0;
	
	unsigned char current_connection;
	
	do
	{		
		if (packet_index == DF1_total_no_of_packets) // wrap around to the beginning
			packet_index = 0;
				
		// proceed to the next DF1packet
		DF1packet = &DF1packetArray[packet_index];
		
		// get the current DF1connection status
		current_connection = DF1packet->DF1connection;
		
		if (!current_connection)
		{			
			// If all the DF1connection attributes are false return
			// immediately to the main sketch
			if (++failed_connections == DF1_total_no_of_packets)
				return;
		}
		packet_index++;     
    
	// if a DF1packet has no DF1connection get the next one		
	}while (!current_connection); 
		
	DF1constructPacket();
}
  
void DF1constructPacket()
{	
  DF1packet->DF1requests++;
  DF1packet->DF1error=99;
	TNS++;	
 
  msgframe[0] = DF1packet->DST;
  msgframe[1] = 0x00;
  msgframe[2] = 0x0F;
  msgframe[3] = 0x00;

  msgframe[4] = TNS & 0x0FF;
  msgframe[5] = TNS >> 8;

  msgframe[6] = DF1packet->FNC;
  
  msgframe[7] = (DF1packet->SIZE * 2) & 0x0FF;

	msgframe[8] = 0x07;
	//* File Type, 0x89 = Integer
	msgframe[9] = 0x89;
	//Starting Element Number
	msgframe[10] = DF1packet->ADDR & 0xFF; // ADDR Lo	
	
	
	unsigned char frameSize;    
	

	if (DF1packet->FNC == DF1_WRITE_N7) {
		frameSize = construct_WRITE_N7();	
	}else if (DF1packet->FNC == DF1_READ_N7){
	    frameSize=11;    
		
	}
    
	unsigned int CheckSumCalc;	      
    CheckSumCalc = CalculateCRC16(frameSize);
	
	frameSize = Add_0X_10(frameSize);	
	
  msgframe[frameSize - 4] = 0x010;
  msgframe[frameSize - 3] = 0x03;
  msgframe[frameSize - 2] = CheckSumCalc & 0xFF; 
  msgframe[frameSize - 1] = CheckSumCalc >> 8; 
  DF1sendPacket(frameSize);

  DF1state = DF1_WAITING_FOR_REPLY;

	if (DF1packet->DST == 0)
			DF1processSuccess();
}

unsigned char Add_0X_10(unsigned char frameSize)
{
			unsigned char cntx10 = 0;
			for (unsigned char i = 0; i < frameSize; i++)
			{
				if (msgframe[i] == 0x10) cntx10++;
			}

			if (cntx10 > 0)
			{
				unsigned char tempframeSize = frameSize + cntx10;
				byte tempframe[tempframeSize];

				unsigned char z = 0;
				for (unsigned char i = 0; i < frameSize; i++)
				{
					tempframe[z] = msgframe[i];
					if (msgframe[i] == 0x10)
					{
						z++;
						tempframe[z] = msgframe[i];
					}
					z++;
				}
			
			for (unsigned char i = 0; i < tempframeSize; i++)
			{
				msgframe[i] = tempframe[i];
			}
			
				return tempframeSize+4; 
				
			}
			else
			{			
				return frameSize+4; 
			}	
}

unsigned char construct_WRITE_N7()
{
	unsigned char no_of_bytes = DF1packet->SIZE * 2; 
    

  msgframe[7] = no_of_bytes; // number of bytes
  msgframe[11] = 0x00;
  unsigned char index = 12; // user SIZE starts at index 12
	unsigned char no_of_registers = DF1packet->SIZE;
	unsigned int temp;
		
  for (unsigned char i = 0; i < no_of_registers; i++)
  {
    temp = DF1packet->DATA_ARRAY[i];
    msgframe[index] = temp & 0xFF;
    index++;
    msgframe[index] = temp >> 8;
    index++;
  }
	unsigned char frameSize = (12 + no_of_bytes);
	return frameSize;
}

void DF1waiting_for_turnaround()
{
		DF1state = DF1_IDLE;
}


void DF1waiting_for_reply()
{
	bool x10 = false;
	bool start0 = true;
	if ((*DF1Port).available()) // is there something to check?
	{
		unsigned char overflowFlag = 0;
		msgbuffer = 0;		
		while ((*DF1Port).available())
		{

			if (overflowFlag) 
				(*DF1Port).read();
			else
			{
				if (msgbuffer == DF1_BUFFER_SIZE)
					overflowFlag = 1;
			
				msgframe[msgbuffer] = (*DF1Port).read();
				if(x10 && msgframe[msgbuffer]==0x10){
					x10 = false;
					msgbuffer--;
				}else{
					if(msgframe[msgbuffer]==0x10)x10 = true;
					if(x10 && msgframe[msgbuffer]!=0x10)x10 = false;
				}	
				
				msgbuffer++;
				
				if(msgbuffer>3 && start0){
					if(msgframe[msgbuffer-2]==0x10 && msgframe[msgbuffer-1]==0x02){
						msgbuffer=0;
						x10 = false;							
					}
				start0 = false;					
				}
			}

			delayMicroseconds(DF1_T1); // inter character time out
		}
		
		
		if ((msgbuffer < 5) || overflowFlag)
			DF1processError();       
      

		else if (msgframe[0] == 0x00 && msgframe[1] == DF1packet->DST && msgframe[2] == 0x4F && msgframe[3] == 0x00)
			DF1processReply();
		else
			DF1processError();
			
	}
	else if ((millis() - DF1delayStart) > DF1timeout) // check DF1timeout
	{
		DF1processError();
		DF1state = DF1_IDLE;
	}
}

void DF1processReply()
{
			
  unsigned int received_crc = ((msgframe[msgbuffer - 1] << 8) | msgframe[msgbuffer - 2]); 
  unsigned int calculated_crc = CalculateCRC16(msgbuffer-4);

	
	if (calculated_crc == received_crc) // verify checksum
	{

		if (msgbuffer>10) // >10 READ
		{
			switch (DF1packet->FNC)
			{
			case DF1_READ_N7:
			process_READ_N7();
			break;
			default:
			DF1processError();
			break;
			}
		}
		else
		{
			switch (DF1packet->FNC) 
			{
			case DF1_WRITE_N7:
			process_WRITE_N7();
			break;
			default:
			DF1processError();
			break;
			}
		}
	
		
	} 
	else // checksum failed
	{
		DF1processError();
	}
}



void process_READ_N7()
{

  if ((msgbuffer-10) == (DF1packet->SIZE * 2)) 
  {

    unsigned char index = 6;
    for (unsigned char i = 0; i < DF1packet->SIZE; i++)
    {
      DF1packet->DATA_ARRAY[i] = (msgframe[index+1] << 8) | msgframe[index]; 
      index += 2;
    }
    DF1processSuccess(); 
  }
  else
    DF1processError();  
}

void process_WRITE_N7()
{
	
  if (msgbuffer==10)
    DF1processSuccess();
  else
    DF1processError();
}

void DF1processError()
{
	DF1packet->DF1error=1;
	DF1packet->DF1failed_requests++;	
	DF1state = DF1_WAITING_FOR_TURNAROUND;
	DF1delayStart = millis(); // start the turnaround delay
}

void DF1processSuccess()
{
	DF1packet->DF1error=0;
	DF1packet->DF1successful_requests++; // transaction sent successfully
	DF1state = DF1_WAITING_FOR_TURNAROUND;
	DF1delayStart = millis(); // start the turnaround delay
}
  
void DF1_configure(HardwareSerial* SerialPort,
											long DF1baud,
											unsigned char DF1byteFormat,
											unsigned int _DF1timeout, 
											DF1Packet* _DF1packets, 
											unsigned int _DF1total_no_of_packets)
{ 
	
	if (DF1baud > 19200)
		DF1_T1 = 750; 
	else 
		DF1_T1 = 16500000/DF1baud; // 1T * 1.5 = T1.5
	
	// initialize
	DF1state = DF1_IDLE;
	DF1timeout = _DF1timeout;
	DF1_total_no_of_packets = _DF1total_no_of_packets;
	DF1packetArray = _DF1packets;
		
	DF1Port = SerialPort;
	(*DF1Port).begin(DF1baud, DF1byteFormat);
	
} 

void DF1_construct(DF1Packet *_DF1packet, 
											unsigned char DST, 
											unsigned char FNC, 
											unsigned int ADDR, 
											unsigned int SIZE, 
											unsigned int* DATA_ARRAY)
{
	_DF1packet->DST = DST;
	_DF1packet->FNC = FNC;
	_DF1packet->ADDR = ADDR;
	_DF1packet->SIZE = SIZE;
	_DF1packet->DATA_ARRAY = DATA_ARRAY;
	_DF1packet->DF1connection = 1;
}

void DF1sendPacket(unsigned char bufferSize)
{
	STXsend();
	
	for (unsigned char i = 0; i < bufferSize; i++)
		(*DF1Port).write(msgframe[i]);		
		
	ACKsend(); 
	
	(*DF1Port).flush();
		
	DF1delayStart = millis(); // start the DF1timeout delay	
}

unsigned int CalculateCRC16(unsigned char n){
            unsigned int iCRC = 0;
            byte bytT = 0;
            for (unsigned int i = 0; i < n; i++)
            {
                bytT = (byte)((iCRC & 0xff) ^ msgframe[i]);
                iCRC = (unsigned int)((iCRC >> 8) ^ CRC16table[bytT]);

            }
            bytT = (byte)((iCRC & 0xff) ^ 3);
            iCRC = (unsigned int)((iCRC >> 8) ^ CRC16table[bytT]);

            return iCRC;			
}

void ACKsend()
{
	for (byte i=0 ; i < 2 ; i++)
	{
		(*DF1Port).write(ACK[i]);
	}
}

void STXsend()
{
	for (byte i=0 ; i < 2 ; i++)
	{
		(*DF1Port).write(STX[i]);
	}
}

void ENQsend()
{
	for (byte i=0 ; i < 2 ; i++)
	{
		(*DF1Port).write(ENQ[i]);
	}
}

void NAKsend()
{
	for (byte i=0 ; i < 2 ; i++)
	{
		(*DF1Port).write(NAK[i]);
	}
}