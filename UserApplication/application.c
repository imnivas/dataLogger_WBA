/*
 * application.c
 *
 *  Created on: Oct 5, 2025
 *      Author: karthig
 */

#include "application.h"
#include "flash_manager.h"
#include "ll_sys_if.h"
#include "cellular.h"
#include "log_module.h"
#include "stm32_seq.h"
#include "stm32_timer.h"
#include "stm32_lpm.h"
#include "command.h"
#include "usart.h"
#include <stdint.h>
#include <string.h>
#include "app_ble.h"
#include "adc_ctrl.h"
#include "adc_ctrl_conf.h"
#include "temp_measurement.h"
#include "stm32wbaxx_ll_adc.h"
#include "stm32_systime.h"

ApplicationContext_t applicationContext;

uint8_t pzem_payload[20] = {0};

Payload_t payload;
static Packet_t  tcp_packet;
static uint8_t   tcp_payload_buf[MAX_PAYLOAD_SIZE];
static uint8_t   tx_buf[MAX_PACKET_BUF];
static uint32_t  uplink_fcnt = 0;
static uint32_t  downlink_fcnt = 0;
static uint8_t cellular_cycle_active = 0;

AppConfig_t app_config;

/* ---- AppConfig flash callbacks (pattern from ota_app.c) ----------------- */
static void FM_Config_WriteCallback(FM_FlashOp_Status_t status)
{
    if (status == FM_OPERATION_COMPLETE)
        LOG_INFO_APP("AppConfig: flash write complete\r\n");
    else
        LOG_INFO_APP("AppConfig: flash write error\r\n");
}

static FM_CallbackNode_t fm_write_cb_node = {
    .NodeList = { .next = NULL, .prev = NULL },
    .Callback = FM_Config_WriteCallback
};

static void FM_Config_EraseCallback(FM_FlashOp_Status_t status)
{
    if (status == FM_OPERATION_COMPLETE) {
        LOG_INFO_APP("AppConfig: erase done, writing\r\n");
        FM_Write((uint32_t *)&app_config,
                 (uint32_t *)APP_CONFIG_FLASH_ADDR,
                 sizeof(AppConfig_t) / 4,
                 &fm_write_cb_node);
    } else {
        LOG_INFO_APP("AppConfig: flash erase error\r\n");
    }
}

static FM_CallbackNode_t fm_erase_cb_node = {
    .NodeList = { .next = NULL, .prev = NULL },
    .Callback = FM_Config_EraseCallback
};

void AppConfig_FillHwIds(void)
{
    uint32_t udn        = LL_FLASH_GetUDN();
    uint32_t company_id = LL_FLASH_GetSTCompanyID();
    uint32_t device_id  = LL_FLASH_GetDeviceID();

    app_config.config.eui64[0] = (uint8_t)((company_id >> 16) & 0xFF);
    app_config.config.eui64[1] = (uint8_t)((company_id >> 8)  & 0xFF);
    app_config.config.eui64[2] = (uint8_t)(company_id & 0xFF);
    app_config.config.eui64[3] = (uint8_t)(device_id & 0xFF);
    app_config.config.eui64[4] = (uint8_t)((udn >> 24) & 0xFF);
    app_config.config.eui64[5] = (uint8_t)((udn >> 16) & 0xFF);
    app_config.config.eui64[6] = (uint8_t)((udn >> 8)  & 0xFF);
    app_config.config.eui64[7] = (uint8_t)(udn & 0xFF);

    /* BD address — same formula as app_ble.c BleGenerateBdAddress */
    app_config.config.ble_addr[0] = (uint8_t)(udn & 0xFF);
    app_config.config.ble_addr[1] = (uint8_t)((udn >> 8) & 0xFF);
    app_config.config.ble_addr[2] = (uint8_t)((udn >> 16) & 0xFF);
    app_config.config.ble_addr[3] = (uint8_t)(company_id & 0xFF);
    app_config.config.ble_addr[4] = (uint8_t)((company_id >> 8) & 0xFF);
    app_config.config.ble_addr[5] = (uint8_t)((company_id >> 16) & 0xFF);

    app_config.config.fw_version = APP_CONFIG_FIRMWARE_VERSION;
    app_config.config.hw_version = APP_CONFIG_HARDWARE_VERSION;
}

static void AppConfig_ApplyDefaults(void)
{
    memset(&app_config, 0, sizeof(app_config));
    app_config.magic1                      = APP_CONFIG_MAGIC;
    app_config.version                     = APP_CONFIG_VERSION;
    app_config.config.frame_head           = USER_CONFIG_FRAME_HEAD;
    strncpy(app_config.config.apn,         "airtelgprs.com",    sizeof(app_config.config.apn) - 1);
    strncpy(app_config.config.server_addr, "platform.adarko.io", sizeof(app_config.config.server_addr) - 1);
    app_config.config.server_port          = 8900;
    app_config.config.modbus_slave_id      = 2;
    app_config.config.send_interval_mins   = 1440; /* 24 hours */
}

void AppConfig_Load(void)
{
    const AppConfig_t *f = (const AppConfig_t *)APP_CONFIG_FLASH_ADDR;
    if (f->magic1 == APP_CONFIG_MAGIC && f->magic2 == APP_CONFIG_MAGIC) {
        memcpy(&app_config, f, sizeof(AppConfig_t));
        LOG_INFO_APP("AppConfig: loaded from flash\r\n");
    } else {
        AppConfig_ApplyDefaults();
		AppConfig_Save();
        LOG_INFO_APP("AppConfig: defaults applied\r\n");
    }
    /* Always re-derive HW IDs from silicon — never trust stored values */
    AppConfig_FillHwIds();
}

void AppConfig_Save(void)
{
    app_config.magic1  = APP_CONFIG_MAGIC;
    app_config.version = APP_CONFIG_VERSION;
    app_config.magic2  = APP_CONFIG_MAGIC;
    FM_Erase(APP_CONFIG_FLASH_SECTOR, 1, &fm_erase_cb_node);
}

/* ------------------------------------------------------------------------- */

static void Log_EUI64(void)
{
    const uint8_t *e = app_config.config.eui64;
    const uint8_t *b = app_config.config.ble_addr;
    LOG_INFO_APP("BD  Addr : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                 b[5], b[4], b[3], b[2], b[1], b[0]);
    LOG_INFO_APP("EUI-64   : %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                 e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
}

static void Send_Data_Req(void *arg);
static void Send_Data(void);
static void Reset_Initiate_Req(void *arg);
static void Reset_Initiate(void);
void MeterReadProcessInit(void);

void UserApplicationInit(void) {
	LOG_INFO_APP("UserApplication Init\n");

	//Call BleGetBdAddress and log the generated BD address before loading AppConfig, to verify that BD address generation does not depend on AppConfig values
	const uint8_t *bd_addr = BleGetBdAddress();
	LOG_INFO_APP("Generated BD Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
			bd_addr[5], bd_addr[4], bd_addr[3], bd_addr[2], bd_addr[1], bd_addr[0]);	
	//generate EUI-64 and log it as well, by appending LL_FLASH_GetSTCompanyID and LL_FLASH_GetDeviceID to the BD address, to verify that EUI-64 generation is correct and does not depend on AppConfig values

		

	AppConfig_Load();
	Log_EUI64();
	SysTime_t sysTime = SysTimeGet();
	LOG_INFO_APP("SysTime: %lu s  SubSeconds: %d\r\n",
	             (unsigned long)sysTime.Seconds, sysTime.SubSeconds);
    LOG_INFO_APP("Firmware Version: 0x%04X\r\n", APP_CONFIG_FIRMWARE_VERSION);
    LOG_INFO_APP("Hardware Version: 0x%04X\r\n", APP_CONFIG_HARDWARE_VERSION);

    //Display the loaded configuration values in the log
    LOG_INFO_APP("APN: %s\r\n", app_config.config.apn);
    LOG_INFO_APP("Server Address: %s\r\n", app_config.config.server_addr);
    LOG_INFO_APP("Server Port: %d\r\n", app_config.config.server_port);
    LOG_INFO_APP("Modbus Slave ID: %d\r\n", app_config.config.modbus_slave_id);
    LOG_INFO_APP("Send Interval (mins): %d\r\n", app_config.config.send_interval_mins);


	VREFMEAS_Init();
	CAPMEAS_Init();
	TEMPMEAS_Init();

    { ADCValue_t _adc = ReadVolatges(); app_config.config.u8Vref_V = _adc.u8Vref_V; app_config.config.u8Temp_C = _adc.u8Temp_C; }

	tcp_packet.Data    = tcp_payload_buf;
	payload.Buffer     = tx_buf;
	payload.BufferSize = 0;

	UTIL_SEQ_RegTask(1U << CFG_TASK_CELLULAR_SEND_DATA, UTIL_SEQ_RFU,
			Send_Data);
    
    UTIL_SEQ_RegTask(1U << CFG_TASK_RESET_INITIATE, UTIL_SEQ_RFU,
			Reset_Initiate);

	UTIL_TIMER_Create(&(applicationContext.SEND_Data_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Send_Data_Req, 0);
    
    UTIL_TIMER_Create(&(applicationContext.Reset_Initate_timer_Id), 0,
			UTIL_TIMER_ONESHOT, &Reset_Initiate_Req, 0);

	/*First Data after boot will be in 5 Seconds*/
	UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id,
			5000U);

}

static void Reset_Initiate_Req(void *arg) {
    LOG_INFO_APP("Reset Initiate Req\n");
    UTIL_TIMER_Stop(&applicationContext.Reset_Initate_timer_Id);
    UTIL_SEQ_SetTask(1 << CFG_TASK_RESET_INITIATE, CFG_SEQ_PRIO_0);
}

static void Reset_Initiate(void) {
    LOG_INFO_APP("Reset Initiate\n");
    NVIC_SystemReset();
}

static void Send_Data_Req(void *arg) {
	LOG_INFO_APP("Send_Data Req\n");
	UTIL_TIMER_Stop(&applicationContext.SEND_Data_timer_Id);
	UTIL_SEQ_SetTask(1 << CFG_TASK_CELLULAR_SEND_DATA, CFG_SEQ_PRIO_0);
	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
				CMD_Process);

}

static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
	uint16_t crc = 0xFFFF;
	for (uint16_t i = 0; i < len; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
	}
	return crc;
}

void RS485_ReadPZEM(void) {
	uint8_t modbus_req[8] = {app_config.config.modbus_slave_id, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00};
	uint16_t crc = modbus_crc16(modbus_req, 6);
	modbus_req[6] = crc & 0xFF;
	modbus_req[7] = (crc >> 8) & 0xFF;
	uint8_t buf[25] = {0};

	HAL_GPIO_WritePin(SLAVE_SW_GPIO_Port, SLAVE_SW_Pin, GPIO_PIN_SET);
	HAL_Delay(20);

	/* Flush any stale bytes in RX buffer before sending request */
	HAL_UART_AbortReceive(&hlpuart1);
	uint8_t dummy;
	while (HAL_UART_Receive(&hlpuart1, &dummy, 1, 5) == HAL_OK);

	HAL_UART_Transmit(&hlpuart1, (uint8_t *)modbus_req, sizeof(modbus_req), 100);

	if (HAL_UART_Receive(&hlpuart1, buf, sizeof(buf), 500) == HAL_OK) {
		if (buf[0] == app_config.config.modbus_slave_id && buf[1] == 0x04 && buf[2] == 0x14) {
			memcpy(pzem_payload, &buf[3], 20);
			LOG_INFO_APP("PZEM-016 read OK\r\n");
		} else {
			LOG_INFO_APP("PZEM-016 bad response: got %02X %02X %02X (expected 02 04 14)\r\n",
					buf[0], buf[1], buf[2]);
		}
        LOG_INFO_APP("PZEM raw[0..9]:  %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					buf[0], buf[1], buf[2], buf[3], buf[4],
					buf[5], buf[6], buf[7], buf[8], buf[9]);
			LOG_INFO_APP("PZEM raw[10..24]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
					buf[10], buf[11], buf[12], buf[13], buf[14],
					buf[15], buf[16], buf[17], buf[18], buf[19],
					buf[20], buf[21], buf[22], buf[23], buf[24]);
	} else {
		LOG_INFO_APP("PZEM-016 timeout (HAL_UART_Receive returned error)\r\n");
	}

	HAL_GPIO_WritePin(SLAVE_SW_GPIO_Port, SLAVE_SW_Pin, GPIO_PIN_RESET);
}

static void Send_Data(void) {
	cellular_cycle_active = 1;
	UTIL_LPM_SetStopMode(1U << CFG_LPM_APP, UTIL_LPM_DISABLE);
	RS485_ReadPZEM();
	BuildPayload();
	MeterReadProcessInit();
	CellularInit();
}

static uint16_t Packet_Serialize(const Packet_t *pkt, uint8_t *out)
{
    uint16_t offset = 0;
    memcpy(out + offset, &pkt->Header, sizeof(PacketHeader_t));
    offset += sizeof(PacketHeader_t);
    memcpy(out + offset, pkt->Data, pkt->Header.Len);
    offset += pkt->Header.Len;
    uint16_t crc = modbus_crc16(out, offset);
    memcpy(out + offset, &crc, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    return offset;
}

void BuildPayload(void) {
    ADCValue_t adc = ReadVolatges();

    /* Fill data payload buffer — track actual length written */
    uint16_t plen = 0;
    memcpy(&tcp_payload_buf[plen], &app_config.config.fw_version, sizeof(uint16_t)); plen += 2;
    memcpy(&tcp_payload_buf[plen], &app_config.config.hw_version, sizeof(uint16_t)); plen += 2;
    tcp_payload_buf[plen++] = adc.u8Vref_V;
    tcp_payload_buf[plen++] = adc.u8Bkup_V;
    tcp_payload_buf[plen++] = adc.u8Temp_C;
    memcpy(&tcp_payload_buf[plen], pzem_payload, 20); plen += 20;

    /* Fill header */
    memset(&tcp_packet.Header, 0, sizeof(PacketHeader_t));
    memcpy(tcp_packet.Header.EUI, app_config.config.eui64, 8);
    tcp_packet.Header.UplinkFCnt   = uplink_fcnt++;
    tcp_packet.Header.DownlinkFCnt = downlink_fcnt;
    tcp_packet.Header.ACKReq       = 0;
    tcp_packet.Header.FPort        = 1;
    tcp_packet.Header.FCtrl        = 0;
    tcp_packet.Header.Len          = plen;

    /* Serialize to flat TX buffer */
    payload.BufferSize = (uint8_t)Packet_Serialize(&tcp_packet, tx_buf);

    LOG_INFO_APP("Payload: %d bytes  FCnt:%lu  Temp:%d C  Vref:%d mV  Bkup:%d mV\r\n",
                 payload.BufferSize, (unsigned long)tcp_packet.Header.UplinkFCnt,
                 (int)adc.u16Temp_C, adc.u16Vref_mV, adc.u16Bkup_mV);
}

void Send_Data_Done(void){
   if (cellular_cycle_active == 0U) {
      return;
   }
   cellular_cycle_active = 0;
   CellularDeInit();
   UTIL_LPM_SetStopMode(1U << CFG_LPM_APP, UTIL_LPM_ENABLE);
   UTIL_TIMER_StartWithPeriod(&applicationContext.SEND_Data_timer_Id,
			app_config.config.send_interval_mins * 60000U);
}


static void CmdProcessNotify(void) {

	//APP_LOG(TS_OFF, VLEVEL_M, "Intrup");
	UTIL_SEQ_SetTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), 0);

}

void MeterReadProcessInit(void) {
	MX_USART2_UART_Init();
//	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_18);
//	UTIL_SEQ_RegTask((1 << CFG_TASK_CELLULAR_COMMAND_Rx), UTIL_SEQ_RFU,
//			CMD_Process);

	CMD_Init(CmdProcessNotify);

}



/* ---- ADC handles (LL constants, nested ADCCTRL_InitConfig_t) ------------ */
#define ADC_COMMON_INIT_CONF \
    .ConvParams = { \
        .TriggerFrequencyMode = LL_ADC_TRIGGER_FREQ_LOW, \
        .Resolution           = LL_ADC_RESOLUTION_12B, \
        .DataAlign            = LL_ADC_DATA_ALIGN_RIGHT, \
        .TriggerStart         = LL_ADC_REG_TRIG_SOFTWARE, \
        .TriggerEdge          = LL_ADC_REG_TRIG_EXT_RISING, \
        .ConversionMode       = LL_ADC_REG_CONV_SINGLE, \
        .DmaTransfer          = LL_ADC_REG_DMA_TRANSFER_NONE, \
        .Overrun              = LL_ADC_REG_OVR_DATA_OVERWRITTEN, \
        .SamplingTimeCommon1  = LL_ADC_SAMPLINGTIME_814CYCLES_5, \
        .SamplingTimeCommon2  = LL_ADC_SAMPLINGTIME_1CYCLE_5 \
    }, \
    .SeqParams = { \
        .Setup    = LL_ADC_REG_SEQ_CONFIGURABLE, \
        .Length   = LL_ADC_REG_SEQ_SCAN_DISABLE, \
        .DiscMode = LL_ADC_REG_SEQ_DISCONT_DISABLE \
    }, \
    .LowPowerParams = { \
        .AutoPowerOff  = DISABLE, \
        .AutonomousDPD = LL_ADC_LP_AUTONOMOUS_DPD_DISABLE \
    }

ADCCTRL_Handle_t LLVRefIntRequest_Handle = {
    .Uid = 0x01, .State = ADCCTRL_HANDLE_NOT_REG,
    .InitConf    = { ADC_COMMON_INIT_CONF },
    .ChannelConf = { .Channel = LL_ADC_CHANNEL_VREFINT,   .Rank = LL_ADC_REG_RANK_1, .SamplingTime = LL_ADC_SAMPLINGTIME_COMMON_1 }
};

ADCCTRL_Handle_t LLVCAPRequest_Handle = {
    .Uid = 0x02, .State = ADCCTRL_HANDLE_NOT_REG,
    .InitConf    = { ADC_COMMON_INIT_CONF },
    .ChannelConf = { .Channel = LL_ADC_CHANNEL_3,         .Rank = LL_ADC_REG_RANK_1, .SamplingTime = LL_ADC_SAMPLINGTIME_COMMON_1 }
};

/* ---- Init ---------------------------------------------------------------- */
VREFMEAS_Cmd_Status_t VREFMEAS_Init(void) {
    ADCCTRL_Cmd_Status_t eReturn = ADCCTRL_RegisterHandle(&LLVRefIntRequest_Handle);
    return ((eReturn == ADCCTRL_OK) || (eReturn == ADCCTRL_HANDLE_ALREADY_REGISTERED))
           ? VREFMEAS_OK : VREFMEAS_ADC_INIT;
}

CAPMEAS_Cmd_Status_t CAPMEAS_Init(void) {
    ADCCTRL_Cmd_Status_t eReturn = ADCCTRL_RegisterHandle(&LLVCAPRequest_Handle);
    return ((eReturn == ADCCTRL_OK) || (eReturn == ADCCTRL_HANDLE_ALREADY_REGISTERED))
           ? CAPMEAS_OK : CAPMEAS_ADC_INIT;
}

/* TEMPMEAS_Init() is provided by temp_measurement.c — call it at boot     */

/* ---- Measurements -------------------------------------------------------- */
uint16_t VREFMEAS_RequestVrefMeasurement(void) {
    uint16_t vref_value = 0;
    UTILS_ENTER_LIMITED_CRITICAL_SECTION(RCC_INTR_PRIO << 4);
    ADCCTRL_RequestIpState(&LLVRefIntRequest_Handle, ADC_ON);
    ADCCTRL_RequestRefVoltage(&LLVRefIntRequest_Handle, &vref_value);
    ADCCTRL_RequestIpState(&LLVRefIntRequest_Handle, ADC_OFF);
    UTILS_EXIT_LIMITED_CRITICAL_SECTION();
    return vref_value;
}

uint16_t CAPMEAS_RequestCapMeasurement(void) {
    uint16_t cap_value = 0;
    UTILS_ENTER_LIMITED_CRITICAL_SECTION(RCC_INTR_PRIO << 4);
    ADCCTRL_RequestIpState(&LLVCAPRequest_Handle, ADC_ON);
    ADCCTRL_RequestRawValue(&LLVCAPRequest_Handle, &cap_value);
    ADCCTRL_RequestIpState(&LLVCAPRequest_Handle, ADC_OFF);
    UTILS_EXIT_LIMITED_CRITICAL_SECTION();
    return cap_value;
}

ADCValue_t ReadVolatges(void) {
    ADCValue_t mV;
    uint16_t Vref = VREFMEAS_RequestVrefMeasurement();
    uint16_t Vcap = CAPMEAS_RequestCapMeasurement();

    /* Read temperature: use raw count + actual Vdda for accurate calibration.
     * ADCCTRL_RequestTemperature uses hardcoded VDDA_APPLI=3300mV which
     * differs from the measured supply, causing a fixed offset error. */
    int16_t temp_raw = 0;
    uint16_t temp_adc_raw = 0;
    UTILS_ENTER_LIMITED_CRITICAL_SECTION(RCC_INTR_PRIO << 4);
    ADCCTRL_RequestIpState(&LLTempRequest_Handle, ADC_ON);
    ADCCTRL_RequestRawValue(&LLTempRequest_Handle, &temp_adc_raw);
    ADCCTRL_RequestIpState(&LLTempRequest_Handle, ADC_OFF);
    UTILS_EXIT_LIMITED_CRITICAL_SECTION();
    temp_raw = (int16_t)__LL_ADC_CALC_TEMPERATURE(Vref, temp_adc_raw,
                                                   LL_ADC_RESOLUTION_12B);

    /* Encode: u8Temp_C = temp + 40 (covers -40..+215 °C); u16Temp_C = raw °C */
    int16_t encoded = temp_raw + TEMPMEAS_MIN_TEMP_LIMIT;
    if (encoded < 0)   encoded = 0;
    if (encoded > 255) encoded = 255;
    uint8_t Temp = (uint8_t)encoded;

    mV.u16Vref_mV = Vref;
    mV.u8Temp_C   = Temp;
    mV.u16Temp_C  = temp_raw;

    LOG_INFO_APP("Vref: %d mV\r\n", Vref);
    LOG_INFO_APP("Vcap raw: %d\r\n", Vcap);
    LOG_INFO_APP("Temp: %d C (raw=%u, encoded u8=%d)\r\n", (int)temp_raw, temp_adc_raw, Temp);

    if (Vref < 2000) Vref = 2000;
    mV.u8Vref_V = (uint8_t)((Vref / 10) - 200);

    /* Vcap is a raw 12-bit count; convert to mV using Vref, then encode */
    uint16_t bkup_mV = (uint16_t)((Vcap / 4095.0f) * Vref);
    mV.u16Bkup_mV = bkup_mV;
    if (bkup_mV < 2000) bkup_mV = 2000;
    mV.u8Bkup_V = (uint8_t)((bkup_mV / 10) - 200);

    return mV;
}
/* ------------------------------------------------------------------------- */