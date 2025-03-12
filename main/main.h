#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <esp_log.h>
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include <stdint.h>
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include <ctype.h>
#include <stdbool.h>

#define LED_GPIO               GPIO_NUM_2
#define DOOR1_GPIO             GPIO_NUM_17 
#define DOOR2_GPIO             GPIO_NUM_16 
#define UART_PORT_NUM          UART_NUM_0

bool using_filesystem = false;
bool master_actions = false;

static const char *TAG                            = "UNI-FM";
static const char *UART_REPLACE_SECOND_UID_PREFIX = ":"; // A prefixum which determines when the second uid comes e.g. replace uid1:uid2
static const char *AUTH_FILE                      = "/spiffs/cards.txt"; // File, where uids are stored
static const char *TEMP_FILE                      = "/spiffs/temp.txt";  // Temporary file path

static const int RC522_SPI_BUS_GPIO_MISO          = 19;
static const int RC522_SPI_BUS_GPIO_MOSI          = 23;
static const int RC522_SPI_BUS_GPIO_SCLK          = 18;
static const int RC522_SPI_SCANNER_GPIO_SDA       = 05; // SPI_SS
static const int RC522_SCANNER_GPIO_RST           = 22; // Reset
static const int UART_TICKS_TO_WAIT               = 100;
static const int UART_BUFFER_SIZE                 = 1024;


static rc522_driver_handle_t driver;
static rc522_handle_t scanner;
TaskHandle_t myTaskHandle = NULL;
QueueHandle_t queue;

static rc522_spi_config_t driver_config = {
    .host_id = SPI3_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
    },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};