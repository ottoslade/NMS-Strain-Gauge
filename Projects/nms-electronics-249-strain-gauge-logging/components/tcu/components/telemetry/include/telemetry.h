#pragma once

#include "can.h"
#include "esp_err.h"

/**
 * @brief 
 * 
 * @param tx_msg 
 * @return esp_err_t 
 */
esp_err_t telemetryTx(CanMsg_t *tx_msg, TickType_t timestamp);

/**
 * @brief 
 * 
 */
void initTelemetryPipeline(void);