/*
 * application.c
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#include "application.h"
#include "cellular.h"
#include "log_module.h"


void UserApplicationInit(void){
	 LOG_INFO_APP("UserApplication Init\n");
	CellularInit();

}
