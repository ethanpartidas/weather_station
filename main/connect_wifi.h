#ifndef CONNECT_WIFI_H
#define CONNECT_WIFI_H

void connect_wifi_init();
void connect_wifi();
uint8_t connect_wifi_connected();
void connect_wifi_config(uint8_t *ssid, uint8_t *password);

#endif
