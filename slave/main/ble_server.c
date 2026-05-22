#include "ble_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

static const char* TAG = "BLE_SERVER";

static bool connected = false;
static uint16_t server_gatts_if = 0;
static uint16_t conn_id = 0;
static uint16_t char_handle_tx = 0;
static uint16_t char_handle_rx = 0;

static void (*data_callback)(const uint8_t*, uint16_t) = NULL;

#define GATTS_SERVICE_UUID_TEST   0xFFFF
#define GATTS_CHAR_UUID_TEST_TX   0xFF01
#define GATTS_CHAR_UUID_TEST_RX   0xFF02
#define PROFILE_NUM               1
#define PROFILE_APP_IDX           0

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write_nr = ESP_GATT_CHAR_PROP_BIT_WRITE_NR;

static const uint16_t GATTS_SERVICE_UUID_TEST = 0xFFFF;
static const uint16_t GATTS_CHAR_UUID_TEST_TX = 0xFF01;
static const uint16_t GATTS_CHAR_UUID_TEST_RX = 0xFF02;

static uint8_t char_value[4] = {0x11, 0x22, 0x33, 0x44};

static const esp_gatts_attr_db_t gatt_db[10] = {
    // Service Declaration
    [0] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t *)&GATTS_SERVICE_UUID_TEST}},

    // Characteristic Declaration - TX
    [1] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},

    // Characteristic Value - TX
    [2] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_TX, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, sizeof(char_value), (uint8_t *)char_value}},

    // Client Characteristic Configuration Descriptor - TX
    [3] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), sizeof(char_value), (uint8_t *)char_value}},

    // Characteristic Declaration - RX
    [4] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write_nr}},

    // Characteristic Value - RX
    [5] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEST_RX, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}},
};

#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40
#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(NULL);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising started");
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                        esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;

        esp_ble_gap_set_device_name("OWL_SLAVE");
        
        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = false,
            .min_interval = 0x0006,
            .max_interval = 0x0010,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data = NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = sizeof(uint16_t),
            .p_service_uuid = (uint8_t *)&GATTS_SERVICE_UUID_TEST,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        };
        
        esp_ble_gap_config_adv_data(&adv_data);
        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 10, 0);
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
        if (!param->write.is_prep) {
            ESP_LOGI(TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
            esp_log_buffer_hex(TAG, param->write.value, param->write.len);
            if (data_callback) {
                data_callback(param->write.value, param->write.len);
            }
        }
        break;

    case ESP_GATTS_EXEC_WRITE_EVT:
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;

    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
        break;

    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "CONNECT_EVT, conn_id %d, remote BDA:", param->connect.conn_id);
        esp_log_buffer_hex(TAG, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        connected = true;
        conn_id = param->connect.conn_id;
        server_gatts_if = gatts_if;
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "DISCONNECT_EVT, reason = %d", param->disconnect.reason);
        connected = false;
        esp_ble_gap_start_advertising(NULL);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        ESP_LOGI(TAG, "The number handle = %x", param->add_attr_tab.num_handle);
        if (param->create.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "Create attribute table successfully, the number handle = %d", param->add_attr_tab.num_handle);
            memcpy((void *)char_value, (void *)gatt_db[2].attr_value, gatt_db[2].attr_len);
            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_APP_IDX].gatts_if, 
                                       gl_profile_tab[PROFILE_APP_IDX].app_id);
        } else {
            ESP_LOGE(TAG, "Create attribute table failed, error code = %x", param->create.status);
        }
        break;

    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == gl_profile_tab[idx].gatts_if) {
                if (gl_profile_tab[idx].gatts_cb) {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

bool ble_server_is_connected(void) {
    return connected;
}

int ble_server_send_data(const uint8_t* data, uint16_t len) {
    if (!connected) return -1;
    return esp_ble_gatts_send_indicate(server_gatts_if, conn_id, char_handle_tx, len, (uint8_t*)data, false);
}

void ble_server_register_callback(void (*on_data)(const uint8_t*, uint16_t)) {
    data_callback = on_data;
}

void ble_server_init(void) {
    ESP_LOGI(TAG, "Initializing BLE server...");

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

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    esp_ble_gatt_set_local_mtu(500);
    ESP_LOGI(TAG, "BLE server initialized");
}
