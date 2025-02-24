/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)

TaskHandle_t myTaskHandle = NULL;
TaskHandle_t myTaskHandle2 = NULL;
QueueHandle_t queue;


#define BUF_SIZE (1024)

 void echo_task(void * arg)
{

    char txBuffer[50];
    queue = xQueueCreate(5, sizeof(txBuffer)); 
    if (queue == 0)
    {
     printf("Failed to create queue= %p\n", queue);
    }

    


    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    //char * msg = (char *) arg;

   // ESP_LOGI("TAG", "%s", msg);
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

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    // Install UART driver using an event queue here
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, uart_buffer_size, \
                                            uart_buffer_size, 10, &uart_queue, 0));

    sprintf(txBuffer, "Hello from Demo_Task 1");
    xQueueSend(queue, (void*)txBuffer, (TickType_t)0);

    // Write data to UART.
    
   // while (1) {
        //char* test_str = "This is a test string.\n";
        //uart_write_bytes(UART_NUM_0, (const char*)test_str, strlen(test_str));
      //  uart_write_bytes_with_break(UART_NUM_0, "test break\n",strlen("test break\n"), 100);
      //  vTaskDelete(NULL);
    //}
}

void send_to_uart(void *arg){
    char rxBuffer[50];
    while(1){
     if( xQueueReceive(queue, &(rxBuffer), (TickType_t)5))
     {
      printf("Received data from queue == %s\n", rxBuffer);
      uart_write_bytes_with_break(UART_NUM_0, rxBuffer,50, 100);
      vTaskDelay(1000/ portTICK_PERIOD_MS);

     }
    }
}

//char msg[12] ="test message";

void app_main(void)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, &myTaskHandle);
    xTaskCreatePinnedToCore(send_to_uart, "send_to_uart", ECHO_TASK_STACK_SIZE, NULL ,10, &myTaskHandle2, 1);
}


