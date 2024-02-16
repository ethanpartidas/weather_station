#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t server = NULL;
static uint8_t th_value[4] = {0};
static char message_buffer[2048] = {0};

static void implant_string(char *template, char *target, char *value) {
	char *dest = strstr(template, target);
	strncpy(dest, value, strlen(target));
}

static esp_err_t get_handler(httpd_req_t *req) {
	float humidity = th_value[0];
	float celsius = th_value[2] + (float)th_value[3] / 10;
	float fahrenheit = celsius * 1.8 + 32;

	FILE *f = fopen("/filesystem/index.html", "r");
	fread(message_buffer, 1, sizeof(message_buffer), f);
	fclose(f);

	char temp[6] = {0};
	sprintf(temp, "%.0f", humidity);
	implant_string(message_buffer, "hm", temp);
	sprintf(temp, "%.1f", celsius);
	implant_string(message_buffer, "cels", temp);
	sprintf(temp, "%.2f", fahrenheit);
	implant_string(message_buffer, "fahre", temp);

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

void http_server_init() {
	esp_vfs_spiffs_conf_t config = {
		.base_path = "/filesystem",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true
	};
	esp_vfs_spiffs_register(&config);
}

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
