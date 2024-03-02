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

#define DHT11_PIN 26
#define RS 23
#define RW 22
#define E 21
#define D7 27
#define D6 13
#define D5 15
#define D4 4
#define D3 16
#define D2 17
#define D1 18
#define D0 19

static char LCD_message_buffer[64] = {0}; 
static SemaphoreHandle_t LCD_message_buffer_update;

static void pollDHT11(void* parameter) {
	uint64_t data = 0x3200140000;
	uint8_t th_value[4];
	int64_t time_us_start;
	int64_t time_us_stop;
    while(1) {
        gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(DHT11_PIN, 0);
    	time_us_start = esp_timer_get_time();
		while (esp_timer_get_time() - time_us_start < 20000) {}
        gpio_set_level(DHT11_PIN, 1);
        gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);
        while(gpio_get_level(DHT11_PIN) == 1){}
        while(gpio_get_level(DHT11_PIN) == 0){}
        while(gpio_get_level(DHT11_PIN) == 1){}
		int timeout = 0;
        for (int i = 0; i < 40; i++) {
            while(gpio_get_level(DHT11_PIN) == 0) {}
            time_us_start = esp_timer_get_time();
            while(gpio_get_level(DHT11_PIN) == 1) {
				time_us_stop = esp_timer_get_time();
				if (time_us_stop - time_us_start > 100) {
					timeout = 1;
					break;
				}
			}
			if (timeout) {
				break;
			}
            uint8_t bit = (time_us_stop - time_us_start > 50) ? 1 : 0;
            data = (data << 1) | bit;
        }
		if (timeout) {
			ESP_LOGW(TAG, "DHT11 Read Timed Out");
			vTaskDelay(1000);
			continue;
		}

		uint16_t checksum =
			  ((data & 0xFF00000000) >> 32)
			+ ((data & 0x00FF000000) >> 24)
			+ ((data & 0x0000FF0000) >> 16)
			+ ((data & 0x000000FF00) >> 8);
		if ((checksum & 0xFF) != (data & 0xFF)) {
			ESP_LOGW(TAG, "DHT11 Checksum Invalid");
			vTaskDelay(1000);
			continue;
		}

		data = (data >> 8) & 0xFFFFFFFF;

        float humidity = (data & 0xFF000000) >> 24;
        float celsius = ((data & 0x0000FF00) >> 8) + (float)(data & 0x000000FF) / 10;
		float fahrenheit = celsius * 1.8 + 32;

		ESP_LOGI(TAG, "Humidity: %.0f%% | Temperature: %.1f°C ~ %.2f°F", humidity, celsius, fahrenheit);
        sprintf(LCD_message_buffer, " Temp: %.2f F                           Humidity: %.0f%%  ", fahrenheit, humidity);
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
	
	int64_t time_us_start = esp_timer_get_time();

    gpio_set_level(E, 1);
	while (esp_timer_get_time() - time_us_start < 1000) {}
    gpio_set_level(E, 0);
	while (esp_timer_get_time() - time_us_start < 2000) {}
}

static void send_letter(char letter) {
    send_command(0x100 + letter);
}

static void send_string(char *buf) {
    for (int i = 0; buf[i] != '\0'; i++) {
        send_letter(buf[i]);
    }
}

static void outputLCD(void* parameter) {
	// two line mode
	send_command(0x038);

    // remove cursor
    send_command(0x00C);

	// clear display
	send_command(0x001);

    while(1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);

		if (xSemaphoreTake(LCD_message_buffer_update, 1) == pdFALSE) {
			continue;
		}

        // return home
        send_command(0x002);

		// write message
        send_string(LCD_message_buffer);
    }
}

void ble_gatt_server_callback(ble_gatt_server_event_t event) {
	switch (event) {
		case BLE_GATT_SERVER_SSID_PASSWORD_SET_EVENT:
			connect_wifi_config(ssid_value, password_value);
			connect_wifi();
		break;
	}
}

void app_main(void)
{
	nvs_flash_init();

	LCD_message_buffer_update = xSemaphoreCreateBinary();

	gpio_config_t input_gpio_conf = {
		.pin_bit_mask = (1 << DHT11_PIN),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&input_gpio_conf);

	gpio_num_t output_pins[11] = {RS, RW, E, D0, D1, D2, D3, D4, D5, D6, D7};
	uint64_t output_pin_bit_mask = 0;
	for (int i = 0; i < 11; i++) {
		output_pin_bit_mask |= (1 << output_pins[i]);
	}
	gpio_config_t output_gpio_conf = {
		.pin_bit_mask = output_pin_bit_mask,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&output_gpio_conf);

	ble_gatt_server_init();
	ble_gatt_server_register_callback(ble_gatt_server_callback);

	connect_wifi_init();
	connect_wifi();

	http_server_init();
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
}
