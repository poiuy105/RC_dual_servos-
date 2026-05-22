#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "ble_client.h"

static const char* TAG = "MASTER_MAIN";

// LED GPIO (for status indication)
#define LED_GPIO GPIO_NUM_8

static void led_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
}

static void send_heartbeat(void* arg) {
    uint8_t heartbeat_data[] = {0x01}; // Simple heartbeat message
    ble_client_send_data(heartbeat_data, sizeof(heartbeat_data));
}

static void on_ble_connect(bool connected) {
    ESP_LOGI(TAG, "BLE %s", connected ? "connected" : "disconnected");
    gpio_set_level(LED_GPIO, connected ? 1 : 0);
}

static void on_ble_data(const uint8_t* data, uint16_t len) {
    ESP_LOGI(TAG, "Received %d bytes from slave", len);
}

void app_main(void) {
    ESP_LOGI(TAG, "OWL Master starting...");

    led_init();
    ble_client_init();
    ble_client_register_connect_callback(on_ble_connect);
    ble_client_register_callback(on_ble_data);

    // Send heartbeat every second
    esp_timer_create_args_t timer_args = {
        .callback = &send_heartbeat,
        .arg = NULL,
        .name = "heartbeat"
    };

    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, 1000000); // 1 second

    ESP_LOGI(TAG, "OWL Master ready!");
}
