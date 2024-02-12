#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_timer.h"

#define GATTS_TAG "GATTS_DEMO"
#define DEVICE_NAME "Weather Station"

#define DHT11_PIN 16
#define RS 17
#define E 18
#define D7 19
#define D6 21
#define D5 22
#define D4 23
#define D3 25
#define D2 26
#define D1 27
#define D0 32

char buf[64] = {0}; 
static SemaphoreHandle_t buf_update;

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
static const uint16_t gatts_char_uuid = 0xFF01;

static const uint16_t primary_service_uuid              = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t characteristic_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t characteristic_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read_notify              = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t char_ccc[2]      			   			= {0x00, 0x00};
static uint8_t char_value[4]                 			= {0x00, 0x00, 0x00, 0x00};

enum {
	SERV_IDX,
	CHAR_IDX,
	VAL_IDX,
	CCC_IDX,
	NUM_HANDLES
};

uint16_t handles[NUM_HANDLES];

#define GATTS_NUM_DEC 5
static const esp_gatts_attr_db_t gatts_db[] = {
	// Service Declaration
	{
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
	// Characteristic Declaration
	{
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
	// Characteristic Value
	{
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&gatts_char_uuid,
			ESP_GATT_PERM_READ,
			sizeof(char_value),
			sizeof(char_value),
			(uint8_t *)&char_value
		}
	},
	// Characteristic CCC
	{
		{ESP_GATT_AUTO_RSP},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&characteristic_client_config_uuid,
			ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
			sizeof(uint16_t),
			sizeof(char_ccc),
			(uint8_t *)&char_ccc
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

			esp_ble_gatts_create_attr_tab(gatts_db, gatts_if, GATTS_NUM_DEC, 0);

			printf("GATT Server Registered\n");
			break;
		case ESP_GATTS_CREAT_ATTR_TAB_EVT:
			memcpy(handles, param->add_attr_tab.handles, sizeof(handles));
			esp_ble_gatts_start_service(handles[SERV_IDX]);
			printf("GATT Server Attribute Table Created\n");
			break;
		case ESP_GATTS_WRITE_EVT:
			if (param->write.handle == handles[CCC_IDX]) {
				esp_ble_gatts_set_attr_value(handles[CCC_IDX], 2, param->write.value);
				char_ccc[0] = param->write.value[0];
				char_ccc[1] = param->write.value[1];
				printf("Client Wrote to Configuration\n");
			}
			break;
		case ESP_GATTS_CONNECT_EVT:
			profile.conn_id = param->connect.conn_id;
			profile.connected = 1;
			printf("Client Connected\n");
			break;
		case ESP_GATTS_DISCONNECT_EVT:
			profile.connected = 0;
			esp_ble_gap_start_advertising(&adv_params);
			printf("Client Disconnected\n");
			break;
		default:
			break;
	}
}

void pollDHT11(void* parameter) {
	uint32_t data = 0;
	int64_t time_us_start;
	int64_t time_us_stop;
    while(1) {
        gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(DHT11_PIN, 0);
    	time_us_start = esp_timer_get_time();
		while (esp_timer_get_time() - time_us_start < 20000) {}
        gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);
        while(gpio_get_level(DHT11_PIN) == 1){}
        while(gpio_get_level(DHT11_PIN) == 0){}
        while(gpio_get_level(DHT11_PIN) == 1){}
        for (int i = 0; i < 32; i++) {
            while(gpio_get_level(DHT11_PIN) == 0){}
            time_us_start = esp_timer_get_time();
            while(gpio_get_level(DHT11_PIN) == 1){}
            time_us_stop = esp_timer_get_time();
            uint8_t bit = (time_us_stop - time_us_start > 50) ? 1 : 0;
            data = (data << 1) | bit;
        }

        float humidity = data >> 24;
        float celsius = ((data << 16) >> 24) + (float)((data << 24) >> 24) / 10;
		float fahrenheit = celsius * 1.8 + 32;

		printf("Humidity: %.0f%% | Temperature: %.1f°C ~ %.2f°F\n", humidity, celsius, fahrenheit);
        sprintf(buf, " Temp: %.2f F                           Humidity: %.0f%%", fahrenheit, humidity);
		xSemaphoreGive(buf_update);
		
		char_value[0] = data >> 24;
		char_value[1] = (data & 0xFF0000) >> 16;
		char_value[2] = (data & 0xFF00) >> 8;
		char_value[3] = data & 0xFF;

		esp_ble_gatts_set_attr_value(handles[VAL_IDX], 4, char_value);

		if (profile.connected && char_ccc[0] == 0x01) {
			esp_ble_gatts_send_indicate(
				profile.gatts_if,
				profile.conn_id,
				handles[VAL_IDX],
				4,
				char_value,
				false
			);
			printf("Notifying Client of Update\n");
		}

        vTaskDelay(2500 / portTICK_PERIOD_MS);
    }
}

void send_command(int command) {
    gpio_set_level(RS, (command & 0x100) != 0);
    gpio_set_level(D7, (command & 0x080) != 0);
    gpio_set_level(D6, (command & 0x040) != 0);
    gpio_set_level(D5, (command & 0x020) != 0);
    gpio_set_level(D4, (command & 0x010) != 0);
    gpio_set_level(D3, (command & 0x008) != 0);
    gpio_set_level(D2, (command & 0x004) != 0);
    gpio_set_level(D1, (command & 0x002) != 0);
    gpio_set_level(D0, (command & 0x001) != 0);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(E, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(E, 0);
}

void send_letter(char letter) {
    send_command(0x100 + letter);
}

void send_string(char *buf) {
    for (int i = 0; buf[i] != '\0'; i++) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        send_letter(buf[i]);
    }
}

void outputLCD(void* parameter) {
	// two line mode
	send_command(0x038);

    // remove cursor
    send_command(0x00C);

    while(1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);

		if (xSemaphoreTake(buf_update, 1) == pdFALSE) {
			continue;
		}

        // clear display
        send_command(0x001);

        // return home
        send_command(0x002);

		// write message
        send_string(buf);
    }
}

void app_main(void)
{
	nvs_flash_init();
	
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

    gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(RS, GPIO_MODE_OUTPUT);
    gpio_set_direction(E, GPIO_MODE_OUTPUT);
    gpio_set_direction(D7, GPIO_MODE_OUTPUT);
    gpio_set_direction(D6, GPIO_MODE_OUTPUT);
    gpio_set_direction(D5, GPIO_MODE_OUTPUT);
    gpio_set_direction(D4, GPIO_MODE_OUTPUT);
    gpio_set_direction(D3, GPIO_MODE_OUTPUT);
    gpio_set_direction(D2, GPIO_MODE_OUTPUT);
    gpio_set_direction(D1, GPIO_MODE_OUTPUT);
    gpio_set_direction(D0, GPIO_MODE_OUTPUT);

	buf_update = xSemaphoreCreateBinary();

    xTaskCreate(
        pollDHT11,
        "Measure Temperature and Humidity",
        2048,
        NULL,
        2,
        NULL
    );
    
    xTaskCreate(
        outputLCD,
        "Output Data to LCD",
        2048,
        NULL,
        1,
        NULL
    );
}
