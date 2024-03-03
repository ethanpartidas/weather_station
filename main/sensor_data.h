#pragma once

struct sensor_data {
	uint8_t humidity;
	uint16_t temperature;
};

static inline uint8_t sensor_data_get_humidity(struct sensor_data sd) {
	return sd.humidity;
}

static inline float sensor_data_get_celsius(struct sensor_data sd) {
	return ((sd.temperature & 0xFF00) >> 8) + (float)(sd.temperature & 0x00FF) / 10;
}

static inline float sensor_data_get_fahrenheit(struct sensor_data sd) {
	return sensor_data_get_celsius(sd) * 1.8 + 32;
}
