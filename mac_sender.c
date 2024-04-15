#include "main.h"
#include <stdio.h>
#include <string.h>



void sendTestMessage()
{
	struct queueMsg_t queueMsg;		// queue message
	uint8_t * msg;
	char testMessage[] = "Hello World";
	uint8_t destAddr = 10;
	uint8_t destSAPI = CHAT_SAPI;
	size_t	length = sizeof(testMessage) - 1;
	bool read  = true;
	bool ack = true;
	osStatus_t retCode;
	
	//------------------------------------------------------------------------------
	for (;;)											// loop until doomsday
	{
		//----------------------------------------------------------------------------
		// MEMORY ALLOCATION				
		//----------------------------------------------------------------------------
		msg = osMemoryPoolAlloc(memPool,osWaitForever);											
	
		msg[0] = gTokenInterface.myAddress << 3 + gTokenInterface.currentView; // Control 1
		msg[1] = destAddr << 3 + destSAPI;
		msg[2] = length;
		memcpy(&msg[3],testMessage,length);
		uint8_t crc = msg[0] + msg[1] + msg[2];
		for(uint8_t i = 0; i<length; i++)
		{
			crc = crc + msg[3 + i];
		}
		uint8_t status = (crc << 2) + (read << 1) + ack;
		msg[3 + length] = status;
		
		queueMsg.type = TO_PHY;
		queueMsg.anyPtr = msg;
		
		//--------------------------------------------------------------------------
		// QUEUE SEND	(send received frame to physical layer sender)
		//--------------------------------------------------------------------------
		retCode = osMessageQueuePut(
			queue_phyS_id,
			&queueMsg,
			osPriorityNormal,
			osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
	}
}
	

void MacSender(void *argument)
{
	 sendTestMessage();
}