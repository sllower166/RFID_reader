#ifndef CONFIG_H
#define CONFIG_H

#define WIFI_SSID ""
#define WIFI_PASS ""

const char *websockets_server_host = "";
const uint16_t websockets_server_port = 443;

const char ENDPOINT_CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";

#endif
