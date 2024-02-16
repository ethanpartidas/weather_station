idf_component_register(
	SRCS
		"ble_gatt_server.c"          
		"connect_wifi.c"
		"http_server.c"
		"weather_station.c"
	INCLUDE_DIRS
		"."
)

spiffs_create_partition_image(
	filesystem
	../filesystem
	FLASH_IN_PROJECT
)
