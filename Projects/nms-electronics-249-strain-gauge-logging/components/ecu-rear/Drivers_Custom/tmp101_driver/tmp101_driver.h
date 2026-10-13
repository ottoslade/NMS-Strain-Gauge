#ifndef __DRIVER_TMP101_H__
#define __DRIVER_TMP101_H__

#include "i2c.h"
#include "cmsis_os2.h"

//Register of the TMP101
#define TMP101_I2C_ADDR (0x48)

//Bit mask of the config registers
#define TMP101_CONF_SHUTDOWN_MASK 0xb00000001
#define TMP101_CONF_THERMOSTAT_MASK 0b00000010
#define TMP101_CONF_INT_POLARITY_MASK 0b00000100
#define TMP101_CONF_FAULT_QUEUE_MASK 0b00011000
#define TMP101_CONF_CONV_RES_MASK 0b01100000
#define TMP101_CONF_OS_ALERT_MASK 0b10000000

//Bit offset of the config registers
#define TMP101_CONF_SHUTDOWN_OFFSET 0
#define TMP101_CONF_THERMOSTAT_OFFSET 1
#define TMP101_CONF_INT_POLARITY_OFFSET 2
#define TMP101_CONF_FAULT_QUEUE_OFFSET 3
#define TMP101_CONF_CONV_RES_OFFSET 5
#define TMP101_CONF_OS_ALERT_OFFSET 7

//Macro for joining the two bytes and justified to MSB
#define CONCAT_TEMP_BYTES(upper, lower) (((int16_t)(upper << 4)) | ((int16_t)(lower >> 4)))

//Get the upper byte from a joined value
#define GET_TEMP_UPPER_BYTE(temp) ((uint8_t)(temp>>4))

//Get the lower byte from a joined value
#define GET_TEMP_LOWER_BYTE(temp) ((uint8_t)((temp & 0x000f) << 4))

//TMP101 Register values
typedef enum tmp101_register_address {
     TMP101_TEMP_REG = 0x00,
     TMP101_CONF_REG = 0x01,
     TMP101_TLOW_REG = 0x02,
     TMP101_THIGH_REG = 0x03
} TMP101_Reg_Addr;

//Shutdown config values
typedef enum {
     SHUTDOWN_UNSET = 0x00,
     SHUTDOWN_SET = 0x01
} TMP101_Shutdown_Conf;

//Thermostat modes
typedef enum {
     COMPARATOR_MODE = 0x00,
     INTERRUPT_MODE = 0x01
} TMP101_Thermostat_Conf;

//Interrupt polarity modes
typedef enum {
     ACTIVE_LOW = 0x00,
     ACTIVE_HIGH = 0x01
} TMP101_Interrupt_Polarity_Conf;

//Fault count trigger configs
typedef enum {
     FAULT_COUNT_1 = 0x00,
     FAULT_COUNT_2 = 0x01,
     FAULT_COUNT_4 = 0x02,
     FAULT_COUNT_6 = 0x03
} TMP101_Fault_Queue_Conf;

//Converter resolution modes
typedef enum {
     BITS_9_MILLISEC_40 = 0x00,
     BITS_10_MILLISEC_80 = 0x01,
     BITS_11_MILLISEC_160 = 0x02,
     BITS_12_MILLISEC_320 = 0x03
} TMP101_Conveter_Res_Conf;

//Alert mode
typedef enum {
     NORMAL_OPER = 0x00,
     SINGLE_CONV = 0x01
} TMP101_Os_Alert_Conf;

//TMP101 Configuration
typedef struct {
     TMP101_Shutdown_Conf shutdown_conf;
     TMP101_Thermostat_Conf thermostat_conf;
     TMP101_Interrupt_Polarity_Conf interrupt_polarity_conf;
     TMP101_Fault_Queue_Conf fault_queue_conf;
     TMP101_Conveter_Res_Conf converter_res_conf;
     TMP101_Os_Alert_Conf os_alert_conf;
} Tmp101_Conf_t;

//TMP101 handle
typedef struct
 {
     I2C_HandleTypeDef *hi2c;
} Tmp101_t;
 
//TMP101 return status
typedef enum {
     TMP101_SUCCESSS = 0x00,
     TMP101_FAILURE  = 0x01
} Tmp101_status_e;

//Constant for conversion from bits to celcius
static const float TMP101_CEL_PER_BIT = 0.0625;

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param conf                The Tmp101_Conf_t pointer that will be referenced for setting the configuration settings.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_write_config_reg(Tmp101_t *tmp101, Tmp101_Conf_t *conf);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param temp_cel                The celcius value that will be used as the temp high limit.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_write_temp_alert_high(Tmp101_t *tmp101, float temp_cel);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param temp_cel            The celcius value that will be used as the temp low limit.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_write_temp_alert_low(Tmp101_t *tmp101, float temp_cel);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param conf                The Tmp101_Conf_t pointer that will be referenced for storing the configuration settings.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_read_config_reg(Tmp101_t *tmp101, Tmp101_Conf_t *conf);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param temp_cel            The variable that will be used to store the temperature read from the TMP101.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_read_temp(Tmp101_t *tmp101, float *temp_cel);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param temp_cel            The variable that will be used to store the high temp limit on the TMP101.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_read_temp_alert_high(Tmp101_t *tmp101, float *temp_cel);

/**
 * @brief 
 * 
 * @param tmp101              The Tmp101_t pointer that will be used to reference correct i2c port
 * @param temp_cel            The variable that will be used to store the low temp limit on the TMP101.
 * @return Tmp101_status_e    The status of the TMP101 function success.
 */
Tmp101_status_e tmp101_read_temp_alert_low(Tmp101_t *tmp101, float *temp_cel);


#endif //__DRIVER_TMP101_H__