#include "stm_flash.h"
#include "FreeRTOS.h"
#include "can.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

// Macro for error checking
#define IS_ESP_OK(x)                                                                               \
  if ((x) != ESP_OK)                                                                               \
    break;
#define HIGH              1
#define LOW               0
#define BASE_PATH         "/spiffs/"
#define ACK               0x79
#define NACK              0x1F
#define CAN_TIMEOUT       2000
#define CAN_ERASE_TIMEOUT 10000
#define STM_BOOT_ADDRESS  {0x08, 0x00, 0xC0, 0x00} //0x0800C000

static const char *TAG_STM_FLASH = "stm_flash";

#ifdef RESET_OVER_CAN
typedef enum
{
  APP_RESET,
  BOOTLOADER_RESET
} target_reset_t;
#endif // RESET_OVER_CAN

/**
 * @brief Function to wait for an ACK over CAN.
 *
 * @param id CAN id
 * @param timeout timeout for waiting for the ACK
 * @return esp_err_t ESP_OK if successful
 */
static esp_err_t waitForCanACK(uint16_t id, uint32_t timeout)
{
  CanMsg_t rx_message;
  if (bootloaderCanRx(&rx_message, timeout) == ESP_OK)
  {
    if ((rx_message.id == id && rx_message.data.data_u8[0] == ACK))
    {
      return ESP_OK;
    }
    else if (rx_message.data.data_u8[0] == NACK)
    {
      ESP_LOGE(TAG_STM_FLASH, "Received NACK from ID: %d", (int)rx_message.id);
      return ESP_FAIL;
    }
    else
    {
      ESP_LOGE(TAG_STM_FLASH, "Received invalid response.");
      return ESP_FAIL;
    }
  }
  ESP_LOGE(TAG_STM_FLASH, "TIMEOUT ERROR, did not receive ACK");
  return ESP_FAIL;
}

#ifdef RESET_OVER_CAN
/**
 * @brief function to reset STM over CAN
 *
 * @param reset_type STM application vs bootloader reset procedure
 * @return esp_err_t
 */
static esp_err_t cmdReset(target_reset_t reset_type)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "Starting RESET Procedure");
  CanMsg_t can_message = {.id = 0x50, .dlc = 1, .data.data_u8 = {0x01}};

  if (canTx(&can_message))
  {
    switch (reset_type)
    {
      case BOOTLOADER_RESET:
        if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
        {
          ESP_LOGI(TAG_STM_FLASH, "Got first ACK, waiting for second");
          return waitForCanACK(can_message.id, CAN_TIMEOUT);
        }
        break;
      case APP_RESET:
        ESP_LOGI(TAG_STM_FLASH, "%s", "RESET sent, waiting for bootloader to launch");
        return ESP_OK;
    }
  }

  return ESP_FAIL;
}
#else  // !RESET_OVER_CAN

/**
 * @brief Function to initialize the reset pin and set it to HIGH
 *
 * @param reset_pin gpio pin in the ESP connected to the reset pin of the STM
 */
static void initGPIO(gpio_num_t reset_pin)
{
  gpio_set_direction(reset_pin, GPIO_MODE_OUTPUT);
  gpio_set_level(reset_pin, HIGH);
}

/**
 * @brief Function to de-initialize the reset pin
 *
 * @param reset_pin gpio pin in the ESP connected to the reset pin of the STM
 */
static void deInitGPIO(gpio_num_t reset_pin)
{
  gpio_set_direction(reset_pin, GPIO_MODE_DISABLE);
  gpio_reset_pin(reset_pin);
}

/**
 * @brief function to reset STM
 *
 * @param reset_pin GPIO pin connected to STM reset
 */
static void resetSTM(gpio_num_t reset_pin)
{
  // Needs to be reimplemented? Do we have to reset STM?
  ESP_LOGI(TAG_STM_FLASH, "%s", "Starting RESET Procedure");

  gpio_set_level(reset_pin, LOW);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  gpio_set_level(reset_pin, HIGH);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  ESP_LOGI(TAG_STM_FLASH, "%s", "Finished RESET Procedure");
}
#endif // !RESET_OVER_CAN

/**
 * @brief Bootloader ID command over CAN
 *
 * @return esp_err_t
 */
static esp_err_t cmdId(char *error)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "CHECK ID");
  CanMsg_t can_message = {.id = 0x02, .dlc = 1, .data.data_u8 = {0x00}};

  const uint8_t bootloader_id[2] = {0x04, 0x82};
  if (canTx(&can_message))
  {
    if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
    {
      CanMsg_t id_message;
      if (bootloaderCanRx(&id_message, CAN_TIMEOUT) == ESP_OK)
      {
        if (id_message.id == can_message.id)
        {
          if (memcmp(&bootloader_id[0], &id_message.data.data_u8[0], (size_t)id_message.dlc) == 0)
          {
            if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
            {
              return ESP_OK;
            }
            else
            {
              sprintf(error, "ID CMD FAILED: did not receive second ACK");
            }
          }
          else
          {
            sprintf(error, "ID CMD FAILED: invalid bootloader ID");
          }
        }
        else
        {
          sprintf(error, "ID CMD FAILED: response has invalid CAN id");
        }
      }
      else
      {
        sprintf(error, "ID CMD FAILED: did not receive a CAN response");
      }
    }
    else
    {
      sprintf(error, "ID CMD FAILED: did not receive first ACK");
    }
  }
  else
  {
    sprintf(error, "ID CMD FAILED: transmission failed");
  }
  ESP_LOGE(TAG_STM_FLASH, "%s", error);
  return ESP_FAIL;
}

/**
 * @brief Bootloader MASS ERASE command over CAN
 *
 * @return esp_err_t
 */
static esp_err_t cmdMassErase(char *error)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "MASS ERASE MEMORY");
  CanMsg_t can_message = {.id = 0x43, .dlc = 1, .data.data_u8 = {0xFF}};

  if (canTx(&can_message))
  {
    if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
    {
      // The erase procedure occurs between the first and second ACKs (takes long af)
      if (waitForCanACK(can_message.id, CAN_ERASE_TIMEOUT) == ESP_OK)
      {
        if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
        {
          return ESP_OK;
        }
        else
        {
          sprintf(error, "ERASE CMD FAILED: did not receive third ACK");
        }
      }
      else
      {
        sprintf(error, "ERASE CMD FAILED: did not receive second ACK");
      }
    }
    else
    {
      sprintf(error, "ERASE CMD FAILED: did not receive first ACK");
    }
  }
  else
  {
    sprintf(error, "ERASE CMD FAILED: transmission failed");
  }
  ESP_LOGE(TAG_STM_FLASH, "%s", error);
  return ESP_FAIL;
}

/**
 * @brief Bootloader command for which sectors to erase. DO NOT CALL OUTSIDE OF cmdEraseSectors()
 * 
 * @param error 
 * @param sectors array of sectors to erase
 * @param sector_count number of sectors to erase
 * @return esp_err_t 
 */
static esp_err_t cmdTransmitEraseSectors(char *error, uint8_t *sectors, uint8_t sector_count)
{
  ESP_LOGI(TAG_STM_FLASH, "TRANSMIT SECTORS TO ERASE");

  CanMsg_t can_message = {.id = 0x4, .dlc = 8};
  for (uint8_t i = 0; i < sector_count; i++)
  {
    can_message.data.data_u8[i] = sectors[i];
  }

  if (canTx(&can_message))
  {
    return waitForCanACK(0x43, CAN_TIMEOUT);
  }
  else
  {
    return ESP_FAIL;
  }
}

/**
 * @brief Bootloader sector erase command over CAN. The function sends a CAN message to 
 * start the erasing, and then sends the sectors to erase using cmdTransmitEraseSectors()
 * 
 * @param error 
 * @param sectors 
 * @param sector_count 
 * @return esp_err_t 
 */
static esp_err_t cmdEraseSectors(char *error, uint8_t *sectors, uint8_t sector_count)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "ERASE MEMORY SECTORS");
  CanMsg_t can_message = {.id = 0x43, .dlc = 1, .data.data_u8 = {sector_count - 1}};

  if (canTx(&can_message))
  {
    if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
    {
      if (waitForCanACK(can_message.id, CAN_ERASE_TIMEOUT) == ESP_OK)
      {
        if (cmdTransmitEraseSectors(error, sectors, sector_count) != ESP_OK)
        {
          sprintf(error, "ERASE SECTORS CMD FAILED: failed to transmit sectors");
          return ESP_FAIL;
        }

        if (waitForCanACK(can_message.id, CAN_TIMEOUT) == ESP_OK)
        {
          return ESP_OK;
        }
        else
        {
          sprintf(error, "ERASE SECTORS CMD FAILED: did not receive third ACK");
        }
      }
      else
      {
        sprintf(error, "ERASE SECTORS CMD FAILED: did not receive second ACK");
      }
    }
    else
    {
      sprintf(error, "ERASE SECTORS CMD FAILED: did not receive first ACK");
    }
  }
  else
  {
    sprintf(error, "ERASE SECTORS CMD FAILED: transmission failed");
  }
  ESP_LOGE(TAG_STM_FLASH, "%s", error);
  return ESP_FAIL;
}

/**
 * @brief Function to set the STM to allow memory being written
 *
 * @param adrMS address byte 1
 * @param adrMI address byte 2
 * @param adrLI address byte 3
 * @param adrLS address byte 4
 * @return esp_err_t
 */
static esp_err_t cmdWrite(const char adrMS, const char adrMI, const char adrLI, const char adrLS)
{
  CanMsg_t can_message = {.id = 0x31, .dlc = 5, .data.data_u8 = {adrMS, adrMI, adrLI, adrLS, 255}};

  if (canTx(&can_message))
  {
    return waitForCanACK(can_message.id, CAN_TIMEOUT);
  }
  ESP_LOGE(TAG_STM_FLASH, "WRITE CMD FAILED");
  return ESP_FAIL;
}

/**
 * @brief Write data to flash address specified by cmdWrite
 *
 * @param data data to write
 * @param data_size size of data
 * @return esp_err_t
 */
static esp_err_t cmdWriteData(const uint8_t *data, const uint8_t data_size)
{
  CanMsg_t can_message = {.id = 0x04, .dlc = 8};

  for (int i = 0; i < data_size; i++)
  {
    can_message.data.data_u8[i] = data[i];
  }

  for (int j = data_size; j < 8; j++)
  {
    can_message.data.data_u8[j] = 0xFF;
  }

  if (canTx(&can_message))
  {
    return waitForCanACK(0x31, CAN_TIMEOUT);
  }
  ESP_LOGE(TAG_STM_FLASH, "WRITE DATA CMD FAILED");
  return ESP_FAIL;
}

/**
 * @brief Function to set the STM to allow memory being read
 *
 * @param adrMS address byte 1
 * @param adrMI address byte 2
 * @param adrLI address byte 3
 * @param adrLS address byte 4
 * @return esp_err_t
 */
static esp_err_t cmdRead(const char adrMS, const char adrMI, const char adrLI, const char adrLS)
{
  CanMsg_t can_message = {.id = 0x11, .dlc = 5, .data.data_u8 = {adrMS, adrMI, adrLI, adrLS, 255}};

  if (canTx(&can_message))
  {
    return waitForCanACK(can_message.id, CAN_TIMEOUT);
  }
  ESP_LOGE(TAG_STM_FLASH, "READ CMD FAILED");
  return ESP_FAIL;
}

/**
 * @brief Function to load memory address for writing and reading data from STM
 *
 * @param loadAddr address to increment
 */
static void incrementLoadAddress(char *loadAddr)
{

  loadAddr[2] += 0x1;

  if (loadAddr[2] == 0)
  {
    loadAddr[1] += 0x1;

    if (loadAddr[1] == 0)
    {
      loadAddr[0] += 0x1;
    }
  }
}

/**
 * @brief Function to CAN write the flash memory address of the STM32Fxx with blocks of data
 *
 * @param address memory address at which to start
 * @param data data that needs to be flashed to STM
 * @retval ESP_OK (esp_err_t): if the flashing procedure is completed successfully
 * @retval ESP_FAIL (esp_err_t): if the process failed
 */
static esp_err_t flashPage(const char *address, const uint8_t *data)
{

  if (cmdWrite(address[0], address[1], address[2], address[3]) != ESP_OK)
  {
    return ESP_FAIL;
  }

  for (int i = 0; i < 256; i += 8)
  {
    if (cmdWriteData(&data[i], 8) != ESP_OK)
    {
      return ESP_FAIL;
    }
  }

  return waitForCanACK(0x31, CAN_TIMEOUT);
}

/**
 * @brief Function to CAN read the flash memory address of the STM32Fxx and verify with the given block of data
 *
 * @param address memory address at which to start
 * @param data pointer to location in which read bytes should be stored
 * @retval ESP_OK (esp_err_t): if the reading procedure is completed successfully
 * @retval ESP_FAIL (esp_err_t): if the process failed
 */
static esp_err_t readPage(const char *address, const uint8_t *data)
{

  if (cmdRead(address[0], address[1], address[2], address[3]) != ESP_OK)
  {
    return ESP_FAIL;
  }

  CanMsg_t rx_message;
  int byte_count = 0;
  while (byte_count < 256)
  {
    if (bootloaderCanRx(&rx_message, CAN_TIMEOUT) == ESP_OK)
    {
      if (rx_message.id == 0x1F)
      {
        ESP_LOGE(TAG_STM_FLASH, "Received NACK while attempting to read memory");
        return ESP_FAIL;
      }
      else if (rx_message.id == 0x11)
      {
        if (memcmp(&data[byte_count], &rx_message.data.data_u8[0], 8) != 0)
        {
          ESP_LOGE(TAG_STM_FLASH, "Found mismatch between memory and binary image.");
          ESP_LOGE(TAG_STM_FLASH, "Now erasing STM32 memory.");
          return ESP_FAIL;
        }
        byte_count += 8;
      }
      else
      {
        ESP_LOGE(TAG_STM_FLASH, "Received invalid response");
        return ESP_FAIL;
      }
    }
    else
    {
      ESP_LOGE(TAG_STM_FLASH, "Lost communication");
      return ESP_FAIL;
    }
  }

  return waitForCanACK(0x11, CAN_TIMEOUT);
}

/**
 * @brief Write the code into the flash memory of STM32Fxx
 * The data from the .bin file is written into the flash memory
 * of the client, block-by-block
 *
 * @param flash_file File pointer of the .bin file to be flashed
 * @param loadAddress memory address at which to start writing
 * @retval ESP_OK - success, ESP_FAIL - failed
 */
static esp_err_t writeTask(FILE *flash_file, char *loadAddress, char *error)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "Write Task");

  uint8_t block[256] = {0};
  int curr_block = 0, bytes_read = 0;

  fseek(flash_file, 0, SEEK_SET);

  while ((bytes_read = fread(block, 1, 256, flash_file)) > 0)
  {
    curr_block++;

    esp_err_t ret = flashPage(loadAddress, block);
    if (ret == ESP_FAIL)
    {
      sprintf(error, "WRITE CMD FAILED: Failed to write block: %d", curr_block);
      ESP_LOGE(TAG_STM_FLASH, "Failed to write block: %d", curr_block);
      return ESP_FAIL;
    }

    incrementLoadAddress(loadAddress);

    memset(block, 0xff, 256);
  }

  return ESP_OK;
}

/**
 * @brief Read the flash memory of the STM32Fxx, for verification
 * It reads the flash memory of the STM32 block-by-block and
 * checks it with the data from the file (with pointer passed)
 *
 * @param flash_file File pointer of the .bin file to be verified against
 * @param readAddress memory address at which to start reading
 * @retval ESP_OK - success, ESP_FAIL - failed
 */
static esp_err_t readTask(FILE *flash_file, char *readAddress, char *error)
{
  ESP_LOGI(TAG_STM_FLASH, "%s", "Read & Verification Task");

  uint8_t block[257] = {0};
  int curr_block = 0, bytes_read = 0;

  fseek(flash_file, 0, SEEK_SET);

  while ((bytes_read = fread(block, 1, 256, flash_file)) > 0)
  {
    curr_block++;

    esp_err_t ret = readPage(readAddress, block);
    if (ret == ESP_FAIL)
    {
      bootloaderCanClearQ();
      cmdMassErase(error);
      sprintf(error, "READ CMD FAILED: Failed to read or verify block: %d", curr_block);
      ESP_LOGE(TAG_STM_FLASH, "Failed to read block: %d", curr_block);
      return ESP_FAIL;
    }

    incrementLoadAddress(readAddress);

    memset(block, 0xff, 256);
  }

  return ESP_OK;
}

esp_err_t flashSTM(const char *file_name, gpio_num_t reset_pin, char *error)
{
  // Clear bootloader CAN queue before new procedure
  bootloaderCanClearQ();

  esp_err_t err = ESP_FAIL;

  char file_path[FILE_PATH_MAX];
  sprintf(file_path, "%s%s", BASE_PATH, file_name);
  ESP_LOGI(TAG_STM_FLASH, "File name: %s", file_path);

#ifdef RESET_OVER_CAN
  if (cmdReset(APP_RESET) != ESP_OK)
  {
    return ESP_FAIL;
  }
#else  // !RESET_OVER_CAN
  initGPIO(reset_pin);
  resetSTM(reset_pin);
#endif // !RESET_OVER_CAN

  // Check bootloader ID and erase memory
  if ((cmdId(error) != ESP_OK) || (cmdMassErase(error) != ESP_OK))
  {
    return ESP_FAIL;
  }

  FILE *flash_file = fopen(file_path, "rb");
  if (flash_file != NULL)
  {
    // This while loop executes only once and breaks if any of the functions do not return ESP_OK
    do
    {
      char writeAddress[4] = STM_BOOT_ADDRESS;

      ESP_LOGI(TAG_STM_FLASH, "%s", "Writing STM32 Memory");
      IS_ESP_OK(writeTask(flash_file, writeAddress, error));

      char readAddress[4] = STM_BOOT_ADDRESS;

      ESP_LOGI(TAG_STM_FLASH, "%s", "Reading STM32 Memory");
      IS_ESP_OK(readTask(flash_file, readAddress, error));

      err = ESP_OK;
      ESP_LOGI(TAG_STM_FLASH, "%s", "STM32 Flashed Successfully!!!");
    } while (0);
  }

  ESP_LOGI(TAG_STM_FLASH, "%s", "Ending Connection");

#ifdef RESET_OVER_CAN
  cmdReset(BOOTLOADER_RESET);
#else  // !RESET_OVER_CAN
  resetSTM(reset_pin);
  deInitGPIO(reset_pin);
#endif // !RESET_OVER_CAN

  ESP_LOGI(TAG_STM_FLASH, "%s", "Closing file");
  fclose(flash_file);

  return err;
}