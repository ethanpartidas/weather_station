#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t server = NULL;
static uint8_t th_value[4] = {0};
static char message_buffer[400] = {0};

static esp_err_t get_handler(httpd_req_t *req) {
	float humidity = th_value[0];
	float celsius = th_value[2] + (float)th_value[3] / 10;
	float fahrenheit = celsius * 1.8 + 32;

	sprintf(message_buffer, "Humidity: %.0f%% | Temperature: %.1f°C ~ %.2f°F", humidity, celsius, fahrenheit);
	
	httpd_resp_send(req, message_buffer, HTTPD_RESP_USE_STRLEN);
	ESP_LOGI(TAG, "Received GET Request");
	return ESP_OK;
}

static httpd_uri_t uri_get = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = get_handler,
	.user_ctx = NULL
};

void http_server_start() {
	if (server != NULL) return;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_start(&server, &config);
	httpd_register_uri_handler(server, &uri_get);
	ESP_LOGI(TAG, "Started HTTP Server");
}

void http_server_stop() {
	httpd_stop(server);
	server = NULL;
	ESP_LOGI(TAG, "Stopped HTTP Server");
}

void http_server_set_th_value(uint8_t *th_value_input) {
	memcpy(th_value, th_value_input, 4);
}
