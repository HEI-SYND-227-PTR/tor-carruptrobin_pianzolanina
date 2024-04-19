#include "main.h"
#include <stdio.h>
#include <string.h>



void sendTestMessage()
{
	struct queueMsg_t queueMsg;		// queue message
	osStatus_t retCode;
	
	//------------------------------------------------------------------------------
	for (;;)											// loop until doomsday
	{
		retCode = osMessageQueueGet( 	
			queue_chatR_id,
			&queueMsg,
			NULL,
			osWaitForever); 	
    CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);		
		
		switch(queueMsg.type)
		{
			case TOKEN:
			{
				
			}
				break;
			case DATABACK:
				{
					uint8_t* dataPtr = (uint8_t*)queueMsg.anyPtr;
				  bool read  = (dataPtr[3 + dataPtr[2]] & 2) >> 1;
					bool ack = (dataPtr[3 + dataPtr[2]] & 1);
					
					if(read == 1 && ack == 1)// Message has made a full circle and is okay
					{
							retCode = osMemoryPoolFree(memPool,dataPtr);
							CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
					else
					{
							read = 0;
							ack = 0;
							dataPtr[3 + dataPtr[2]] = dataPtr[3 + dataPtr[2]] & (ack + (read << 1));
							queueMsg.type = TO_PHY;
							queueMsg.anyPtr = dataPtr;
					}
				}
				break;
			case NEW_TOKEN:
				break;
			case START:
				break;
			case STOP:
				break;
			case DATA_IND:
			{
				uint8_t* msg = osMemoryPoolAlloc(memPool,osWaitForever);											
				msg[0] = gTokenInterface.myAddress << 3 + queueMsg.sapi; // Control 1
				msg[1] = queueMsg.addr << 3 + queueMsg.sapi;
				size_t length = strlen(queueMsg.anyPtr);
				msg[2] = length;
				memcpy(&msg[3],queueMsg.anyPtr,length);
				uint8_t crc = msg[0] + msg[1] + msg[2];
				for(uint8_t i = 0; i<length; i++){
					crc = crc + msg[3 + i];
				}
				bool read  = false;
				bool ack = false;
				uint8_t status = (crc << 2) + (read << 1) + ack;
				msg[3 + length] = status;
				
				queueMsg.type = TO_PHY;
				queueMsg.anyPtr = msg;
			}
				break;
		}
		


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