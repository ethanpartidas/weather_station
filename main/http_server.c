#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "http_server.h"

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t server = NULL;
static struct sensor_data sd;
static char message_buffer[4096] = {0};
static char log_buffer[32768] = {0};

static void implant_string(char *template, char *target, char *value) {
    char *dest = strstr(template, target);
    if (dest == NULL) {
        ESP_LOGE(TAG, "Target string (%s) not found in template.", target);
        return;
    }
    strncpy(dest, value, strlen(target));
}

static esp_err_t get_handler(httpd_req_t *req) {
    float humidity = sensor_data_get_humidity(sd);
    float celsius = sensor_data_get_celsius(sd);
    float fahrenheit = sensor_data_get_fahrenheit(sd);

    FILE *f = fopen("/filesystem/index.html", "r");
    fread(message_buffer, 1, sizeof(message_buffer), f);
    fclose(f);

    char temp[6] = {0};
    sprintf(temp, "%02.0f", humidity);
    implant_string(message_buffer, "hm", temp);
    sprintf(temp, "%02.0f", celsius);
    implant_string(message_buffer, "cs", temp);
    sprintf(temp, "%02.0f", fahrenheit);
    implant_string(message_buffer, "fh", temp);

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

static esp_err_t download_handler(httpd_req_t *req) {
    FILE *f = fopen("/filesystem/log.csv", "r");
    fread(log_buffer, 1, sizeof(log_buffer), f);
    fclose(f);

    httpd_resp_set_type(req, "text/csv");
    httpd_resp_send(req, log_buffer, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Received Download Request");
    return ESP_OK;
}

static httpd_uri_t uri_download = {
    .uri = "/log.csv",
    .method = HTTP_GET,
    .handler = download_handler,
    .user_ctx = NULL
};

void initialize_sntp() {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
}

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

    initialize_sntp();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_download);

    ESP_LOGI(TAG, "Started HTTP Server");
}

void http_server_stop() {
    httpd_stop(server);
    server = NULL;
    ESP_LOGI(TAG, "Stopped HTTP Server");
}

void http_server_set_sensor_data(struct sensor_data sd_input) {
    sd = sd_input;

    uint8_t humidity = sensor_data_get_humidity(sd);
    float celsius = sensor_data_get_celsius(sd);

    char time_str[64];
    time_t now;
    struct tm time_info;
    time(&now);
    localtime_r(&now, &time_info);
    strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", &time_info);
    if (time_str[0] == '1') return;

    sprintf(message_buffer, "%s,%.1f,%d\n", time_str, celsius, humidity);

    FILE *f = fopen("/filesystem/log.csv", "a");
    fwrite(message_buffer, 1, strlen(message_buffer), f);
    fclose(f);
}
