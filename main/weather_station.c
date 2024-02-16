#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "ble_gatt_server.h"
extern uint8_t ssid_value[33];
extern uint8_t password_value[64];
#include "connect_wifi.h"
#include "http_server.h"

#define TAG "WEATHER_STATION"

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

static char LCD_message_buffer[64] = {0}; 
static SemaphoreHandle_t LCD_message_buffer_update;

static void pollDHT11(void* parameter) {
	uint32_t data = 0;
	uint8_t th_value[4];
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

		ESP_LOGI(TAG, "Humidity: %.0f%% | Temperature: %.1f°C ~ %.2f°F", humidity, celsius, fahrenheit);
        sprintf(LCD_message_buffer, " Temp: %.2f F                           Humidity: %.0f%%", fahrenheit, humidity);
		xSemaphoreGive(LCD_message_buffer_update);
		
		th_value[0] = data >> 24;
		th_value[1] = (data & 0xFF0000) >> 16;
		th_value[2] = (data & 0xFF00) >> 8;
		th_value[3] = data & 0xFF;

		ble_gatt_server_set_th_value(th_value);
		http_server_set_th_value(th_value);

		ble_gatt_server_notify();

        vTaskDelay(2500 / portTICK_PERIOD_MS);
    }
}

static void send_command(int command) {
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

static void send_letter(char letter) {
    send_command(0x100 + letter);
}

static void send_string(char *buf) {
    for (int i = 0; buf[i] != '\0'; i++) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        send_letter(buf[i]);
    }
}

static void outputLCD(void* parameter) {
	// two line mode
	send_command(0x038);

    // remove cursor
    send_command(0x00C);

    while(1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);

		if (xSemaphoreTake(LCD_message_buffer_update, 1) == pdFALSE) {
			continue;
		}

        // clear display
        send_command(0x001);

        // return home
        send_command(0x002);

		// write message
        send_string(LCD_message_buffer);
    }
}

void app_main(void)
{
	nvs_flash_init();

	LCD_message_buffer_update = xSemaphoreCreateBinary();

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

	ble_gatt_server_init();
	connect_wifi_init();
	http_server_start();

    xTaskCreatePinnedToCore(
        pollDHT11,
        "Measure Temperature and Humidity",
        2048,
        NULL,
        2,
        NULL,
		1
    );
    
    xTaskCreatePinnedToCore(
        outputLCD,
        "Output Data to LCD",
        2048,
        NULL,
        1,
        NULL,
		1
    );

	while (1) {
		if (ble_gatt_server_ssid_password_set() && !connect_wifi_connected()) {
			connect_wifi(ssid_value, password_value);
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
