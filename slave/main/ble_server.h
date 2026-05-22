#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#define DEVICE_NAME         "OWL_SLAVE"
#define GATT_SERVICE_UUID   0xFFFF
#define GATT_CHAR_TX_UUID   0xFF01
#define GATT_CHAR_RX_UUID   0xFF02

typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED
} ble_state_t;

void ble_server_init(void);
bool ble_server_is_connected(void);
int ble_server_send_data(const uint8_t* data, uint16_t len);
void ble_server_register_callback(void (*on_data)(const uint8_t*, uint16_t));

#endif
