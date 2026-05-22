#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "ble_server.h"

static const char* TAG = "SLAVE_MAIN";

// LED GPIO (for status indication)
#define LED_GPIO GPIO_NUM_8

static void led_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

static void on_ble_data(const uint8_t* data, uint16_t len) {
    ESP_LOGI(TAG, "Received %d bytes from master", len);
    
    // Echo back a response
    uint8_t response[] = {0x02}; // Simple response
    ble_server_send_data(response, sizeof(response));
}

void app_main(void) {
    ESP_LOGI(TAG, "OWL Slave starting...");

    led_init();
    ble_server_init();
    ble_server_register_callback(on_ble_data);

    ESP_LOGI(TAG, "OWL Slave ready!");
}
