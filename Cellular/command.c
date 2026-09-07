/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    command.c
 * @author  KarthiG
 * @brief   Main command driver GSM/LTE Modem
 ******************************************************************************
 * @attention
 *
 * Copyright (c) ADARKO.
 * All rights reserved.
 *
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "command.h"
#include "utilities_conf.h"
#include "stm32_tiny_sscanf.h"
#include "stm32_adv_trace.h"
#include "stm32_timer.h"
#include "log_module.h"
#include "usart.h"
#include "application.h"
#include "stm32_systime.h"

/* Private typedef -----------------------------------------------------------*/
/**
 * @brief  Structure defining an AT Response
 */
struct ATResponse_s {
	const char *string; /*< Response string  from modem */
	const int size_string; /*< size of the Response string, not including the final \0 */
	void (*set)(const char *param);
	void (*run)(const char *param);
};

typedef struct {
	uint8_t is_at_ready;
	uint8_t is_sim_ready;
	uint8_t is_sms_ready;
} Modem_Status_s;

Modem_Status_s modem_status = { 0, 0, 0 };

typedef struct {
	UTIL_TIMER_Object_t cellular_command_timer_Id;
	UTIL_TIMER_Object_t cellular_response_timer_Id;
} CellularContext_t;

static CellularContext_t cellularContext;

uint8_t charRxhlp1;
uint8_t current_command = 0;
uint8_t execute_next_command = 1;
int creg_n = 0;
int creg_stat = -1;
int cgreg_n = 0;
int cgreg_stat = -1;
int network_opened = 0;
int tcp_udp_network_opened = 0;
int cip_close = -1;
static uint8_t ignore_next_ok = 0;
static uint8_t dns_result_pending = 0;
static uint8_t dns_ok_seen = 0;
static uint8_t ignore_dns_error = 0;
static uint8_t cntp_ok_seen = 0;
static uint8_t netopen_ok_seen = 0;
static uint8_t data_cycle_finished = 0;
uint32_t timer_period_modem_init_ms = 60000;
uint32_t timer_period_modem_cmd_ms = 200;
uint32_t timer_period_modem_response_ms = 60000;

#define CMD_SIZE                        540
#define CIRC_BUFF_SIZE                  256
///* Character added when a RX error has been detected */
#define AT_ERROR_RX_CHAR 0x01

static void (*RxCpltCallbackhlp1)(uint8_t *rxChar, uint16_t size, uint8_t error);

#define MODEM_AT_READY      "*ATREADY: 1"
#define MODEM_CPIN_READY    "+CPIN: READY"
#define MODEM_CPIN_SIM_REMOVED    "+CPIN: SIM REMOVED"
#define MODEM_SMS_DONE      "SMS DONE"
#define MODEM_CGEV		    "+CGEV"
#define MODEM_CSQ 		    "+CSQ"
#define MODEM_CREG 			"+CREG"
#define MODEM_CGREG 		"+CGREG"
#define MODEM_OK 			"OK"
#define MODEM_ERROR 		"ERROR"
#define MODEM_NETOPEN 		"+NETOPEN"
#define MODEM_CDNSGIP 		"+CDNSGIP"
#define MODEM_CIPOPEN 		"+CIPOPEN"
#define MODEM_CIPSEND 		"+CIPSEND"
#define MODEM_CNTP_GET 		"+CNTP"
#define MODEM_CIPRXGET 	    "+CIPRXGET"
#define MODEM_CCLK 		    "+CCLK"
#define MODEM_CIPSEND_READY ">"
#define MODEM_IPCLOSE 		"+IPCLOSE"
#define MODEM_CIPCLOSE 		"+CIPCLOSE"

#define MODEM_USER_DL_HEADER "901A"

void modem_at_ready(const char *param);
void modem_sim_ready(const char *param);
void modem_sim_removed(const char *param);
void modem_sms_ready(const char *param);
void modem_cgev(const char *param);
void modem_ok_resp(const char *param);
void modem_error_resp(const char *param);

void modem_csq(const char *param);
void modem_creg(const char *param);
void modem_cgreg(const char *param);
void modem_netopen(const char *param);
void modem_cdns_gip(const char *param);
void modem_cipopen(const char *param);
void modem_cipsend(const char *param);
void modem_cntp(const char *param);
void modem_cclk(const char *param);
void modem_cipsend_ready(const char *param);
void modem_user_dl(const char *param);
void modem_ipclose(const char *param);
void modem_cipclose(const char *param);
void modem_ciprxget(const char *param);

void do_nothing(const char *param);

void GSM_Uart_Transmit(uint8_t *p_data, uint16_t size);
static void modem_response_timeout(void *arg);
static void modem_continue_without_ntp(void);

typedef enum AT_COMMANDS_SEQUENCE_e {
	AT_E0 = 0,       // disable echo
	AT_CREG, AT_CGREG, // =1,2
	AT_CPSI, //=2
	AT_CSQ, // =3,
	AT_CREG_QUERY, // = 4,
	AT_CGREG_QUERY, // = 5,
	AT_CGDCONT, // = 6,
	AT_CNTP_SET, // = 7,
	AT_CNTP_GET, // = 8,
	AT_CCLK, // = 9,
	AT_NETOPEN, // = 10,
	AT_CDNSCFG, // = 11,
	AT_CDNSGIP, // = 12,
	AT_CIPRXGET_SET, // = 13,
	AT_CIPOPEN, // = 14,
	AT_CIPSEND, // = 15,
	AT_SEND, // = 16, //> send data
	AT_CIPRXGET_READ, // = 17, //AT+CIPRXGET=3,1,12
	AT_CIPCLOSE, // = 18,
	AT_MAX_SEQ,

} ATSequence_t;

/**
 * @brief  Array of all supported AT Commands
 */
static const struct ATResponse_s ATResponse[] = {
//
		{ .string = MODEM_AT_READY, .size_string = sizeof(MODEM_AT_READY) - 1,
				.set = do_nothing, .run = modem_at_ready },
		//
		{ .string = MODEM_CPIN_READY, .size_string = sizeof(MODEM_CPIN_READY)
				- 1, .set = do_nothing, .run = modem_sim_ready },
		//
		{ .string = MODEM_CPIN_SIM_REMOVED, .size_string = sizeof(MODEM_CPIN_SIM_REMOVED)
				- 1, .set = do_nothing, .run = modem_sim_removed },
		//
		{ .string = MODEM_SMS_DONE, .size_string = sizeof(MODEM_SMS_DONE) - 1,
				.set = do_nothing, .run = modem_sms_ready },
		//
		{ .string = MODEM_CGEV, .size_string = sizeof(MODEM_CGEV) - 1, .set =
				modem_cgev, .run = do_nothing },
		//
		{ .string = MODEM_CSQ, .size_string = sizeof(MODEM_CSQ) - 1, .set =
				modem_csq, .run = do_nothing },
		//
		{ .string = MODEM_OK, .size_string = sizeof(MODEM_OK) - 1, .set =
				do_nothing, .run = modem_ok_resp },
		//
		{ .string = MODEM_ERROR, .size_string = sizeof(MODEM_ERROR) - 1, .set =
				do_nothing, .run = modem_error_resp },
		//
		{ .string = MODEM_CREG, .size_string = sizeof(MODEM_CREG) - 1, .set =
				modem_creg, .run = do_nothing },
		//
		{ .string = MODEM_CGREG, .size_string = sizeof(MODEM_CGREG) - 1, .set =
				modem_cgreg, .run = do_nothing },
		//
		{ .string = MODEM_NETOPEN, .size_string = sizeof(MODEM_NETOPEN) - 1,
				.set = modem_netopen, .run = do_nothing },
		//
		{ .string = MODEM_CDNSGIP, .size_string = sizeof(MODEM_CDNSGIP) - 1,
				.set = modem_cdns_gip, .run = do_nothing },
		//
		{ .string = MODEM_CIPOPEN, .size_string = sizeof(MODEM_CIPOPEN) - 1,
				.set = modem_cipopen, .run = do_nothing },
		//
		{ .string = MODEM_CIPSEND_READY, .size_string =
				sizeof(MODEM_CIPSEND_READY) - 1, .set = do_nothing, .run =
				modem_cipsend_ready },
		//
		{ .string = MODEM_CIPSEND, .size_string = sizeof(MODEM_CIPSEND) - 1,
				.set = modem_cipsend, .run = do_nothing },
		//
		{ .string = MODEM_CNTP_GET, .size_string = sizeof(MODEM_CNTP_GET) - 1,
				.set = modem_cntp, .run = do_nothing },
		//
		{ .string = MODEM_CCLK, .size_string = sizeof(MODEM_CCLK) - 1, .set =
				modem_cclk, .run = do_nothing },
		//
		{ .string = MODEM_USER_DL_HEADER, .size_string =
				sizeof(MODEM_USER_DL_HEADER) - 1, .set = do_nothing, .run =
				modem_user_dl },
		//
		{ .string = MODEM_IPCLOSE, .size_string = sizeof(MODEM_IPCLOSE) - 1,
				.set = modem_ipclose, .run = do_nothing },
		//
		{ .string = MODEM_CIPRXGET, .size_string = sizeof(MODEM_CIPRXGET) - 1,
				.set = modem_ciprxget, .run = do_nothing },
//
		};

static char circBuffer[CIRC_BUFF_SIZE];
static char command[CMD_SIZE];
static unsigned i = 0;
static uint32_t widx = 0;
static uint32_t ridx = 0;
static uint32_t charCount = 0;
static uint32_t circBuffOverflow = 0;

/**
 *
 * @brief  Parse a command and process it
 * @param  The command
 * @retval None
 */
static void parse_cmd(const char *cmd);

/**
 * @brief  Print a string corresponding to an ATEerror_t
 * @param  The AT error code
 * @retval None
 */
static void com_error(ATEerror_t error_type);

static void CMD_GetChar(uint8_t *rxChar, uint16_t size, uint8_t error);

/**
 * @brief  CNotifies the upper layer that a charchter has been receveid
 * @param  None
 * @retval None
 */
static void (*NotifyCb)(void);

/**
 * @brief  Remove backspace and its preceding character in the Command string
 * @param  Command string to process
 * @retval 0 when OK, otherwise error
 */
static int32_t CMD_ProcessBackSpace(char *cmd);

void check_modem_status(void) {
	if (modem_status.is_sim_ready && modem_status.is_sms_ready
			&& modem_status.is_at_ready) {
		LOG_INFO_APP("All ready\r\n");

		current_command = AT_CREG;
		UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id,
				timer_period_modem_init_ms);

	}
}

void modem_at_ready(const char *param) {

	modem_status.is_at_ready = 1;
	LOG_INFO_APP("Modem is ready to receive at command\r\n");
	check_modem_status();
}

void modem_sim_ready(const char *param) {

	modem_status.is_sim_ready = 1;
	LOG_INFO_APP("Modem is sim ready\r\n");
	check_modem_status();
}

void modem_sim_removed(const char *param) {

	modem_status.is_sim_ready = 1;
	LOG_INFO_APP("Modem has No SIM\r\n");
	Send_Data_Done();

}

void modem_sms_ready(const char *param) {

	modem_status.is_sms_ready = 1;
	LOG_INFO_APP("Modem is ready for sms\r\n");
	check_modem_status();
}

void modem_cgev(const char *param) {
//	LOG_INFO_APP("Modem CGEV event:%s\r\n", param);
//	+CGEV: NW PDN ACT 1
//	+CGEV: NW PDN DEACT 1
//	+CGEV: ME PDN DEACT 1
//	+CGEV: ME PDN DEACT 1
//	+CGEV: EPS PDN ACT 1
//	+CGEV: NW REATTACH
	if (strstr(param, "PDN ACT")) {
		network_opened = 1;
		LOG_INFO_APP("Modem network opened event\r\n");
	} else if (strstr(param, "PDN DEACT")) {
		network_opened = 0;
		LOG_INFO_APP("Modem network closed event\r\n");
	} else if (strstr(param, "REATTACH")) {
		LOG_INFO_APP("Modem reattach event\r\n");
	}

}

void modem_creg(const char *param) {

	if (2 == tiny_sscanf(param, " %d,%d", // expect format: +CREG: <n>,<stat>
			&creg_n, &creg_stat)) {
		LOG_INFO_APP("Modem CREG n:%d, stat:%d\r\n", creg_n, creg_stat);
		if (creg_stat == 1) {
			execute_next_command = 1;
			LOG_INFO_APP("Modem registered to home network\r\n");
		} else if (creg_stat == 5) {
			execute_next_command = 1;
			LOG_INFO_APP("Modem registered, roaming\r\n");
		} else {
			LOG_INFO_APP("Modem CREG registration failed; ending cellular cycle\r\n");
			Send_Data_Done();
		}
	} else {
		LOG_INFO_APP("Modem CREG parse error\r\n");
	}
}

void modem_cgreg(const char *param) {

	if (2 == tiny_sscanf(param, " %d,%d", // expect format: +CGREG: <n>,<stat>
			&cgreg_n, &cgreg_stat)) {
		LOG_INFO_APP("Modem CGREG n:%d, stat:%d\r\n", cgreg_n, cgreg_stat);
		if (cgreg_stat == 1) {
			execute_next_command = 1;
			LOG_INFO_APP("Modem registered to home network\r\n");
		} else if (cgreg_stat == 5) {
			execute_next_command = 1;
			LOG_INFO_APP("Modem registered, roaming\r\n");
		} else {
			LOG_INFO_APP("Modem CGREG registration failed; ending cellular cycle\r\n");
			Send_Data_Done();
		}
	} else {
		LOG_INFO_APP("Modem CGREG parse error\r\n");
	}
}

void modem_csq(const char *param) {
	int rssi = 0;
	int ber = 0;

	if (2 == tiny_sscanf(param, " %d,%d", // expect format: +CSQ: <rssi>,<ber>
			&rssi, &ber)) {
		LOG_INFO_APP("Modem RSSI :%d, BER :%d\r\n", rssi, ber);
	} else {
		LOG_INFO_APP("Modem CSQ parse error\r\n");
	}

}

void modem_netopen(const char *param) {

	int status = 1;
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	if (1 == tiny_sscanf(param, " %d", // expect format: 0
			&status)) {
		if (status == 0) {
			LOG_INFO_APP("Modem network opened\r\n");
			/* PDP must be open before CNTP can reach the NTP server. */
			current_command = AT_CNTP_SET;
			ignore_next_ok = netopen_ok_seen == 0U;
			netopen_ok_seen = 0;
			UTIL_TIMER_StartWithPeriod(
					&cellularContext.cellular_command_timer_Id,
					timer_period_modem_cmd_ms);
		} else {
			LOG_INFO_APP("Modem network failed\r\n");
			Send_Data_Done();
		}
	} else {
		LOG_INFO_APP("Modem NETOPEN parse error\r\n");
	}
}

void modem_cdns_gip(const char *param) {
	int result = -1;
	int error_code = -1;

	/* SIMCom responses are typically +CDNSGIP: 1,"host","ip" or : 0. */
	if (1 == tiny_sscanf(param, " %d", &result) && result == 1) {
		UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
		LOG_INFO_APP("Modem DNS resolution succeeded\r\n");
		dns_result_pending = 0;
		ignore_dns_error = 0;
		current_command = AT_CIPRXGET_SET;
		ignore_next_ok = dns_ok_seen == 0U;
		dns_ok_seen = 0;
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_command_timer_Id,
				timer_period_modem_cmd_ms);
	} else if (2 == tiny_sscanf(param, " %d,%d", &result, &error_code)
			&& result == 0) {
		UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
		LOG_INFO_APP("Modem DNS failed with status %d, error %d\r\n",
				result, error_code);
		dns_result_pending = 0;
		/* The modem reports +CDNSGIP failure followed by a transaction ERROR. */
		ignore_dns_error = 1;
		if (data_cycle_finished == 0U) {
			LOG_INFO_APP("Modem DNS unavailable; trying configured server %s\r\n",
					app_config.config.server_addr);
			current_command = AT_CIPRXGET_SET;
			UTIL_TIMER_StartWithPeriod(
					&cellularContext.cellular_command_timer_Id,
					timer_period_modem_cmd_ms);
		}
	} else {
		UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
		LOG_INFO_APP("Modem DNS parse error\r\n");
		if (data_cycle_finished == 0U) {
			data_cycle_finished = 1;
			Send_Data_Done();
		}
	}
}

void modem_cipopen(const char *param) {
	//+CIPOPEN: 1,0 - 1 means connection 1 is opened successfully, 0 means success
	int conn_id = -1;
	int status = -1;
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	if (2 == tiny_sscanf(param, " %d,%d", // expect format: +CIPOPEN: <conn_id>,<status>
			&conn_id, &status)) {
		if (status == 0) {
			LOG_INFO_APP("Modem CIPOPEN connection %d opened successfully\r\n",
					conn_id);
			current_command = AT_CIPSEND;
			UTIL_TIMER_StartWithPeriod(
					&cellularContext.cellular_command_timer_Id,
					timer_period_modem_cmd_ms);
		} else {
			LOG_INFO_APP(
					"Modem CIPOPEN connection %d failed with status %d\r\n",
					conn_id, status);
			Send_Data_Done();
		}
	} else {
		LOG_INFO_APP("Modem CIPOPEN parse error\r\n");
	}
}

void modem_cipsend(const char *param) {
	//+CIPSEND: <link_num>,<reqSendLength>,<cnfSendLength> // if reqSendLength and cnfSendLength are same, means data sent successfully
	int conn_id = -1;
	int reqSendLength = -1;
	int cnfSendLength = -1;
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	if (3 == tiny_sscanf(param, " %d,%d,%d", // expect format: +CIPSEND: <link_num>,<reqSendLength>,<cnfSendLength>
			&conn_id, &reqSendLength, &cnfSendLength)) {
		if (reqSendLength == cnfSendLength) {
			LOG_INFO_APP(
					"Modem CIPSEND connection %d sent data successfully, length %d\r\n",
					conn_id, reqSendLength);
		} else {
			LOG_INFO_APP(
					"Modem CIPSEND connection %d failed to send data, req length %d, cnf length %d\r\n",
					conn_id, reqSendLength, cnfSendLength);
		}
		current_command = AT_CIPCLOSE;
		UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id,
				timer_period_modem_cmd_ms);
	} else {
		LOG_INFO_APP("Modem CIPSEND parse error\r\n");
	}
}

void modem_user_dl(const char *param) {
	LOG_INFO_APP("Modem user data received\r\n");
	//901A88BFF068180090400100
	LOG_INFO_APP("Modem user data :%s\r\n", param);
	current_command = AT_CIPCLOSE;
	UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id, 500);
}

void modem_ciprxget(const char *param) {
	//+CIPRXGET: 1,1 +CIPRXGET: <link_num>,<dataavailabe>, 0- data not availabe, 1 - data available
	//+CIPRXGET: 3,1,12,0  +CIPRXGET: <mode>,<link_num>,<read_len>,<rest_len> // mode 3 hexdata, link_num, read_len - number of bytes read, rest_len - number of bytes remaining in the buffer
	int mode = -1;
	int data_available = -1;
	int conn_id = -1;
	int read_len = -1;
	int rest_len = -1;
	if (4 == tiny_sscanf(param, " %d,%d,%d,%d", // expect format: +CIPRXGET: <mode>,<link_num>,<read_len>,<rest_len>
			&mode, &conn_id, &read_len, &rest_len)) {
		if (mode == 3) {
			LOG_INFO_APP(
					"Modem CIPRXGET connection %d read length %d, rest length %d\r\n",
					conn_id, read_len, rest_len);
			if (rest_len == 0) {

			}

		} else {
			LOG_INFO_APP("Modem CIPRXGET unsupported mode %d\r\n", mode);
		}
	} else {
		if (2 == tiny_sscanf(param, " %d,%d", // expect format: +CIPRXGET: <link_num>,<dataavailabe>
				&conn_id, &data_available)) {
			if (data_available == 1) {
				LOG_INFO_APP("Modem CIPRXGET connection %d data available\r\n",
						conn_id);
				current_command = AT_CIPRXGET_READ;
				UTIL_TIMER_StartWithPeriod(
						&cellularContext.cellular_command_timer_Id, 500);
			}
		} else {
			LOG_INFO_APP("Modem CIPRXGET parse error\r\n");
		}
	}

}

void modem_ok_resp(const char *param) {

	//LOG_INFO_APP("Current Command %d\r\n", current_command);
	if (ignore_next_ok != 0U) {
		ignore_next_ok = 0;
		return;
	}

	/* CDNSGIP completes asynchronously; its OK is not the DNS result. */
	if (current_command == AT_CDNSGIP && dns_result_pending != 0U) {
		dns_ok_seen = 1;
		return;
	}

	//move to next command only if not in the middle of network open, cipopen, cipsend, cntp get, send data, ciprxget read
	if (current_command == AT_CGDCONT) {
		/* AT+CGDCONT only defines the PDP context; open it before CNTP. */
		current_command = AT_NETOPEN;
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_command_timer_Id,
				timer_period_modem_cmd_ms);
	} else if (current_command == AT_CCLK) {
		current_command = AT_CDNSCFG;
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_command_timer_Id,
				timer_period_modem_cmd_ms);
	} else if (current_command != AT_NETOPEN && current_command != AT_CIPOPEN
			&& current_command != AT_CIPSEND && current_command != AT_CNTP_GET
			&& current_command != AT_SEND && current_command != AT_CIPRXGET_READ) {

//		if (creg_stat == 1 || creg_stat == 5 || creg_stat == -1) {
		current_command++;
		//LOG_INFO_APP("Current Command %d execute\r\n", current_command);
		if (current_command < AT_MAX_SEQ) { //AT_MAX is not a command, just to indicate the end of commands
			UTIL_TIMER_StartWithPeriod(
					&cellularContext.cellular_command_timer_Id,
					timer_period_modem_cmd_ms);
		} else {
//				current_command = 0;
//				UTIL_TIMER_StartWithPeriod(
//						&cellularContext.cellular_command_timer_Id,
//						timer_period_modem_cmd_ms);

			Send_Data_Done();
		}
//		} else {
//			UTIL_TIMER_StartWithPeriod(
//					&cellularContext.cellular_command_timer_Id,
//					timer_period_modem_cmd_ms);
//		}
	} else if (current_command == AT_NETOPEN) {
		netopen_ok_seen = 1;
	} else if (current_command == AT_CNTP_GET) {
		cntp_ok_seen = 1;
	}
}
void modem_error_resp(const char *param) {
//	UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id,
//			timer_period_modem_cmd_ms);
	if (data_cycle_finished != 0U) {
		return;
	} else if (ignore_dns_error != 0U) {
		ignore_dns_error = 0;
		LOG_INFO_APP("Ignoring trailing DNS transaction ERROR\r\n");
		return;
	} else if (current_command == AT_CNTP_SET || current_command == AT_CNTP_GET) {
		LOG_INFO_APP("Modem CNTP command rejected; continuing without time sync\r\n");
		modem_continue_without_ntp();
	} else {
		Send_Data_Done();
	}
}

void modem_cipsend_ready(const char *param) {
	LOG_INFO_APP("Modem ready to accept data\r\n");
	//send data

	current_command = AT_SEND;
	UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id,
			timer_period_modem_cmd_ms);
}

void modem_cntp(const char *param) {
	int datetime = -1;
	//+CNTP: 0
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	if (1 == tiny_sscanf(param, " %d", // expect format: +CNTP: <status>
			&datetime)) {
		if (datetime == 0) {
			LOG_INFO_APP("Modem CNTP get time success\r\n");
			current_command = AT_CCLK;
			ignore_next_ok = cntp_ok_seen == 0U;
			cntp_ok_seen = 0;
			UTIL_TIMER_StartWithPeriod(
					&cellularContext.cellular_command_timer_Id,
					timer_period_modem_cmd_ms);
		} else {
			LOG_INFO_APP("Modem CNTP get time failed\r\n");
			modem_continue_without_ntp();
		}
	} else {
		LOG_INFO_APP("Modem CNTP parse error\r\n");
		modem_continue_without_ntp();
	}
}

static void modem_continue_without_ntp(void) {
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	cntp_ok_seen = 0;
	ignore_next_ok = 0;
	current_command = AT_CCLK;
	LOG_INFO_APP("Continuing cellular upload without NTP synchronization\r\n");
	UTIL_TIMER_StartWithPeriod(&cellularContext.cellular_command_timer_Id,
			timer_period_modem_cmd_ms);
}

void modem_cclk(const char *param) {
	//char datetime[30] = { 0 };
	LOG_INFO_APP("Modem CCLK date time : %s\r\n", param);
	//+CCLK: "yy/MM/dd,hh:mm:ss+zz"
	//+CCLK: "25/10/16,17:30:41+00"

	int year, month, day, hour, min, sec, tz;
	if (7
			== tiny_sscanf(param, " \"%d/%d/%d,%d:%d:%d%+%d\"", &year, &month,
					&day, &hour, &min, &sec, &tz)) {
		LOG_INFO_APP("Modem CCLK parse date time :%d-%d-%d %d:%d:%d tz:%d\r\n",
				year, month, day, hour, min, sec, tz);
		if ((year == 70 && month == 1 && day == 1)
				|| month < 1 || month > 12 || day < 1 || day > 31
				|| hour < 0 || hour > 23 || min < 0 || min > 59
				|| sec < 0 || sec > 59) {
			LOG_INFO_APP("Modem CCLK contains invalid/unset time; keeping current time\r\n");
			return;
		}
		/* Manual Unix epoch — avoids broken mktime on this libc.
		 * yy is 2-digit year relative to 2000 (e.g. 26 = 2026). */
		static const uint16_t yday[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
		int y = 2000 + year - 1970;
		uint32_t days = (uint32_t)y * 365u + (uint32_t)((y + 1) / 4);
		days += yday[month - 1];
		if (month > 2 && ((2000 + year) % 4 == 0)) days++;
		days += (uint32_t)(day - 1);
		uint32_t epoch_time = days * 86400u
		                    + (uint32_t)hour * 3600u
		                    + (uint32_t)min  * 60u
		                    + (uint32_t)sec;
		epoch_time -= (uint32_t)tz * 900u; /* quarter-hour TZ offset to UTC */

		LOG_INFO_APP("Modem CCLK epoch time :%lu\r\n", (unsigned long)epoch_time);
		SysTime_t sysTime = { .Seconds = epoch_time, .SubSeconds = 0 };
		SysTimeSet(sysTime);
		LOG_INFO_APP("SysTime set to %lu\r\n", (unsigned long)epoch_time);
	} else {
		LOG_INFO_APP("Modem CCLK parse date time error\r\n");
	}

}

void modem_ipclose(const char *param) {
	//+IPCLOSE: 1,1 +IPCLOSE: <client_index>,<close_reason>  // close_reason 0 - Closed by local, active 1 - Closed by remote, passive 2 - Closed for sending timeout
	int conn_id = -1;
	int close_reason = -1;
	if (2 == tiny_sscanf(param, " %d,%d", // expect format: +IPCLOSE: <client_index>,<close_reason>
			&conn_id, &close_reason)) {
		LOG_INFO_APP("Modem IPCLOSE connection %d closed, reason %d\r\n",
				conn_id, close_reason);
		cip_close = close_reason;
	} else {
		LOG_INFO_APP("Modem IPCLOSE parse error\r\n");
	}
}

void do_nothing(const char *param) {
}

void GSM_Uart_Init(
		void (*RxCbhlp1)(uint8_t *rxChar, uint16_t size, uint8_t error)) {

//	UART_WakeUpTypeDef WakeUpSelection;

//	  /*record call back*/
	RxCpltCallbackhlp1 = RxCbhlp1;

//	/*Set wakeUp event on start bit*/
//	WakeUpSelection.WakeUpEvent = UART_WAKEUP_ON_READDATA_NONEMPTY;
//
//	HAL_UARTEx_StopModeWakeUpSourceConfig(&huart2, WakeUpSelection);
//
//	/* Make sure that no UART transfer is on-going */
//	while (__HAL_UART_GET_FLAG(&huart2, USART_ISR_BUSY) == SET)
//		;
//
//	/* Make sure that UART is ready to receive)   */
//	while (__HAL_UART_GET_FLAG(&huart2, USART_ISR_REACK) == RESET)
//		;
//
//	/* Enable USART interrupt */
//	__HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
//
//	/*Enable wakeup from stop mode*/
//	HAL_UARTEx_EnableStopMode(&huart2);

	HAL_UART_Receive_IT(&huart2, &charRxhlp1, 1);
}

void GSM_Uart_Transmit(uint8_t *p_data, uint16_t size) {

	HAL_UART_Transmit(&huart2, p_data, size, 1000);

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == USART2) {

		//APP_LOG(TS_OFF, VLEVEL_M, "ErrCD %d\r\n", huart->ErrorCode);

		if ((NULL != RxCpltCallbackhlp1)
				&& (HAL_UART_ERROR_NONE == huart->ErrorCode)) {
			RxCpltCallbackhlp1(&charRxhlp1, 1, 0);
		}
		HAL_UART_Receive_IT(&huart2, &charRxhlp1, 1);
	}

	/* USER CODE END HAL_UART_RxCpltCallback_2 */
}

int tsnprintf(char *buf, int buf_size, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = tiny_vsnprintf_like(buf, buf_size, fmt, args);
	va_end(args);
	return ret;
}

static void Send_Cellular_Command_Req(void *arg) {
	LOG_INFO_APP("Send_Cellular_Command_Req %d \n", current_command);
	UTIL_TIMER_Stop(&cellularContext.cellular_command_timer_Id);
//	command_to_modem(current_command);

	switch (current_command) {
	case AT_E0: //ATE0 — disable echo so binary payload isn't parsed as AT response
		GSM_Uart_Transmit((uint8_t *)"ATE0\r\n", 6);
		break;
	case AT_CREG: //AT+CREG=1
		const char *cmd = "AT+CREG=1\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd, strlen(cmd));
		break;
	case AT_CGREG: //AT+CGREG=1
		const char *cmd1 = "AT+CGREG=1\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd1, strlen(cmd1));
		break;
	case AT_CPSI: //AT+CPSI?
		const char *cmd_cpsi = "AT+CPSI?\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd_cpsi, strlen(cmd_cpsi));
		break;
	case AT_CSQ: //AT+CSQ
		const char *cmd2 = "AT+CSQ\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd2, strlen(cmd2));
		break;
	case AT_CREG_QUERY: //AT+CREG?
		const char *cmd3 = "AT+CREG?\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd3, strlen(cmd3));
		break;
	case AT_CGREG_QUERY: //AT+CGREG?
		const char *cmd4 = "AT+CGREG?\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd4, strlen(cmd4));
		break;
	case AT_CCLK: //AT+CCLK?
		const char *cmd_cclk = "AT+CCLK?\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd_cclk, strlen(cmd_cclk));
		break;
	case AT_CGDCONT: //AT+CGDCONT=1,"IP","<apn>"
		char ATCGDCONT[96];
		if (tsnprintf(ATCGDCONT, sizeof(ATCGDCONT),
				"AT+CGDCONT=1,\"ip\",\"%s\"\r\n", app_config.config.apn)
				>= (int)sizeof(ATCGDCONT)) {
			LOG_INFO_APP("APN command is too long\r\n");
			Send_Data_Done();
			break;
		}
		GSM_Uart_Transmit((uint8_t*) ATCGDCONT, strlen(ATCGDCONT));
		break;
	case AT_CNTP_SET: //AT+CNTP="time.nist.gov",123
		const char *cmd_cntp_set = "AT+CNTP=\"3.pool.ntp.org\",0\r\n"; //3.pool.ntp.org //time.nist.gov
		GSM_Uart_Transmit((uint8_t*) cmd_cntp_set, strlen(cmd_cntp_set));
		break;
	case AT_CNTP_GET: //AT+CNTP
		const char *cmd_cntp_get = "AT+CNTP\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd_cntp_get, strlen(cmd_cntp_get));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_NETOPEN: //AT+NETOPEN
		const char *cmd6 = "AT+NETOPEN\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd6, strlen(cmd6));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_CDNSCFG: //AT+CDNSCFG="
		const char *cmd7 = "AT+CDNSCFG=\"8.8.8.8\",\"1.1.1.1\"\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd7, strlen(cmd7));
		break;
	case AT_CDNSGIP: //AT+CDNSGIP="
		const char *cmd8 = "AT+CDNSGIP=\"%s\"\r\n";
		char ATCDNSGIP[96];
		if (tsnprintf(ATCDNSGIP, sizeof(ATCDNSGIP), cmd8,
				app_config.config.server_addr) >= (int)sizeof(ATCDNSGIP)) {
			LOG_INFO_APP("DNS hostname command is too long\r\n");
			Send_Data_Done();
			break;
		}
		dns_result_pending = 1;
		GSM_Uart_Transmit((uint8_t*) ATCDNSGIP, strlen(ATCDNSGIP));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_CIPRXGET_SET: //AT+CIPRXGET=1
		const char *cmd9 = "AT+CIPRXGET=1\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd9, strlen(cmd9));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_CIPOPEN: //AT+CIPOPEN=1,"TCP","<server>",<port>
		char ATCIPOPEN[128];
		if (tsnprintf(ATCIPOPEN, sizeof(ATCIPOPEN),
				"AT+CIPOPEN=1,\"TCP\",\"%s\",%d\r\n",
				app_config.config.server_addr,
				app_config.config.server_port) >= (int)sizeof(ATCIPOPEN)) {
			LOG_INFO_APP("TCP hostname command is too long\r\n");
			Send_Data_Done();
			break;
		}
		GSM_Uart_Transmit((uint8_t*) ATCIPOPEN, strlen(ATCIPOPEN));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_CIPSEND: //AT+CIPSEND=1,size
		const char *cmd11 = "AT+CIPSEND=1,%d\r\n";
		char ATCIPSEND[50];
		tsnprintf(ATCIPSEND, sizeof(ATCIPSEND), cmd11, payload.BufferSize);
		GSM_Uart_Transmit((uint8_t*) ATCIPSEND, strlen(ATCIPSEND));
		UTIL_TIMER_StartWithPeriod(
				&cellularContext.cellular_response_timer_Id,
				timer_period_modem_response_ms);
		break;
	case AT_SEND: //send data
	{
		LOG_INFO_APP("TX payload (%d bytes):\r\n", payload.BufferSize);
		for (uint8_t _i = 0; _i < payload.BufferSize; _i++)
			LOG_INFO_APP(" %02X", payload.Buffer[_i]);
		LOG_INFO_APP("\r\n");
		GSM_Uart_Transmit(payload.Buffer, payload.BufferSize);
		break;
	}
	case AT_CIPRXGET_READ: //AT+CIPRXGET=3,1,12
		const char *cmd12 = "AT+CIPRXGET=3,1,12\r\n";
		GSM_Uart_Transmit((uint8_t*) cmd12, strlen(cmd12));
		break;
	case AT_CIPCLOSE: //AT+CIPCLOSE=1
		if (cip_close > 0) {
			LOG_INFO_APP(
					"Remote closed the connection, no need to send CIPCLOSE\r\n");
			current_command = 0;
			Send_Data_Done();
			break;
		} else {
			const char *cmd13 = "AT+CIPCLOSE=1\r\n";
			GSM_Uart_Transmit((uint8_t*) cmd13, strlen(cmd13));
		}
		break;

	}

}

static void modem_response_timeout(void *arg) {
	LOG_INFO_APP("Modem response timeout in command %d\r\n", current_command);
	UTIL_TIMER_Stop(&cellularContext.cellular_response_timer_Id);
	dns_result_pending = 0;
	ignore_dns_error = 0;
	if (data_cycle_finished == 0U) {
		data_cycle_finished = 1;
		Send_Data_Done();
	}
}

void CMD_Init(void (*CmdProcessNotify)(void)) {
	//at_init();    /*Preset AT registers to defaults*/

	GSM_Uart_Init(CMD_GetChar);
	if (CmdProcessNotify != NULL) {
		NotifyCb = CmdProcessNotify;
	}
	widx = 0;
	ridx = 0;
	charCount = 0;
	i = 0;
	circBuffOverflow = 0;
	modem_status.is_at_ready = 0;
	modem_status.is_sim_ready = 0;
	modem_status.is_sms_ready = 0;
	current_command = 0;
	execute_next_command = 1;
	creg_n = 0;
	creg_stat = -1;
	cgreg_n = 0;
	cgreg_stat = -1;
	cip_close = -1;
	ignore_next_ok = 0;
	dns_result_pending = 0;
	dns_ok_seen = 0;
	ignore_dns_error = 0;
	cntp_ok_seen = 0;
	netopen_ok_seen = 0;
	data_cycle_finished = 0;
	UTIL_TIMER_Create(&(cellularContext.cellular_command_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Send_Cellular_Command_Req, 0);
	UTIL_TIMER_Create(&(cellularContext.cellular_response_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &modem_response_timeout, 0);
}

void CMD_Process(void) {

	/* Process all commands */
	if (circBuffOverflow == 1) {
		com_error(AT_TEST_PARAM_OVERFLOW);
		/*Full flush in case of overflow */
		UTILS_ENTER_CRITICAL_SECTION();
		ridx = widx;
		charCount = 0;
		circBuffOverflow = 0;
		UTILS_EXIT_CRITICAL_SECTION();
		i = 0;
	}

	while (charCount != 0) {
#if 1 /* echo On    */
		//LOG_INFO_APP(":%d, %c  ", ridx, circBuffer[ridx]);
		//LOG_INFO_APP("%c", circBuffer[ridx]);
#endif /* 0 */

		if (circBuffer[ridx] == AT_ERROR_RX_CHAR) {
			ridx++;
			if (ridx == CIRC_BUFF_SIZE) {
				ridx = 0;
			}
			UTILS_ENTER_CRITICAL_SECTION();
			charCount--;
			UTILS_EXIT_CRITICAL_SECTION();
			com_error(AT_RX_ERROR);
			i = 0;
		} else if (circBuffer[ridx] == '>') {
			ridx++;
			if (ridx == CIRC_BUFF_SIZE) {
				ridx = 0;
			}
			UTILS_ENTER_CRITICAL_SECTION();
			charCount--;
			UTILS_EXIT_CRITICAL_SECTION();
			command[0] = '>';
			command[1] = '\0';
			parse_cmd(command);
			i = 0;

		} else if ((circBuffer[ridx] == '\r') || (circBuffer[ridx] == '\n')) {
			//LOG_INFO_APP("%c",circBuffer[ridx]);
			ridx++;
			if (ridx == CIRC_BUFF_SIZE) {
				ridx = 0;
			}
			UTILS_ENTER_CRITICAL_SECTION();
			charCount--;
			UTILS_EXIT_CRITICAL_SECTION();
			if (i != 0) {
				command[i] = '\0';
				UTILS_ENTER_CRITICAL_SECTION();
				CMD_ProcessBackSpace(command);
				UTILS_EXIT_CRITICAL_SECTION();

				//LOG_INFO_APP("RRR:%s", command);

				parse_cmd(command);
				i = 0;
			}
		} else if (i == (CMD_SIZE - 1)) {
			i = 0;
			com_error(AT_TEST_PARAM_OVERFLOW);
		} else {
			command[i++] = circBuffer[ridx++];
			if (ridx == CIRC_BUFF_SIZE) {
				ridx = 0;
			}
			UTILS_ENTER_CRITICAL_SECTION();
			charCount--;
			UTILS_EXIT_CRITICAL_SECTION();
		}
	}
}

/* Private Functions Definition -----------------------------------------------*/

static int32_t CMD_ProcessBackSpace(char *cmd) {
	uint32_t i = 0;
	uint32_t bs_cnt = 0;
	uint32_t cmd_len = 0;
	/*get command length and number of backspace*/
	while (cmd[cmd_len] != '\0') {
		if (cmd[cmd_len] == '\b') {
			bs_cnt++;
		}
		cmd_len++;
	}
	/*for every backspace, remove backspace and its preceding character*/
	for (i = 0; i < bs_cnt; i++) {
		int curs = 0;
		int j = 0;

		/*set cursor to backspace*/
		while (cmd[curs] != '\b') {
			curs++;
		}
		if (curs > 0) {
			for (j = curs - 1; j < cmd_len - 2; j++) {
				cmd[j] = cmd[j + 2];
			}
			cmd[j++] = '\0';
			cmd[j++] = '\0';
			cmd_len -= 2;
		} else {
			return -1;
		}
	}
	return 0;
}

static void CMD_GetChar(uint8_t *rxChar, uint16_t size, uint8_t error) {
	charCount++;
	if (charCount == (CIRC_BUFF_SIZE + 1)) {
		circBuffOverflow = 1;
		charCount--;
	} else {
		circBuffer[widx++] = *rxChar;
		if (widx == CIRC_BUFF_SIZE) {
			widx = 0;
		}
	}

	if (NotifyCb != NULL) {
		NotifyCb();
	}
}

static void parse_cmd(const char *cmd) {
	const struct ATResponse_s *Current_ATResponse;
	int i;

	//define circular buffer variable for debug purpose to store the command *cmd
	LOG_INFO_APP(":%s\r\n", cmd);
	if (cmd[0] == '\0') {
//		status = AT_OK;
	} else if (cmd[0] == '9' && cmd[1] == '0') {
		/* should not happen as backspace are processed before */
		modem_user_dl(cmd);
	} else {
		for (i = 0; i < (sizeof(ATResponse) / sizeof(struct ATResponse_s));
				i++) {
			if (strncmp(cmd, ATResponse[i].string, ATResponse[i].size_string)
					== 0) {
				Current_ATResponse = &(ATResponse[i]);
				/* point to the string after the command to parse it */
				cmd += Current_ATResponse->size_string;

				/* parse after the command */
				switch (cmd[0]) {
				case '\0': /* nothing after the command */
					LOG_INFO_APP("\r\nRUN\r\n");
					Current_ATResponse->run(cmd);
					break;
				case ':':
					LOG_INFO_APP("\r\nSET OR GET\r\n");
					if ((cmd[1] == '?') && (cmd[2] == '\0')) {
						//status = Current_ATResponse->get(cmd + 1);
					} else {
						Current_ATResponse->set(cmd + 1);
					}
					break;
				default:
					LOG_INFO_APP("\r\nNOT FOUND\r\n");
					break;
				}
				/* we end the loop as the command was found */
				break;
			}
		}
	}

	//com_error(status);
}

static void com_error(ATEerror_t error_type) {
	if (error_type > AT_MAX) {
		error_type = AT_MAX;
	}
	LOG_INFO_APP("Error Type %d", error_type);
//	LOG_INFO_APP(ATError_description[error_type]);
}

/* USER CODE BEGIN PrFD */

////
//static int32_t sscanf_uint32_as_hhx(const char *from, uint32_t *value) {
//	return tiny_sscanf(from, "%hhx:%hhx:%hhx:%hhx",
//			&((unsigned char*) (value))[3], &((unsigned char*) (value))[2],
//			&((unsigned char*) (value))[1], &((unsigned char*) (value))[0]);
//
//}
////
//static int32_t sscanf_16_hhx(const char *from, uint8_t *pt) {
//	return tiny_sscanf(from,
//			"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
//			&pt[0], &pt[1], &pt[2], &pt[3], &pt[4], &pt[5], &pt[6], &pt[7],
//			&pt[8], &pt[9], &pt[10], &pt[11], &pt[12], &pt[13], &pt[14],
//			&pt[15]);
//
//}
/* USER CODE END PrFD */
