#include "tmp101_driver.h"

//Event flags that are used to indicate when the DMA transfer is complete.
osEventFlagsId_t i2c_write_finish_event;
osEventFlagsId_t i2c_read_finish_event;

Tmp101_status_e tmp101_write_register_dma(I2C_HandleTypeDef *hi2c, TMP101_Reg_Addr addr, uint8_t *data)
{
     Tmp101_status_e status = TMP101_SUCCESSS;

     i2c_write_finish_event = osEventFlagsNew(NULL);
     if (i2c_write_finish_event == NULL)
     {
          return TMP101_FAILURE;
     }

     uint8_t buf[3] = {addr, data[0], data[1]};
     if (HAL_I2C_Master_Transmit_DMA(hi2c, TMP101_I2C_ADDR<<1, (uint8_t *)buf, 3) != HAL_OK)
     {
          status = TMP101_FAILURE;
     }else{
          osEventFlagsWait(i2c_write_finish_event, 0x0001U, osFlagsWaitAny, osWaitForever);
     }
     osEventFlagsDelete(i2c_write_finish_event);
     return status;
}

Tmp101_status_e tmp101_read_register_dma(I2C_HandleTypeDef *hi2c, TMP101_Reg_Addr addr, uint8_t *data)
{
     Tmp101_status_e status = TMP101_SUCCESSS;
     i2c_write_finish_event = osEventFlagsNew(NULL);
     if (i2c_write_finish_event == NULL)
     {
          return TMP101_FAILURE;
     }
     i2c_read_finish_event = osEventFlagsNew(NULL);
     if (i2c_read_finish_event == NULL)
     {
          return TMP101_FAILURE;
     }

     if (HAL_I2C_Master_Transmit_DMA(hi2c, TMP101_I2C_ADDR<<1, (uint8_t *)&addr, 1) != HAL_OK)
     {
          status = TMP101_FAILURE;
     }else{
          osEventFlagsWait(i2c_write_finish_event, 0x0001U, osFlagsWaitAny, osWaitForever);
          if (HAL_I2C_Master_Receive_DMA(hi2c, TMP101_I2C_ADDR<<1, data, 2) != HAL_OK)
          {
               status = TMP101_FAILURE;
          }else{
               osEventFlagsWait(i2c_read_finish_event, 0x0001U, osFlagsWaitAny, osWaitForever);
          }
     }
     osEventFlagsDelete(i2c_read_finish_event);
     osEventFlagsDelete(i2c_write_finish_event);
     return status;
}

Tmp101_status_e tmp101_write_config_reg(Tmp101_t *tmp101, Tmp101_Conf_t *conf)
{
     uint8_t config_reg = ((conf->shutdown_conf << TMP101_CONF_SHUTDOWN_OFFSET) & TMP101_CONF_SHUTDOWN_MASK) | 
          ((conf->thermostat_conf << TMP101_CONF_THERMOSTAT_OFFSET) & TMP101_CONF_THERMOSTAT_MASK) |
          ((conf->interrupt_polarity_conf << TMP101_CONF_INT_POLARITY_OFFSET) & TMP101_CONF_INT_POLARITY_MASK) |
          ((conf->fault_queue_conf << TMP101_CONF_FAULT_QUEUE_OFFSET) & TMP101_CONF_FAULT_QUEUE_MASK) |
          ((conf->converter_res_conf << TMP101_CONF_CONV_RES_OFFSET) & TMP101_CONF_CONV_RES_MASK) | 
          ((conf->os_alert_conf << TMP101_CONF_OS_ALERT_OFFSET) & TMP101_CONF_OS_ALERT_MASK);

          uint8_t buf[] = {config_reg, 0x00};

     return tmp101_write_register_dma(tmp101->hi2c, TMP101_CONF_REG, buf);
     
}

Tmp101_status_e tmp101_write_temp_alert_high(Tmp101_t *tmp101, float temp_cel)
{
     int16_t temp_RAW = (int16_t)(temp_cel / TMP101_CEL_PER_BIT);
     uint8_t buf[2];
     buf[0] = GET_TEMP_UPPER_BYTE(temp_RAW);
     buf[1] = GET_TEMP_LOWER_BYTE(temp_RAW);

     return tmp101_write_register_dma(tmp101->hi2c, TMP101_THIGH_REG, buf);
}

Tmp101_status_e tmp101_write_temp_alert_low(Tmp101_t *tmp101, float temp_cel)
{
     int16_t temp_RAW = (int16_t)(temp_cel / TMP101_CEL_PER_BIT);
     uint8_t buf[2];
     buf[0] = GET_TEMP_UPPER_BYTE(temp_RAW);
     buf[1] = GET_TEMP_LOWER_BYTE(temp_RAW);

     return tmp101_write_register_dma(tmp101->hi2c, TMP101_TLOW_REG, buf);
}

Tmp101_status_e tmp101_read_temp(Tmp101_t *tmp101, float *temp_cel)
{
     uint8_t buf[2] = {0};

     if (tmp101_read_register_dma(tmp101->hi2c,TMP101_TEMP_REG, buf) != TMP101_SUCCESSS)
     {
          return TMP101_FAILURE;
     }

     *temp_cel = ((float)CONCAT_TEMP_BYTES(buf[0], buf[1]))* TMP101_CEL_PER_BIT;

     return TMP101_SUCCESSS;
}

Tmp101_status_e tmp101_read_temp_alert_high(Tmp101_t *tmp101, float *temp_cel)
{
     uint8_t buf[2];

     if (tmp101_read_register_dma(tmp101->hi2c,TMP101_THIGH_REG, buf) != TMP101_SUCCESSS)
     {
          return TMP101_FAILURE;
     }

     *temp_cel = ((float)CONCAT_TEMP_BYTES(buf[0], buf[1]))* TMP101_CEL_PER_BIT;

     return TMP101_SUCCESSS;
}

Tmp101_status_e tmp101_read_temp_alert_low(Tmp101_t *tmp101, float *temp_cel)
{
     uint8_t buf[2];

     if (tmp101_read_register_dma(tmp101->hi2c,TMP101_TLOW_REG, buf) != TMP101_SUCCESSS)
     {
          return TMP101_FAILURE;
     }

     *temp_cel = ((float)CONCAT_TEMP_BYTES(buf[0], buf[1]))* TMP101_CEL_PER_BIT;

     return TMP101_SUCCESSS;
}

Tmp101_status_e tmp101_read_config_reg(Tmp101_t *tmp101, Tmp101_Conf_t *conf)
{
     uint8_t buf[2];

     if (tmp101_read_register_dma(tmp101->hi2c, TMP101_CONF_REG, buf) != TMP101_SUCCESSS)
     {
          return TMP101_FAILURE;
     }

     conf->shutdown_conf = (buf[0] & TMP101_CONF_SHUTDOWN_MASK) >> TMP101_CONF_SHUTDOWN_OFFSET;
     conf->thermostat_conf = (buf[0] & TMP101_CONF_THERMOSTAT_MASK) >> TMP101_CONF_THERMOSTAT_OFFSET;
     conf->interrupt_polarity_conf = (buf[0] & TMP101_CONF_INT_POLARITY_MASK) >> TMP101_CONF_INT_POLARITY_OFFSET;
     conf->fault_queue_conf = (buf[0] & TMP101_CONF_FAULT_QUEUE_MASK) >> TMP101_CONF_FAULT_QUEUE_OFFSET;
     conf->converter_res_conf = (buf[0] & TMP101_CONF_CONV_RES_MASK) >> TMP101_CONF_CONV_RES_OFFSET;
     conf->os_alert_conf = (buf[0] & TMP101_CONF_OS_ALERT_MASK) >> TMP101_CONF_OS_ALERT_OFFSET;

     return TMP101_SUCCESSS;
}

//DMA I2C transmit call is complete
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
     //Indicate to tmp101_write_register_dma that the DMA transfer is complete.
     osEventFlagsSet(i2c_write_finish_event, 0x0001U);
}

//DMA I2C recieve call is complete
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
     //Indicate to tmp101_read_register_dma that the DMA transfer is complete.
     osEventFlagsSet(i2c_read_finish_event, 0x0001U);
}