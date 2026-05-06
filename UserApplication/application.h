/*
 * application.h
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#ifndef APPLICATION_USER_USERAPPLICATION_APPLICATION_H_
#define APPLICATION_USER_USERAPPLICATION_APPLICATION_H_

#include <stdint.h>

void UserApplicationInit(void);

void Send_Data_Done(void);

void RS485_ReadPZEM(void);

extern uint8_t pzem_payload[20];

#endif /* APPLICATION_USER_USERAPPLICATION_APPLICATION_H_ */
