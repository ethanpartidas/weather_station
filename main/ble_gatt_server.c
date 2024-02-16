#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"

#include <string.h>
#include "ble_gatt_server.h"

#define TAG "BLE_GATT_SERVER"
#define DEVICE_NAME "Weather Station"

static ble_gatt_server_callback_t callback = NULL;

static uint8_t adv_service_uuid128[32] = {
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
	0xfb, 0x3f, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

static esp_ble_adv_data_t adv_data = {
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
	.service_uuid_len = sizeof(adv_service_uuid128),
	.p_service_uuid = adv_service_uuid128,
	.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
	.adv_int_min = 0x20,
	.adv_int_max = 0x40,
	.adv_type = ADV_TYPE_IND,
	.own_addr_type = BLE_ADDR_TYPE_PUBLIC,
	.channel_map = ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile {
	esp_gatt_if_t gatts_if;
	uint16_t conn_id;
	uint8_t connected;
};

static struct gatts_profile profile = {};

static const uint16_t gatts_service_uuid = 0x00FF;
static const uint16_t gatts_th_uuid = 0xFF01;
static const uint16_t gatts_ssid_uuid = 0xFF02;
static const uint16_t gatts_password_uuid = 0xFF03;

static const uint16_t primary_service_uuid              = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t characteristic_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t characteristic_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_notify              = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write             	= ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static uint8_t th_ccc[2]      			   				= {0};
static uint8_t th_value[4]                 				= {0};
uint8_t ssid_value[33]									= {0};
uint8_t password_value[64]								= {0};
uint8_t ssid_set = 0;

enum {
	SERV_IDX,
	TH_IDX,
	TH_VAL_IDX,
	TH_CCC_IDX,
	SSID_IDX,
	SSID_VAL_IDX,
	PASS_IDX,
	PASS_VAL_IDX,
	NUM_HANDLES
};

static uint16_t handles[NUM_HANDLES];

static const esp_gatts_attr_db_t gatts_db[] = {
	// Service Declaration
	[SERV_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&primary_service_uuid,
			ESP_GATT_PERM_READ,
			sizeof(uint16_t),
			sizeof(gatts_service_uuid),
			(uint8_t *)&gatts_service_uuid
		}
	},
	// TH Characteristic Declaration
	[TH_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&characteristic_declaration_uuid,
			ESP_GATT_PERM_READ,
			sizeof(uint8_t),
			sizeof(uint8_t),
			(uint8_t *)&char_prop_read_notify
		}
	},
	// TH Characteristic Value
	[TH_VAL_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&gatts_th_uuid,
			ESP_GATT_PERM_READ,
			sizeof(th_value),
			sizeof(th_value),
			(uint8_t *)&th_value
		}
	},
	// TH Characteristic CCC
	[TH_CCC_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&characteristic_client_config_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			sizeof(uint16_t),
			sizeof(th_ccc),
			(uint8_t *)&th_ccc
		}
	},
	// SSID Characteristic Declaration
	[SSID_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&characteristic_declaration_uuid,
			ESP_GATT_PERM_READ,
			sizeof(uint8_t),
			sizeof(uint8_t),
			(uint8_t *)&char_prop_write
		}
	},
	// SSID Characteristic Value
	[SSID_VAL_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&gatts_ssid_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			32,
			0,
			(uint8_t *)&ssid_value
		}
	},
	// Password Characteristic Declaration
	[PASS_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&characteristic_declaration_uuid,
			ESP_GATT_PERM_READ,
			sizeof(uint8_t),
			sizeof(uint8_t),
			(uint8_t *)&char_prop_write
		}
	},
	// Password Characteristic Value
	[PASS_VAL_IDX] = {
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&gatts_password_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			63,
			0,
			(uint8_t *)&password_value
		}
	},
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
	switch(event) {
		case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
			esp_ble_gap_start_advertising(&adv_params);
		default:
			break;
	}
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
	switch (event) {
		case ESP_GATTS_REG_EVT:
			profile.gatts_if = gatts_if;

			esp_ble_gap_set_device_name(DEVICE_NAME);

			esp_ble_gap_config_adv_data(&adv_data);

			esp_ble_gatts_create_attr_tab(gatts_db, gatts_if, NUM_HANDLES, 0);

			ESP_LOGI(TAG, "GATT Server Registered");
			break;
		case ESP_GATTS_CREAT_ATTR_TAB_EVT:
			memcpy(handles, param->add_attr_tab.handles, sizeof(handles));
			esp_ble_gatts_start_service(handles[SERV_IDX]);
			ESP_LOGI(TAG, "GATT Server Attribute Table Created");
			break;
		case ESP_GATTS_WRITE_EVT:
			if (param->write.handle == handles[TH_CCC_IDX]) {
					esp_ble_gatts_set_attr_value(handles[TH_CCC_IDX], 2, param->write.value);
					th_ccc[0] = param->write.value[0];
					th_ccc[1] = param->write.value[1];
					ESP_LOGI(TAG, "Client Wrote to Configuration");
			} else if (param->write.handle == handles[SSID_VAL_IDX]) {
				memset(ssid_value, 0, 32);
				memcpy(ssid_value, param->write.value, param->write.len);
				ESP_LOGI(TAG, "Client Set SSID: %s", ssid_value);
				ssid_set = 1;
			} else if (param->write.handle == handles[PASS_VAL_IDX]) {
				memset(password_value, 0, 63);
				memcpy(password_value, param->write.value, param->write.len);
				ESP_LOGI(TAG, "Client Set Password: %s", password_value);
				if (ssid_set) {
					callback(BLE_GATT_SERVER_SSID_PASSWORD_SET_EVENT);
				}
			}
			break;
		case ESP_GATTS_CONNECT_EVT:
			profile.conn_id = param->connect.conn_id;
			profile.connected = 1;
			ESP_LOGI(TAG, "Client Connected");
			break;
		case ESP_GATTS_DISCONNECT_EVT:
			profile.connected = 0;
			esp_ble_gap_start_advertising(&adv_params);
			ESP_LOGI(TAG, "Client Disconnected");
			break;
		default:
			break;
	}
}

void ble_gatt_server_init() {
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	esp_bt_controller_init(&bt_cfg);
	esp_bt_controller_enable(ESP_BT_MODE_BLE);

	esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
	esp_bluedroid_init_with_cfg(&bluedroid_cfg);
	esp_bluedroid_enable();

	esp_ble_gap_register_callback(gap_event_handler);
	esp_ble_gatts_register_callback(gatts_event_handler);

	esp_ble_gatts_app_register(0);

	esp_ble_gatt_set_local_mtu(500);
}

void ble_gatt_server_set_th_value(uint8_t *th_value_input) {
	memcpy(th_value, th_value_input, 4);
	esp_ble_gatts_set_attr_value(handles[TH_VAL_IDX], 4, th_value);
}

void ble_gatt_server_notify() {
	if (profile.connected && th_ccc[0] == 0x01) {
		esp_ble_gatts_send_indicate(
			profile.gatts_if,
			profile.conn_id,
			handles[TH_VAL_IDX],
			4,
			th_value,
			false
		);
		ESP_LOGI(TAG, "Notifying Client of Update");
	}
}

void ble_gatt_server_register_callback(ble_gatt_server_callback_t cb) {
	callback = cb;
}
