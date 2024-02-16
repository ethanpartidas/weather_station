#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include <string.h>

#define MAX_ATTEMPTS 3

static const char *TAG = "CONNECT_WIFI";

static int wifi_connection_attempts = 0;
static int wifi_connected = 0;
static SemaphoreHandle_t wifi_connect_done;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	if (wifi_connection_attempts < MAX_ATTEMPTS) {
		ESP_LOGI(TAG, "Reattempting to Connect to WIFI");
		esp_wifi_connect();
		wifi_connection_attempts++;
	} else {
		xSemaphoreGive(wifi_connect_done);
	}
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	ESP_LOGI(TAG, "Connected to WIFI");
	wifi_connection_attempts = 0;
	wifi_connected = 1;
	xSemaphoreGive(wifi_connect_done);
}

void connect_wifi_init() {
	wifi_connect_done = xSemaphoreCreateBinary();

	// create default event loop
	esp_event_loop_create_default();

	// next function needs network interface
	esp_netif_init();
	// register wifi events to default event loop
	esp_netif_create_default_wifi_sta();

	// initialize wifi stack and start wifi task
	wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&init_config);
}

void connect_wifi_config(uint8_t *ssid, uint8_t *password) {
	wifi_config_t wifi_config = {};
	strcpy((char *)wifi_config.sta.ssid, (char *)ssid);
	strcpy((char *)wifi_config.sta.password, (char *)password);
	ESP_LOGI(TAG, "SSID Set: %s, Password Set: %s", wifi_config.sta.ssid, wifi_config.sta.password);
	esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

void connect_wifi() {
	if (wifi_connected) return;

	esp_event_handler_instance_t wifi_event_handler_instance;
	esp_event_handler_instance_register(
		WIFI_EVENT,
		WIFI_EVENT_STA_DISCONNECTED,
		&wifi_event_handler,
		NULL,
		&wifi_event_handler_instance
	);
	esp_event_handler_instance_t ip_event_handler_instance;
	esp_event_handler_instance_register(
		IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&ip_event_handler,
		NULL,
		&ip_event_handler_instance
	);

	esp_wifi_start();

	ESP_LOGI(TAG, "Attempting to Connect to WIFI");
	esp_wifi_connect();

	xSemaphoreTake(wifi_connect_done, 10000 / portTICK_PERIOD_MS);

	if (!wifi_connected) {
		ESP_LOGI(TAG, "Failed to Connect to WIFI");
		esp_wifi_stop();
	}

	wifi_connection_attempts = 0;

	esp_event_handler_instance_unregister(
		WIFI_EVENT,
		WIFI_EVENT_STA_DISCONNECTED,
		wifi_event_handler_instance
	);
	esp_event_handler_instance_unregister(
		IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		ip_event_handler_instance
	);
}

uint8_t connect_wifi_connected() {
	return wifi_connected;
}
