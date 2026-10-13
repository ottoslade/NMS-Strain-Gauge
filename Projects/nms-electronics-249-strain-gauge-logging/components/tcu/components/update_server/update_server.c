#include "update_server.h"
#include "can.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_wifi.h"
#include "stm_flash.h"
#include "util.h"
#include <stdio.h>

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE     (200 * 1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192
#define CHUNK_SIZE      1024

/* Macro to check file extension */
#define IS_FILE_EXT(filename, ext)                                                                 \
  (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/**
 * @brief structure for data used by file server
 * 
 */
struct file_server_data
{
  /* Base path of file storage */
  char base_path[ESP_VFS_PATH_MAX + 1];

  /* Scratch buffer for temporary storage during file transfer */
  char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "FILE_SERVER_HTTP";
static httpd_handle_t file_server = NULL;
extern SemaphoreHandle_t reset_esp;

/**
 * @brief Handler to respond with an icon file embedded in flash.
 * 
 * @param req HTTP GET request field
 * @retval ESP_OK -> after the embedded encoding of the icon is sent as HTTP response ot the client
 * @retval ESP_FAIL -> process fails
 */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
  extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
  extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
  const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
  return ESP_OK;
}

/**
 * @brief HTTP GET request handler to serve upload_script.html to the client
 * 
 * @param req HTTP request
 * @return esp_err_t -> ESP error status to determine success of operation
 */
static esp_err_t upload_script_get_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "Serving upload script to client...");
  /* Get handle to embedded file upload script */
  extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
  extern const unsigned char upload_script_end[] asm("_binary_upload_script_html_end");
  const size_t upload_script_size = (upload_script_end - upload_script_start);

  /* Add file upload form and script which on execution sends a POST request to /upload */
  httpd_resp_send(req, (const char *)upload_script_start, upload_script_size);
  return ESP_OK;
}

/**
 * @brief Function to copy the full path of the file into the destination buffer
 * 
 * @param dest destination buffer
 * @param base_path buffer containing the base path of the file
 * @param uri buffer containing the full URI to extract the path
 * @param destsize size of the destination buffer
 * @return  dest + base_pathlen -> char pointer to path skipping the base length
 */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri,
                                     size_t destsize)
{
  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest)
  {
    pathlen = MIN(pathlen, quest - uri);
  }
  const char *hash = strchr(uri, '#');
  if (hash)
  {
    pathlen = MIN(pathlen, hash - uri);
  }

  if (base_pathlen + pathlen + 1 > destsize)
  {
    /* Full path string won't fit into destination buffer */
    return NULL;
  }

  /* Construct full path (base + path) */
  strcpy(dest, base_path);
  strlcpy(dest + base_pathlen, uri, pathlen + 1);

  /* Return pointer to path, skipping the base */
  return dest + base_pathlen;
}

/**
 * @brief Function to upload a file received from a client to SPIFFS
 * 
 * @param req HTTP request handler
 * @param filename name of the file to upload
 * @param filepath path of the file to upload
 * @return esp_err_t 
 */
static esp_err_t upload_file(httpd_req_t *req, const char *filename, const char *filepath)
{
  FILE *fd = NULL;
  struct stat file_stat;

  if (!filename)
  {
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Filename too long");
    return ESP_FAIL;
  }

  /* Filename cannot have a trailing '/' */
  if (filename[strlen(filename) - 1] == '/' ||
      (strcmp(&filename[strlen(filename) - 4], ".bin") != 0))
  {
    ESP_LOGE(TAG, "Invalid filename : %s", filename);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
    return ESP_FAIL;
  }

  if (stat(filepath, &file_stat) == 0)
  {
    ESP_LOGE(TAG, "File already exists : %s", filepath);
    /* Respond with 400 Bad Request */
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
    return ESP_FAIL;
  }

  /* File cannot be larger than a limit */
  if (req->content_len > MAX_FILE_SIZE)
  {
    ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
    /* Respond with 400 Bad Request */
    httpd_resp_send_err(req,
                        HTTPD_400_BAD_REQUEST,
                        "File size must be less than " MAX_FILE_SIZE_STR "!");
    /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
    return ESP_FAIL;
  }

  fd = fopen(filepath, "w");
  if (!fd)
  {
    ESP_LOGE(TAG, "Failed to create file : %s", filepath);
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to create file");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Receiving file : %s...", filename);

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
  int received;

  /* Content length of the request gives
     * the size of the file being uploaded */
  int remaining = req->content_len;

  while (remaining > 0)
  {

    ESP_LOGI(TAG, "Remaining size : %d", remaining);
    /* Receive the file part by part into a buffer */
    if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
    {
      if (received == HTTPD_SOCK_ERR_TIMEOUT)
      {
        /* Retry if timeout occurred */
        continue;
      }

      /* In case of unrecoverable error,
             * close and flash the unfinished file*/
      fclose(fd);
      unlink(filepath);

      ESP_LOGE(TAG, "File reception failed!");
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive file");
      return ESP_FAIL;
    }

    /* Write buffer content to file on storage */
    if (received && (received != fwrite(buf, 1, received, fd)))
    {
      /* Couldn't write everything to file!
             * Storage may be full? */
      fclose(fd);
      unlink(filepath);

      ESP_LOGE(TAG, "File write failed!");
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to write file to storage");
      return ESP_FAIL;
    }

    /* Keep track of remaining size of
         * the file left to be uploaded */
    remaining -= received;
  }

  /* Close file upon upload completion */
  fclose(fd);
  ESP_LOGI(TAG, "File reception complete");

  return ESP_OK;
}

/**
 * @brief Function to flash the ESP throught a file received from the HTTP client
 * 
 * @param req HTTP request
 * @return esp_err_t 
 */
static esp_err_t flashESP(httpd_req_t *req, char *error)
{
  ESP_LOGI(TAG, "Flashing ESP...");

  // Get next available OTA partition
  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  if (!ota_partition)
  {
    ESP_LOGE(TAG, "No available OTA partition found");
    sprintf(error, "ESP OTA: no available OTA partition found");
    return ESP_FAIL;
  }

  // Start OTA update process
  esp_ota_handle_t ota_handle;
  esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
    sprintf(error, "ESP OTA: esp_ota_begin failed (%s)", esp_err_to_name(err));
    return ESP_FAIL;
  }

  uint8_t buffer[CHUNK_SIZE];
  size_t read_bytes;
  int remaining = req->content_len;
  while (remaining > 0)
  {
    // Receive chunk of .bin file from HTTP client
    if ((read_bytes = httpd_req_recv(req, (char *)buffer, MIN(remaining, CHUNK_SIZE))) <= 0)
    {
      ESP_LOGE(TAG, "File reception failed!");
      sprintf(error, "ESP OTA: failed to receive file");
      esp_ota_end(ota_handle);
      return ESP_FAIL;
    }

    // Write the chunk received from client into available OTA partition
    err = esp_ota_write(ota_handle, buffer, read_bytes);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
      sprintf(error, "ESP OTA: esp_ota_write failed (%s)", esp_err_to_name(err));
      esp_ota_end(ota_handle);
      return ESP_FAIL;
    }

    /* Keep track of remaining size of
         * the file left to be uploaded */
    remaining -= read_bytes;
  }

  ESP_LOGI(TAG, "Finished flashing ESP32, now updating partition data");

  // Finish OTA procedure
  err = esp_ota_end(ota_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
    sprintf(error, "ESP OTA: esp_ota_end failed (%s)", esp_err_to_name(err));
    return ESP_FAIL;
  }

  // Set the OTA partition flashed as the partition the ESP should boot into at the next reboot
  err = esp_ota_set_boot_partition(ota_partition);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
    sprintf(error, "ESP OTA: esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
    return ESP_FAIL;
  }

  ESP_LOGE(TAG, "ESP flashed successfully!");

  return ESP_OK;
}

/**
 * @brief Handler to flash a file from the server
 * 
 * @param req HTTP POST request field
 * @retval ESP_OK -> file is downloaded successfully, HTTP response is also sent to the client
 * @retval ESP_FAIL -> process fails
 */
static esp_err_t flash_post_handler(httpd_req_t *req)
{
  ESP_LOGE(TAG, "Flashing a board");

  char filepath[FILE_PATH_MAX];

  /* Store the character for flashing to rear vs front */
  char component = (req->uri + sizeof("/flash/") - 1)[0];

  /* Skip leading "/flash" and "/R" (flash to rear) or "/F" (flash to front) or "/E" (flash to ESP) from URI to get filename */
  /* Note sizeof() counts NULL termination hence the -1 */
  /* component is 'R', 'F", or 'E' to denote if this is the Rear STM, Front STM, or ESP to be flashed */
  const char *filename = get_path_from_uri(filepath,
                                           ((struct file_server_data *)req->user_ctx)->base_path,
                                           req->uri + sizeof("/flash/-") - 1,
                                           sizeof(filepath));

  filename++;
  bool flash_failed = false;
  char error[128];
  // Flash ESP
  if (component == 'E')
  {
    if (flashESP(req, error) != ESP_OK)
    {
      ESP_LOGE(TAG, "%s", "ESP target flashing failed");
      flash_failed = true;
    }
  }
  // Flash Rear STM
  else if (component == 'R')
  {
    if (upload_file(req, filename, filepath) != ESP_OK)
    {
      return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FILENAME: %s, FILEPATH: %s", filename, filepath);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (flashSTM(filename, RESET_PIN_REARSTM, error) != ESP_OK)
    {
      ESP_LOGE(TAG, "%s", "Rear target flashing failed");
      flash_failed = true;
    }
  }
  // Flash Front STM
  else if (component == 'F')
  {
    if (upload_file(req, filename, filepath) != ESP_OK)
    {
      return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FILENAME: %s, FILEPATH: %s", filename, filepath);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (flashSTM(filename, RESET_PIN_FRONTSTM, error) != ESP_OK)
    {
      ESP_LOGE(TAG, "%s", "Front target flashing failed");
      flash_failed = true;
    }
  }
  else
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URL");
    return ESP_FAIL;
  }

  // Remove flashed image from file system
  int ret = unlink(filepath);
  ESP_LOGI(TAG, "Unliked %s, with error: %d", filepath, ret);
  char response[256];

  if (flash_failed)
  {
    httpd_resp_set_status(req, HTTPD_500);
    sprintf(response, "%s failed to flash (%s)", filename, error);
    httpd_resp_sendstr(req, response);
  }
  else
  {
    httpd_resp_set_status(req, HTTPD_200);
    sprintf(response, "%s flashed successfully", filename);
    httpd_resp_sendstr(req, response);

    if (component == 'E')
    {
      // Signal an ESP reset to boot into the updated flash memory partition
      xSemaphoreGive(reset_esp);
    }
  }

  return ESP_OK;
}

/**
 * @brief Function to initialize SPIFFS memory in the ESP
 * This function prints to serial if SPIFFS was initialized successfully
 * 
 */
static void initSPIFFS(void)
{
  ESP_LOGI(TAG, "%s", "Initializing SPIFFS");
  /* Set spiffs configuration */
  esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                .partition_label = NULL,
                                .max_files = 5,
                                .format_if_mount_failed = true};

  // formatting spiffs to remove all files in case something bad happened
  esp_spiffs_format(NULL);

  /* Initialize the SPIFFS memory with the configuration above */
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(TAG, "%s", "Failed to mount or format filesystem");
    }
    else if (ret == ESP_ERR_NOT_FOUND)
    {
      ESP_LOGE(TAG, "%s", "Failed to find SPIFFS partition");
    }
    else
    {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  /* Display information of the SPIFFS partition */
  size_t total, used;
  if (esp_spiffs_info(NULL, &total, &used) == ESP_OK)
  {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }
}

/**
 * @brief Funcion to start the update HTTP server
 * 
 * @param base_path path of the initial directory of the file server, currently only spiffs base path is supported
 * @retval ESP_OK -> HTTP server is started successfully 
 * @retval ESP_FAIL -> HTTP server fails to start
 */
static esp_err_t start_update_server(const char *base_path)
{
  static struct file_server_data *server_data = NULL;

  /* Validate file storage base path */
  if (!base_path || strcmp(base_path, "/spiffs") != 0)
  {
    ESP_LOGE(TAG, "File server presently supports only '/spiffs' as base path");
    return ESP_ERR_INVALID_ARG;
  }

  if (file_server)
  {
    ESP_LOGE(TAG, "File server already started");
    return ESP_ERR_INVALID_STATE;
  }

  /* Allocate memory for server data */
  server_data = calloc(1, sizeof(struct file_server_data));
  if (!server_data)
  {
    ESP_LOGE(TAG, "Failed to allocate memory for server data");
    return ESP_ERR_NO_MEM;
  }
  strlcpy(server_data->base_path, base_path, sizeof(server_data->base_path));

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 10240;
  config.max_open_sockets = 1;
  config.backlog_conn = 1;
  config.lru_purge_enable = true;

  /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(TAG, "Starting HTTP Server");
  if (httpd_start(&file_server, &config) != ESP_OK)
  {
    ESP_LOGE(TAG, "%s", "Failed to start file server!");
    return ESP_FAIL;
  }

  /* URI handler for getting uploaded files */
  httpd_uri_t file_download = {
      .uri = "/", // Match all URIs of type /path/to/file
      .method = HTTP_GET,
      .handler = upload_script_get_handler,
      .user_ctx = server_data // Pass server data as context
  };
  httpd_register_uri_handler(file_server, &file_download);

  /* URI handler for getting uploaded files */
  httpd_uri_t favicon = {.uri = "/favicon.ico",
                         .method = HTTP_GET,
                         .handler = favicon_get_handler,
                         .user_ctx = NULL};
  httpd_register_uri_handler(file_server, &favicon);

  /* URI handler for flashing files from server */
  httpd_uri_t file_flash = {
      .uri = "/flash/*", // Match all URIs of type /flash/path/to/file
      .method = HTTP_POST,
      .handler = flash_post_handler,
      .user_ctx = server_data // Pass server data as context
  };
  httpd_register_uri_handler(file_server, &file_flash);

  return ESP_OK;
}

/**
 * @brief Function to stop the update HTTP server
 * 
 * @return esp_err_t 
 * @retval ESP_OK -> HTTP server is stopped successfully 
 * @retval ESP_FAIL -> HTTP server fails to stop
 */
static esp_err_t stop_update_server(void)
{
  ESP_LOGI(TAG, "Stopping HTTP Server");
  if (file_server != NULL)
  {
    esp_err_t ret = httpd_stop(file_server);
    file_server = NULL;
    return ret;
  }

  return ESP_FAIL;
}

static void manageUpdatesTask(void *args)
{
  uint8_t inverter_enable = 0;
  bool server_started = false;

  for (;;)
  {
    taskENTER_CRITICAL();
    decode_can_0x0c0_Inverter_Enable(canBuses[VEHICLE_CAN].converter, &inverter_enable);
    taskEXIT_CRITICAL();

    // if inverter is not enabled then allow short range wifi and updates
    if (inverter_enable == 0 && !server_started)
    {
      ESP_ERROR_CHECK(
          esp_wifi_set_protocol(ESP_IF_WIFI_AP,
                                WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
      start_update_server("/spiffs");
      server_started = true;
    }

    // if inverter is enable stop update server and enable long range wifi
    if (inverter_enable == 1 && server_started)
    {
      stop_update_server();
      ESP_ERROR_CHECK(esp_wifi_set_protocol(ESP_IF_WIFI_AP, WIFI_PROTOCOL_LR));
      server_started = false;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void updatesInit(void)
{
  // Initialize file storage
  initSPIFFS();

  xTaskCreate(manageUpdatesTask, "manage_updates", 4096, NULL, PRIORITY_MEDIUM, NULL);
}
