
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

#define RC522_SPI_BUS_GPIO_MISO    (19) 
#define RC522_SPI_BUS_GPIO_MOSI    (23)
#define RC522_SPI_BUS_GPIO_SCLK    (18)
#define RC522_SPI_SCANNER_GPIO_SDA (05) // VSPI_SS
#define RC522_SCANNER_GPIO_RST     (22) // soft-reset
#define LED_GPIO GPIO_NUM_2

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

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        int succ =0;
        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
         rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str));
        //RC522_RETURN_ON_ERROR(rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)));
        rc522_picc_print(picc);

        FILE* f = fopen("/spiffs/cards.txt", "r");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return;
        }
        char line[12];
        int line_counter = 1;

       while(fgets(line, sizeof(line), f)!=NULL){
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }

            if(strcmp(line, uid_str)==0){
                succ =1;
                break;           
            }
            line_counter++;
        }
        line_counter = (line_counter/2) + line_counter%2;
        fclose(f);
        if(line_counter==1){
            ESP_LOGE(TAG, "MASTER KEY SCANNED");
            master_actions = 1;
            return;
        }
        if(succ==0){
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
                return;


            }else{
                ESP_LOGE(TAG, "Card declined");
                blink_led(0);
            }
        }
        else{
            succ = 0;
            if(master_actions){
                delete_line("/spiffs/cards.txt", line_counter);
                ESP_LOGE(TAG, "Card deleted");
                master_actions = 0;
                return;
            }else{
                ESP_LOGI(TAG, "Card found: %s", line);
                blink_led(1);
            }
        }
        // strip newline
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed");
    }
}

void app_main(void)
{
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

    rc522_spi_create(&driver_config, &driver);
    rc522_driver_install(driver);

    rc522_config_t scanner_config = {
        .driver = driver,
    };

    rc522_create(&scanner_config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    rc522_start(scanner);
}
