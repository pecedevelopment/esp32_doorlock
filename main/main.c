
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

#define RC522_SPI_BUS_GPIO_MISO    (19) 
#define RC522_SPI_BUS_GPIO_MOSI    (23)
#define RC522_SPI_BUS_GPIO_SCLK    (18)
#define RC522_SPI_SCANNER_GPIO_SDA (05) // VSPI_SS
#define RC522_SCANNER_GPIO_RST     (22) // soft-reset
#define LED_GPIO                   GPIO_NUM_2
#define UART_BAUDRATE              115200
#define UART_PORT_NUM              UART_NUM_0
#define BUF_SIZE                   (1024)
#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)



static const char *TAG = "UNI-FM";

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

static rc522_driver_handle_t driver;
static rc522_handle_t scanner;
TaskHandle_t myTaskHandle1 = NULL;
TaskHandle_t myTaskHandle2 = NULL;
QueueHandle_t queue;
char txBuffer[50];
int uart_using_filesystem = 0;


void blink_led(int num){
    esp_rom_gpio_pad_select_gpio(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    if(num==0){                                 //led turned on for 5 secs
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(5000 / portTICK_PERIOD_MS); // 5 sec delay
        gpio_set_level(LED_GPIO, 0);
    }
    else{
        for(int i = 0; i<9; i++){             // blinking for 5 secs
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(250 / portTICK_PERIOD_MS); // 0.25 sec delay
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(250 / portTICK_PERIOD_MS); // 0.25 sec delay
        }
    }
}
void delete_line(const char *filename, int line_to_delete) {
    FILE *file, *temp;
    char buffer[1024];
    int current_line = 1;

    // Open the original file for reading
    file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Error opening file.\n");
        return;
    }

    // Open a temporary file for writing
    temp = fopen("/spiffs/temp.txt", "w");
    if (temp == NULL) {
        ESP_LOGE(TAG,"Error creating temporary file.\n");
        fclose(file);
        return;
    }

    // Read and copy all lines except the one to delete
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (current_line != line_to_delete) {
            fputs(buffer, temp);
        }
        current_line++;
    }

    // Close both files
    fclose(file);
    fclose(temp);

    // Remove the original file and rename the temporary file
    remove(filename);
    rename("/spiffs/temp.txt", filename);
}


int master_actions = 0;

int find_uid(char * uid_str, int * line_counter){
    FILE* f = fopen("/spiffs/cards.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -1;
    }
    *line_counter = 1;
    char line[12];
    while(fgets(line, sizeof(line), f)!=NULL){
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if(strcmp(line, uid_str)==0){
            *line_counter = ((*line_counter)/2) + (*line_counter)%2;
            fclose(f);
            return 1;         
        }
        (*line_counter)++;
    }
    fclose(f);
    return 0;
}

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
        rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str));
        rc522_picc_print(picc);
        while(uart_using_filesystem==1){
            ESP_LOGI(TAG, "Waiting for uart_using_filesystem =0 at on_picc_state_changed");
            vTaskDelay(1000/ portTICK_PERIOD_MS);
        }
        uart_using_filesystem = 1;
        // searching for uid in the txt
        int uid_line = -1;
        int succ = find_uid(uid_str, &uid_line);
        // idiot proof mechanism, you can't delete the master key
        if(uid_line==1){
            ESP_LOGE(TAG, "MASTER KEY SCANNED");
            master_actions = 1;
            uart_using_filesystem = 0;
            return;
        }
        if(succ==0){
            // adding card's uid to /spiffs/cards.txt
            if(master_actions){
                FILE* f = fopen("/spiffs/cards.txt", "a");
                if (f == NULL) {
                    ESP_LOGE(TAG, "Failed to open file for appending");
                    return;
                }
                fprintf(f,"\n%s",uid_str);
                fclose(f);
                ESP_LOGE(TAG, "Card added");
                master_actions = 0;
                uart_using_filesystem = 0;
                return;
            }else{
                ESP_LOGE(TAG, "Card declined: %s", uid_str);
                blink_led(0);
            }
        }
        else{
            succ = 0;
            if(master_actions){
                // removing card's uid from the list
                delete_line("/spiffs/cards.txt", uid_line);
                ESP_LOGE(TAG, "Card deleted");
                master_actions = 0;
                uart_using_filesystem = 0;
                return;
            }else{
                ESP_LOGI(TAG, "Card authenticated: %s", uid_str);
                sprintf(txBuffer, uid_str);
                xQueueSend(queue, (void*)txBuffer, (TickType_t)0);
                blink_led(1);
                /*while (1) {
                    vTaskDelay(3000/ portTICK_PERIOD_MS);
                }*/
            }
        }
        uart_using_filesystem = 0;
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed from the reader");
    }
}

void parseCommand(char *input, char *cmd, char *uid1, char *uid2) {
    char *token = strtok(input, " "); // First token (command)
    if (token) strcpy(cmd, token);

    char *uid_old = strtok(NULL, ":"); // Get the first UID (until ':')
    if (uid_old) strcpy(uid1, uid_old);

    char *uid_new = strtok(NULL, ""); // Get the second UID (rest of the string)
    if (uid_new) strcpy(uid2, uid_new);
}
void trim_leading(char *str) {
    char *start = str;

    // Move `start` to the first non-space character
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // Move the trimmed content to the original buffer
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void uart_comm(void *){

   // char rxBuffer[50];
    uint8_t data[128];
    while(1){
    /* if( xQueueReceive(queue, &(rxBuffer), (TickType_t)5))
     {*/
      /*printf("Received data from queue == %s\n", rxBuffer);
      rxBuffer[49]='\n';
      uart_write_bytes_with_break(UART_PORT_NUM, rxBuffer,50, 100);*/
      int len = uart_read_bytes(UART_PORT_NUM, data, 127, 100); //100 hány tickenként olvasson
      if(len){
        data[len] = '\0';
        ESP_LOGI("UART RECEIVER", "received str: %s", (char*) data);
        while(uart_using_filesystem==1){
            ESP_LOGI(TAG, "Waiting for uart_using_filesystem =0 at uart_comm");
            vTaskDelay(1000/ portTICK_PERIOD_MS);
        }

            uart_using_filesystem =1;

            char input[128];
            strcpy(input, (char*)data);
           /* char command[8] = {0};
            char uid1[12] ={0};
            char uid2[12]={0};*/
            char command[20], uid1[50], uid2[50];
            parseCommand(input, command, uid1, uid2);
            trim_leading(uid1);
            trim_leading(uid2);
            ESP_LOGI("uid2", "received str:%s", uid2);
            
            if(strcmp("list", command)==0){
                
                FILE* f = fopen("/spiffs/cards.txt", "r");
                if (f == NULL) {
                    ESP_LOGE(TAG, "Failed to open file for reading /spiffs/cards.txt");
                    return;
                }
                char line[12];
                while (fgets(line, sizeof(line), f) != NULL) {
                    size_t len = strlen(line);
                    
                    // Remove the trailing newline, if any
                    if (len > 0 && line[len - 1] == '\n') {
                        line[len - 1] = '\0';
                        len--; // Adjust length after removing the newline
                    }
                
                    // Trim leading spaces
                    char *start = line;
                    while (*start && isspace((unsigned char)*start)) {
                        start++;
                    }
                
                    // Trim trailing spaces
                    char *end = start + strlen(start) - 1;
                    while (end >= start && isspace((unsigned char)*end)) {
                        end--;
                    }
                    
                    // Null-terminate the trimmed line
                    *(end + 1) = '\0';
                
                    // If the line is empty after trimming, skip it
                    if (*start == '\0') {
                        continue; // Skip empty or whitespace-only lines
                    }
                
                    // Log the non-empty line, along with its length for debugging
                    ESP_LOGI(TAG, "Line: \"%s\", Length: %d", start, strlen(start));
                }
                fclose(f);
            }
            else if(strcmp("replace", command)==0){
                FILE *file = fopen("/spiffs/cards.txt", "r");
                FILE *temp = fopen("/spiffs/temp.txt", "w");

                if (!file) {
                    printf("Error opening file. /spiffs/cards.txt\n");
                    return;
                }
                if (!temp) {
                    printf("Error opening file. /spiffs/temp.txt\n");
                    return;
                }

                char buffer[1024];
                int replaced = 0;

                while (fgets(buffer, sizeof(buffer), file)) {
                    // Remove newline characters for exact comparison
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strcmp(buffer, uid1) == 0 && !replaced) {
                        fprintf(temp, "%s\n", uid2); // Write the replacement line
                        replaced = 1;
                    } else {
                        fprintf(temp, "%s\n", buffer); // Write the original line
                    }
                }

                fclose(file);
                fclose(temp);

                // Replace the original file with the updated one
                remove("/spiffs/cards.txt");
                rename("/spiffs/temp.txt", "/spiffs/cards.txt");

                if (replaced) {
                    printf("Line replaced successfully.\n");
                } else {
                    printf("Line not found.\n");
                }
            }
            else if(strcmp("remove", command)){

            } 
            uart_using_filesystem = 0;
        
      }
      vTaskDelay(1000/ portTICK_PERIOD_MS);

     //}
    }

}
void rc522(void *){
    
    while(1){
        rc522_spi_create(&driver_config, &driver);
        rc522_driver_install(driver);
    
        rc522_config_t scanner_config = {
            .driver = driver,
        };
    
        rc522_create(&scanner_config, &scanner);
        rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
        rc522_start(scanner);
        vTaskDelay(1000/ portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
     /* Configure parameters of an UART driver,
     * communication pins and install the driver */

     uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    //const int uart_buffer_size = (1024 * 2);
    //QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
   // ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));

    queue = xQueueCreate(5, sizeof(txBuffer)); 
    if (queue == 0)
    {
    printf("Failed to create queue= %p\n", queue);
    }
   
    ESP_LOGI("spiffs", "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("spiffs", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("spiffs", "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE("spiffs", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

#ifdef CONFIG_EXAMPLE_SPIFFS_CHECK_ON_START
    ESP_LOGI("spiffs", "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE("spiffs", "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return;
    } else {
        ESP_LOGI("spiffs", "SPIFFS_check() successful");
    }
#endif

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("spiffs", "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI("spiffs", "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW("spiffs", "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE("spiffs", "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGI("spiffs", "SPIFFS_check() successful");
        }
    }

    // All done, unmount partition and disable SPIFFS
    //esp_vfs_spiffs_unregister(conf.partition_label);
    //ESP_LOGI(TAG, "SPIFFS unmounted");

  //  xTaskCreatePinnedToCore(rc522, "rc522", CONFIG_MAIN_TASK_STACK_SIZE, NULL ,9, &myTaskHandle1, 1);
  xTaskCreatePinnedToCore(uart_comm, "uart_comm", CONFIG_MAIN_TASK_STACK_SIZE, NULL ,10, &myTaskHandle2, 0);
    rc522_spi_create(&driver_config, &driver);
    rc522_driver_install(driver);

    rc522_config_t scanner_config = {
        .driver = driver,
    };

    rc522_create(&scanner_config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    rc522_start(scanner);
    
    //setup_uart(void);
   

}
