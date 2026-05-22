#include "ble_client.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"

static const char* TAG = "BLE_CLIENT";

static bool connected = false;
static bool get_server = false;
static uint16_t slave_conn_id = 0;
static uint16_t slave_gattc_if = 0;
static uint16_t char_handle_tx = 0;
static uint16_t char_handle_rx = 0;
static esp_bd_addr_t slave_bda;

static void (*data_callback)(const uint8_t*, uint16_t) = NULL;
static void (*connect_callback)(bool) = NULL;

static esp_bt_uuid_t service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = GATT_SERVICE_UUID,
};

static esp_ble_scan_params_t scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    uint8_t* adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(30);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t* scan_result = (esp_ble_gap_cb_param_t*)param;
        if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            adv_name = esp_ble_resolve_adv_data_by_type(
                scan_result->scan_rst.ble_adv,
                scan_result->scan_rst.adv_data_len + scan_result->scan_rst.scan_rsp_len,
                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            if (adv_name != NULL && strncmp((char*)adv_name, SLAVE_DEVICE_NAME, adv_name_len) == 0) {
                if (!connected) {
                    connected = true;
                    memcpy(slave_bda, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                    esp_ble_gap_stop_scanning();
                    esp_ble_gattc_open(slave_gattc_if, slave_bda, BLE_ADDR_TYPE_PUBLIC, true);
                }
            }
        }
        break;
    }
    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param) {
    switch (event) {
    case ESP_GATTC_REG_EVT:
        slave_gattc_if = gattc_if;
        esp_ble_gap_set_scan_params(&scan_params);
        break;

    case ESP_GATTC_CONNECT_EVT:
        slave_conn_id = param->connect.conn_id;
        esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
        if (connect_callback) connect_callback(true);
        break;

    case ESP_GATTC_DISCONNECT_EVT:
        connected = false;
        get_server = false;
        if (connect_callback) connect_callback(false);
        esp_ble_gap_start_scanning(30);
        break;

    case ESP_GATTC_CFG_MTU_EVT:
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &service_uuid);
        break;

    case ESP_GATTC_SEARCH_RES_EVT:
        if (param->search_res.srvc_id.uuid.uuid.uuid16 == GATT_SERVICE_UUID) {
            get_server = true;
        }
        break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (get_server) {
            esp_ble_gattc_get_char_by_uuid(gattc_if, slave_conn_id, 0xFFFF, 0xFFFF, service_uuid, NULL);
        }
        break;

    case ESP_GATTC_NOTIFY_EVT:
        if (data_callback) {
            data_callback(param->notify.value, param->notify.value_len);
        }
        break;

    case ESP_GATTC_GET_CHAR_EVT:
        if (param->get_char.status == ESP_GATT_OK) {
            if (param->get_char.char_uuid.uuid.uuid16 == GATT_CHAR_TX_UUID) {
                char_handle_tx = param->get_char.char_handle;
                ESP_LOGI(TAG, "Found TX characteristic, handle: %d", char_handle_tx);
            } else if (param->get_char.char_uuid.uuid.uuid16 == GATT_CHAR_RX_UUID) {
                char_handle_rx = param->get_char.char_handle;
                ESP_LOGI(TAG, "Found RX characteristic, handle: %d", char_handle_rx);
                
                // Enable notification for RX characteristic
                esp_ble_gattc_register_for_notify(gattc_if, slave_bda, param->get_char.char_handle);
            }
        }
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
        ESP_LOGI(TAG, "Register for notify status: %d", param->reg_for_notify.status);
        if (param->reg_for_notify.status == ESP_GATT_OK) {
            uint16_t notify_en = 1;
            esp_ble_gattc_write_char_descr(gattc_if, slave_conn_id, 
                                          param->reg_for_notify.handle + 1,  // CCCD handle is next after char handle
                                          sizeof(notify_en), (uint8_t*)&notify_en,
                                          ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
            ESP_LOGI(TAG, "Notification enabled");
        }
        break;

    default:
        break;
    }
}

void ble_client_init(void) {
    ESP_LOGI(TAG, "Initializing BLE client...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));
    // esp_ble_gatt_set_local_mtu(500); // Commented out for compatibility
}

bool ble_client_is_connected(void) {
    return connected && get_server;
}

int ble_client_send_data(const uint8_t* data, uint16_t len) {
    if (!ble_client_is_connected()) return -1;
    return esp_ble_gattc_write_char(slave_gattc_if, slave_conn_id, char_handle_tx,
        len, (uint8_t*)data, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
}

void ble_client_register_callback(void (*on_data)(const uint8_t*, uint16_t)) {
    data_callback = on_data;
}

void ble_client_register_connect_callback(void (*on_connect)(bool)) {
    connect_callback = on_connect;
}
