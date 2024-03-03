#pragma once

#include "sensor_data.h"

void http_server_init();
void http_server_start();
void http_server_stop();
void http_server_set_sensor_data(struct sensor_data sd_input);
