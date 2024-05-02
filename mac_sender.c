#include "main.h"
#include <stdio.h>
#include <string.h>

//Attributes for MAC-Sender internal Buffer Queue
const osMessageQueueAttr_t queue_messBuff_attr = {
	.name = "MAC_INTERNAL_BUFFER"};

osMessageQueueId_t queue_messBuff_id;

void MacSender(void *argument)
{
	//Initialization for some needed variables
	struct queueMsg_t queueMsg; // queue message
	struct queueMsg_t tokenMsg; // token
	bool iHavetheToken = false;
	osStatus_t retCode;
	uint8_t dataBackErrorCounter = 0;

	//Creation of internal Buffer Queue
	queue_messBuff_id = osMessageQueueNew(4, sizeof(struct queueMsg_t), &queue_messBuff_attr);

	//------------------------------------------------------------------------------
	for (;;) // loop until doomsday
	{
		//Retrive next Input from queue (Waiting if nothing to retrive)
		retCode = osMessageQueueGet(
			queue_macS_id,
			&queueMsg,
			NULL,
			osWaitForever);
		CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		
		//Case of START received
		if(queueMsg.type == START)
		{
				gTokenInterface.connected = true; 	//Adjust intern Important else not receiving on MAC-Receiver side
				gTokenInterface.station_list[gTokenInterface.myAddress] = ((1 << CHAT_SAPI) + (1 << TIME_SAPI));	//Adjust station list with our station
		}
		//Case of STOP received
		else if(queueMsg.type == STOP)
		{
				gTokenInterface.connected = false;	//Adjust intern
				gTokenInterface.station_list[gTokenInterface.myAddress] = ((0 << CHAT_SAPI) + (1 << TIME_SAPI));	//Adjust station list, taking out our station
		}
		//Case NEW_TOKEN
		else if(queueMsg.type == NEW_TOKEN)
		{
				uint8_t *msg = osMemoryPoolAlloc(memPool, osWaitForever);	//Allocate memory from memory pool for the token
				msg[0] = TOKEN_TAG;	//Token start identifier
				for (uint8_t i = 0; i < 15; i++)
				{
					msg[1 + i] = gTokenInterface.station_list[i];			//put connected stations from our list into new token
				}
				msg[16] = 0;
				queueMsg.type = NEW_TOKEN;
				queueMsg.anyPtr = msg;			
				retCode = osMessageQueuePut(	//Send new token to Phy
					queue_phyS_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		}
		// Case DATABACK (when we send a message we wait for the databack and check it), we keep the token during this!
		else if (queueMsg.type == DATABACK && iHavetheToken == true) // Token still with us 
		{
			uint8_t *dataPtr = (uint8_t *)queueMsg.anyPtr;				//Give a datatype to the void pointer -> Access to different elements easier
			
			bool read = (dataPtr[3 + dataPtr[2]] & 2) >> 1; 			//Read out read bit
			bool ack = (dataPtr[3 + dataPtr[2]] & 1);					//Read out ack bit

			if (read == 1 && ack == 1) 		// Message has made a full circle and is okay
			{
				dataBackErrorCounter = 0;		//Counter because only limited resends
				retCode = osMemoryPoolFree(memPool, dataPtr);		//free memory space of message, when received and validated from destination address -> No memory overuse
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				
				//Preparation to send token to next station (To_Phy)
				queueMsg = tokenMsg;		//Put token back as message
				queueMsg.type = TO_PHY;		
				iHavetheToken = false;
			}
			else if (read == 0 && ack == 0) // Message has made a turn but no receiver on the ring
			{
				dataBackErrorCounter = 0;
				retCode = osMemoryPoolFree(memPool, dataPtr);			//free memory space of message else message would turn forever and ring is blocked
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				
				//send MAC Error message to lcd
				queueMsg.type = MAC_ERROR;
				char* errorMessage = "MAC Error : \nNo receiving Station";		//Text which should be displayed
				char* msg = osMemoryPoolAlloc(memPool, osWaitForever);			//Allocate memory space for this message
				strcpy(msg,errorMessage);										//String copy into previously new allocated memory
				queueMsg.addr = gTokenInterface.myAddress;						//Source address needed
				retCode = osMessageQueuePut(									//Send to lcd queue
						queue_lcd_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
				
				//Preparation to send token to next station (To_Phy)
				queueMsg = tokenMsg;
				queueMsg.type = TO_PHY;
				iHavetheToken = false;
			}
			else		//Message has made a turn, was received but with error -> try resending 20 times
			{
				dataBackErrorCounter++;
				if(dataBackErrorCounter == 20)		//When limit of resending tries reached -> Error message to lcd and delete message
				{
					retCode = osMemoryPoolFree(memPool,queueMsg.anyPtr);		//free memory space of message else message would turn forever and ring is blocked
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
					
					//send MAC Error message to lcd
					queueMsg.type = MAC_ERROR;
					char* errorMessage = "MAC Error : \nFailed after 20 tries";		//Text which should be displayed
					char* msg = osMemoryPoolAlloc(memPool, osWaitForever);			//Allocate memory space for this message
					strcpy(msg,errorMessage);										//String copy into previously new allocated memory
					queueMsg.addr = gTokenInterface.myAddress;						//Source address needed
					retCode = osMessageQueuePut(									//Send to lcd queue
						queue_lcd_id,
						&queueMsg,
						osPriorityNormal,
						osWaitForever);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
					
					//Preparation to send token to next station (To_Phy)
					queueMsg = tokenMsg;
					queueMsg.type = TO_PHY;
					iHavetheToken = false;
					dataBackErrorCounter = 0;
				}
				else		//if limit of tries not reached -> resend
				{
					queueMsg.type = TO_PHY;
					queueMsg.anyPtr = dataPtr;
				}
			}
			retCode = osMessageQueuePut(			//Send whatever was previously in the databack section prepared to send to PHY (Token, resending message, message to lcd)
				queue_phyS_id,
				&queueMsg,
				osPriorityNormal,
				osWaitForever);
			CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		}
		//Case TOKEN
		else if (queueMsg.type == TOKEN)
		{
			iHavetheToken = true;		//Variable just for us to see if we have token and databack to be sure
			tokenMsg = queueMsg;		//Store token 
			uint8_t *dataPtr = (uint8_t *)queueMsg.anyPtr;			//Give a datatype to the void pointer -> Access to different elements easier
			
			//Check wheter station list was updated
			bool stationHasChanged = false;							
			for(uint8_t i=0; i<15;i++)
			{
				if(gTokenInterface.station_list[i] != dataPtr[i + 1])		//check if differences
				{
					stationHasChanged = true;
				}
				if(i != gTokenInterface.myAddress)			//Adjust our station list if station is not us
				{
					gTokenInterface.station_list[i] = dataPtr[i+1];
				}
				else										//When our station put our correct info into the token
				{
					dataPtr[i+1] = gTokenInterface.station_list[i];
				}
			}
			if(stationHasChanged)		//When station list updated -> send to lcd
			{
				stationHasChanged = false;
				queueMsg.type = TOKEN_LIST;
				retCode = osMessageQueuePut(
					queue_lcd_id,
					&queueMsg,
					osPriorityNormal,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);	
			}

			//Next we look if there is something in the buffer to send
			if (osMessageQueueGetCount(queue_messBuff_id) != 0)
			{
				retCode = osMessageQueueGet(			//Get message from Mac send internal buffer
					queue_messBuff_id,
					&queueMsg,
					NULL,
					osWaitForever);
				CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

				//Case DATA_IND
				if(queueMsg.type == DATA_IND)		//only DATA_IND should be in MAC_S internal buffer
				{
					uint8_t *msg = osMemoryPoolAlloc(memPool, osWaitForever);			//Allocate memory space for this message
					msg[0] = (gTokenInterface.myAddress << 3) + queueMsg.sapi; 			//Put myAddress as source Addres and right sapi
					msg[1] = (queueMsg.addr << 3) + queueMsg.sapi;						//Put address received from queue as destination address and right sapi 
					size_t length = strlen(queueMsg.anyPtr);							//Length of Message also needs to be in data
					msg[2] = length;
					memcpy(&msg[3], queueMsg.anyPtr, length);							//Copy data from queue into new message
					
					//Calculate CRC (sum of all)
					uint8_t crc = msg[0] + msg[1] + msg[2];								//CRC for first 3 bytes
					for (uint8_t i = 0; i < length; i++)								//Calculate crc for rest of message
					{
						crc = crc + msg[3 + i];
					}

					bool read = false;
					bool ack = false;
					if(queueMsg.addr == 0xF)				//for broadcast messages read and ack allways 1
					{
						ack = true;
						read = true;
					}
					uint8_t status = (crc << 2) + (read << 1) + ack;					//Put CRC, read and ack at end of message
					msg[3 + length] = status;

					retCode = osMemoryPoolFree(memPool,queueMsg.anyPtr);				//Free memory space occupied by old message (to not use unneccesary memory space)
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);

					queueMsg.type = TO_PHY;				//Prepare to send to PHY
					queueMsg.anyPtr = msg;				//Put the queue pointer to the new created message
				}
			}
			else
			{
				queueMsg = tokenMsg;
			}

			//--------------------------------------------------------------------------
			// QUEUE SEND	(send received frame to physical layer sender)
			//--------------------------------------------------------------------------
			retCode = osMessageQueuePut(
				queue_phyS_id,
				&queueMsg,
				osPriorityNormal,
				osWaitForever);
			CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
		}
		else
		{
			retCode = osMessageQueuePut(
				queue_messBuff_id,
				&queueMsg,
				osPriorityNormal,
				0);
			CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
			if(retCode != osOK)
			{
					retCode = osMemoryPoolFree(memPool,queueMsg.anyPtr);
					CheckRetCode(retCode, __LINE__, __FILE__, CONTINUE);
			}
		}
	}
}
