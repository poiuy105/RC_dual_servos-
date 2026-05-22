#ifndef BLE_CLIENT_H
#define BLE_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#define DEVICE_NAME         "OWL_MASTER"
#define SLAVE_DEVICE_NAME   "OWL_SLAVE"
#define GATT_SERVICE_UUID   0xFFFF
#define GATT_CHAR_TX_UUID   0xFF01
#define GATT_CHAR_RX_UUID   0xFF02

typedef enum {
    BLE_STATE_IDLE,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_DISCONNECTED
} ble_state_t;

void ble_client_init(void);
bool ble_client_is_connected(void);
int ble_client_send_data(const uint8_t* data, uint16_t len);
void ble_client_register_callback(void (*on_data)(const uint8_t*, uint16_t));
void ble_client_register_connect_callback(void (*on_connect)(bool));

#endif
