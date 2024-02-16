#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

void http_server_init();
void http_server_start();
void http_server_stop();
void http_server_set_th_value(uint8_t *th_value_input);

#endif
