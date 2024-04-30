/**
 * The `MacReceiver` function processes incoming messages from a queue, performs various checks and
 * operations based on the message content, and forwards the messages to different queues accordingly.
 */

#include "main.h"
#include <stdio.h>
#include <string.h>

void MacReceiver(void *argument)
{
	struct queueMsg_t queueMsg; // queue message
	osStatus_t retCode;
	for (;;) // loop until doomsday
	{
		// Get the message in the MacR Queue
		// If empty, wait for a new message
		retCode = osMessageQueueGet(
			queue_macR_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		// Cast the pointer to a uint8
		uint8_t *dataPtr = (uint8_t *)queueMsg.anyPtr;

		if(queueMsg.type == FROM_PHY)
		{
			// If the message in the queue is a Token
			// -> Send to the MacSender as is
			if(dataPtr[0] == TOKEN_TAG)
			{
				queueMsg.type = TOKEN; 
				retCode = osMessageQueuePut(
					queue_macS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
			}
			//  Not a token but we are not connected
			// -> Send directly to the physical sender
			else if(gTokenInterface.connected == false)
			{
				queueMsg.type = TO_PHY;
				retCode = osMessageQueuePut(
						queue_phyS_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);	
			}
			else // Not a token
			{
				// Extract the source and destination addresses and SAPIs
				uint8_t srcAddr = (dataPtr[0] >> 3);
				uint8_t destAddr = (dataPtr[1] >> 3);
				uint8_t srcSapi = (dataPtr[0] & 0b111);
				uint8_t destSapi = (dataPtr[1] & 0b111);
				
				// If the SAPI of the destination is the same as our SAPI
				if(((1 << destSapi) == (gTokenInterface.station_list[gTokenInterface.myAddress] & (1 << CHAT_SAPI)))
					|| ((1 << destSapi) == (gTokenInterface.station_list[gTokenInterface.myAddress] & (1 << TIME_SAPI))))
				{
					// If the destination is us
					if(destAddr == gTokenInterface.myAddress || destAddr == 0xF)
					{
						bool read = true;
						bool ack = false;

						// If the destination is not broadcast
						if(destAddr != 0xF)
						{
							// CRC Compute
							uint8_t crc = dataPtr[0] + dataPtr[1] + dataPtr[2];
							for (uint8_t i = 0; i < dataPtr[2]; i++)
							{
								crc = crc + dataPtr[3 + i];
							}
							if((crc & 0b00111111) == (dataPtr[3 + dataPtr[2]] >> 2)) //CRC Check
							{
								ack = true;
							}
							dataPtr[3 + dataPtr[2]] = (dataPtr[3 + dataPtr[2]] & 0b11111100) + (ack + (read << 1));
						} 
						else // In broadcast, the ack is always true
						{
							ack = true;
						}

						// We allocate memory for the DataInd and copy the message
						uint8_t *msg = osMemoryPoolAlloc(memPool, osWaitForever);
						memcpy(msg,&dataPtr[3],dataPtr[2]);
						msg[dataPtr[2]] = '\0';

						// If the the destination is us but we are not the source
						// -> Send the message to the physical sender
						if(srcAddr != gTokenInterface.myAddress) 
						{
							queueMsg.type = TO_PHY;
							retCode = osMessageQueuePut(
								queue_phyS_id,
								&queueMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);	
						}
						// If bothe the destination and the source are us
						// -> Send the message to the MacSender as a Databack
						else if(srcAddr == gTokenInterface.myAddress)
						{
							queueMsg.type = DATABACK;
							queueMsg.addr = srcAddr;
							queueMsg.sapi = srcSapi;
							retCode = osMessageQueuePut(
								queue_macS_id,
								&queueMsg,
								osPriorityNormal,
								osWaitForever);
							CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						}
						
						// we prepare the DataInd message
						queueMsg.type = DATA_IND;
						queueMsg.anyPtr = msg;
						queueMsg.addr = srcAddr;
						queueMsg.sapi = srcSapi;
						if(ack == true)
						{
							// If the message has a correct CRC
							// -> We send Data_IND to the appropriate queue
							if(destSapi == CHAT_SAPI)
							{
								retCode = osMessageQueuePut(
									queue_chatR_id,
									&queueMsg,
									osPriorityNormal,
									osWaitForever);
								CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);	
							}
							else if (destSapi == TIME_SAPI)
							{
								retCode = osMessageQueuePut(
									queue_timeR_id,
									&queueMsg,
									osPriorityNormal,
									osWaitForever);
								CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);							
							}
						}
						else // If the CRC has a problem, free the memory
						{
							retCode = osMemoryPoolFree(memPool,queueMsg.anyPtr);
							CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
						}
					}
					// If i am the source BUT i am not the destination
					// -> Send a databack to the MacSender
					else if(srcAddr == gTokenInterface.myAddress)
					{
						queueMsg.type = DATABACK;
						queueMsg.addr = srcAddr;
						queueMsg.sapi = srcSapi;
						retCode = osMessageQueuePut(
							queue_macS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);					
					}
				}
				// If the destination SAPI is not the same as our SAPI
				// -> Send the message to the physical sender
				else
				{
						queueMsg.type = TO_PHY;
						retCode = osMessageQueuePut(
							queue_phyS_id,
							&queueMsg,
							osPriorityNormal,
							osWaitForever);
						CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);	
				}
			}
		}
	}
}
