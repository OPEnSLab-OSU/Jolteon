#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/fdtable.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include "wt_wifi.h"
#include "wt_winc_at.h"

#define WINC_TCP_COUNT 7
#define WINC_SOCKET_COUNT 11
#define WINC_SOCKET_TIMEOUT_MS 5000

/* Owned by the UART AT session. Store fd + 1 so the zero state is free. */
static struct winc_socket {
	int handle;
	struct sockaddr_in peer;
	bool send_event;
	bool auto_receive;
} sockets[WINC_SOCKET_COUNT];
static uint8_t rx_payload[WT_WINC_PAYLOAD_MAX];
static bool station_connected;

static const struct {
	const char *name;
	const char *args;
} commands[] = {
	{ "VER", "" }, { "LIST", "" }, { "ECHO", "=<0|1>" }, { "ECHO?", "" },
	{ "SCAN", "[=<channel>]" }, { "DHCP", "=1" },
	{ "CONN", "=<ssid>,<security>[,<key>][,<channel>]" },
	{ "MODE", "" }, { "DISMODE", "" }, { "GETMAC", "" }, { "GETIP", "" },
	{ "RSSI", "" }, { "RESDNS", "=<host>" },
	{ "TCPCLI", "=<ip>,<port>[,0[,<auto recv>]]" },
	{ "UDP", "=<local port>[,<ip>[,<port>[,<auto recv>]]]" },
	{ "UDPCFG", "=<socket>,<ip>,<port>" }, { "UDPCB", "=<socket>,<0|1>" },
	{ "CLOSE", "=<socket>" }, { "CLOSEALL", "" },
};

static unsigned int split_fields(char *text, char **fields, unsigned int max)
{
	unsigned int count = 0;
	while (text && count < max) {
		char *comma = strchr(text, ',');
		fields[count++] = text;
		if (comma) {
			*comma++ = '\0';
		}
		text = comma;
	}
	return text ? max + 1 : count;
}

static int number(const char *text, unsigned int limit)
{
	unsigned int value = 0;
	if (!text || !*text) {
		return -1;
	}
	for (; *text >= '0' && *text <= '9'; ++text) {
		value = value * 10 + *text - '0';
		if (value > limit) {
			return -1;
		}
	}
	return *text ? -1 : (int)value;
}

static int parse_peer(struct sockaddr_in *peer, const char *ip, const char *port)
{
	struct sockaddr_in address = { .sin_family = AF_INET };
	int value = number(port, UINT16_MAX);

	if (value <= 0 || zsock_inet_pton(AF_INET, ip, &address.sin_addr) != 1) {
		return -301;
	}
	address.sin_port = htons(value);
	*peer = address;
	return 0;
}

static void close_socket(int id)
{
	(void)zsock_close(sockets[id].handle - 1);
	sockets[id] = (struct winc_socket){ 0 };
}

void wt_winc_close_all(void)
{
	for (int i = 0; i < WINC_SOCKET_COUNT; ++i) {
		if (sockets[i].handle) {
			close_socket(i);
		}
	}
}

static int existing_socket(const char *text)
{
	int id = text && *text && !text[1] ? wt_winc_socket_digit(*text) : -1;
	return id >= 0 && sockets[id].handle ? id : -300;
}

static int open_socket(const struct shell *sh, bool udp, char **args, unsigned int count)
{
	struct sockaddr_in peer = { .sin_family = AF_INET };
	struct sockaddr_in local = { .sin_family = AF_INET };
	int first = udp ? WINC_TCP_COUNT : 0;
	int end = udp ? WINC_SOCKET_COUNT : WINC_TCP_COUNT;
	int id = first;
	int auto_receive = 1;
	int fd, ret;

	if (!wt_wifi_has_ipv4()) {
		return -206;
	}
	if (udp) {
		if (!count || count > 4) {
			return -103;
		}
		int port = number(args[0], UINT16_MAX);
		if (port < 0) {
			return -301;
		}
		local.sin_port = htons(port);
		peer.sin_addr.s_addr = INADDR_BROADCAST;
		peer.sin_port = local.sin_port;
		if (count >= 2 && *args[1] &&
		    zsock_inet_pton(AF_INET, args[1], &peer.sin_addr) != 1) {
			return -301;
		}
		if (count >= 3 && *args[2]) {
			int remote_port = number(args[2], UINT16_MAX);

			if (remote_port <= 0) {
				return -301;
			}
			peer.sin_port = htons(remote_port);
		}
	} else {
		if (count < 2 || count > 4) {
			return -103;
		}
		if (parse_peer(&peer, args[0], args[1])) {
			return -301;
		}
		if (count >= 3 && *args[2] && strcmp(args[2], "0")) {
			return !strcmp(args[2], "1") ? -104 : -105;
		}
	}
	if (count == 4 && *args[3]) {
		auto_receive = number(args[3], 1);
		if (auto_receive < 0) {
			return -105;
		}
	}
	while (id < end && sockets[id].handle) {
		++id;
	}
	if (id == end) {
		return udp ? -304 : -303;
	}
	fd = zsock_socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
	if (fd < 0) {
		return udp ? -304 : -303;
	}
	ret = zsock_fcntl(fd, ZVFS_F_SETFL, ZVFS_O_NONBLOCK);
	if (!ret && udp) {
		ret = zsock_bind(fd, (const struct sockaddr *)&local, sizeof(local));
	} else if (!ret) {
		ret = zsock_connect(fd, (const struct sockaddr *)&peer, sizeof(peer));
		if (ret < 0 && errno == EINPROGRESS) {
			struct zsock_pollfd pending = { .fd = fd, .events = ZSOCK_POLLOUT };
			int result = zsock_poll(&pending, 1, WINC_SOCKET_TIMEOUT_MS);
			int socket_error;
			socklen_t error_len = sizeof(socket_error);
			ret = result > 0 && !zsock_getsockopt(fd, SOL_SOCKET, SO_ERROR,
							     &socket_error, &error_len) && !socket_error ? 0 : -1;
		}
	}
	if (ret) {
		int error = errno;

		(void)zsock_close(fd);
		return udp && error == EADDRINUSE ? -302 : -107;
	}
	sockets[id] = (struct winc_socket){
		.handle = fd + 1, .peer = peer, .send_event = true, .auto_receive = auto_receive,
	};
	station_connected = true;
	shell_print(sh, "+OK+%s=%X", udp ? "UDP" : "TCPCLI", id);
	return 0;
}

static int raw_write(const struct shell *sh, const uint8_t *data, size_t length)
{
	int64_t deadline = k_uptime_get() + 2000;
	while (length && k_uptime_get() < deadline) {
		size_t count = 0;
		int ret = sh->iface->api->write(sh->iface, data, length, &count);
		if (ret) {
			return ret;
		}
		data += count;
		length -= count;
		if (!count) {
			k_sleep(K_MSEC(1));
		}
	}
	return length ? -ETIMEDOUT : 0;
}

static void socket_receive(const struct shell *sh, int id, bool requested)
{
	struct sockaddr_in peer = { 0 };
	socklen_t peer_len = sizeof(peer);
	bool udp = id >= WINC_TCP_COUNT;
	int64_t deadline = k_uptime_get() + WINC_SOCKET_TIMEOUT_MS;
	char header[48];
	int header_size;
	int size;

	do {
		size = zsock_recvfrom(sockets[id].handle - 1, rx_payload, sizeof(rx_payload),
				      ZSOCK_MSG_DONTWAIT, (struct sockaddr *)&peer, &peer_len);
		if (size >= 0 || errno != EAGAIN || !requested) {
			break;
		}
		k_sleep(K_MSEC(20));
	} while (k_uptime_get() < deadline);
	if (size < 0 || (!udp && size == 0)) {
		if (size >= 0 || errno != EAGAIN) {
			shell_print(sh, "+EVT+RECV=%X,%d", id, size == 0 ? -312 : -107);
			sockets[id].auto_receive = false;
		} else if (requested) {
			shell_print(sh, "+EVT+RECV=%X,-111", id);
		}
		return;
	}
	if (udp) {
		char ip[16];
		zsock_inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
		header_size = snprintk(header, sizeof(header), "+EVT+RECV=%X,%d,U,%s,%u,", id, size,
				      ip, ntohs(peer.sin_port));
	} else {
		header_size = snprintk(header, sizeof(header), "+EVT+RECV=%X,%d,T,", id, size);
	}
	if (header_size > 0 && (size_t)header_size < sizeof(header) &&
	    !raw_write(sh, (const uint8_t *)header, header_size) && !raw_write(sh, rx_payload, size)) {
		const uint8_t end = WT_WINC_ETB;
		(void)raw_write(sh, &end, 1);
	}
}

void wt_winc_service(const struct shell *sh)
{
	bool connected = wt_wifi_is_associated();

	if (station_connected && !connected) {
		wt_winc_close_all();
		shell_print(sh, "+EVT+DISCONN");
	}
	station_connected = connected;
	if (connected) {
		for (int id = 0; id < WINC_SOCKET_COUNT; ++id) {
			if (sockets[id].handle && sockets[id].auto_receive) {
				socket_receive(sh, id, false);
			}
		}
	}
}

void wt_winc_stream_command(const struct shell *sh, const struct wt_winc_frame *frame)
{
	int id = frame->socket;
	bool udp = id >= WINC_TCP_COUNT;
	struct winc_socket *socket = &sockets[id];
	struct sockaddr_in peer = socket->peer;
	const uint8_t *payload = frame->data + frame->payload_start;
	int64_t deadline;
	size_t sent = 0;
	int ret = 0;

	if (!socket->handle || !wt_wifi_has_ipv4()) {
		shell_print(sh, "+ERROR+%s=%d", frame->directive == 'R' ? "RECV" : "SEND",
			    socket->handle ? -206 : -300);
		return;
	}
	if (frame->directive == 'R') {
		socket_receive(sh, id, true);
		return;
	}
	if (frame->directive == 'U') {
		char address[24];
		char *args[2];
		size_t size = frame->payload_start - 8;
		if (!udp || size >= sizeof(address)) {
			shell_print(sh, "+ERROR+SEND=-301");
			return;
		}
		memcpy(address, frame->data + 7, size);
		address[size] = '\0';
		if (split_fields(address, args, 2) != 2 || parse_peer(&peer, args[0], args[1])) {
			shell_print(sh, "+ERROR+SEND=-301");
			return;
		}
	}
	if (udp && !peer.sin_port) {
		shell_print(sh, "+ERROR+SEND=-311");
		return;
	}
	deadline = k_uptime_get() + WINC_SOCKET_TIMEOUT_MS;
	while (sent < frame->payload_len && k_uptime_get() < deadline) {
		ret = zsock_sendto(socket->handle - 1, payload + sent, frame->payload_len - sent,
				   ZSOCK_MSG_DONTWAIT, udp ? (const struct sockaddr *)&peer : NULL,
				   udp ? sizeof(peer) : 0);
		if (ret > 0) {
			sent += ret;
			if (udp) {
				break;
			}
		} else if (ret < 0 && errno != EAGAIN) {
			break;
		} else {
			k_sleep(K_MSEC(20));
		}
	}
	if (!udp || socket->send_event || sent != frame->payload_len) {
		shell_print(sh, "+EVT+SEND=%X,%d", id, sent == frame->payload_len ? (int)sent :
			    k_uptime_get() >= deadline ? -111 : -107);
	}
}

struct winc_scan { const struct shell *sh; unsigned int count; };

static void scan_result(const char *ssid, int rssi, unsigned int channel,
			enum wifi_security_type security, const uint8_t *bssid, void *user)
{
	struct winc_scan *scan = user;
	int auth;
	char mac[18] = "";
	char name[WIFI_SSID_MAX_LEN + 1];
	size_t index = 0;

	if (!channel || channel > 14) {
		return;
	}
	switch (security) {
	case WIFI_SECURITY_TYPE_NONE: auth = 1; break;
	case WIFI_SECURITY_TYPE_PSK:
	case WIFI_SECURITY_TYPE_PSK_SHA256:
	case WIFI_SECURITY_TYPE_WPA_PSK: auth = 2; break;
	case WIFI_SECURITY_TYPE_WEP:
	case WIFI_SECURITY_TYPE_WEP_OPEN:
	case WIFI_SECURITY_TYPE_WEP_SHARED: auth = 3; break;
	case WIFI_SECURITY_TYPE_EAP: auth = 4; break;
	default: return;
	}
	if (bssid) {
		snprintk(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
			 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
	}
	for (; index < sizeof(name) - 1 && ssid[index]; ++index) {
		unsigned char c = ssid[index];
		name[index] = c < 32 || c == 127 ? '?' : c;
	}
	name[index] = '\0';
	shell_print(scan->sh, "+EVT+SCAN=%u,%d,%d,%u,%s,%s", scan->count++, rssi, auth,
		    channel, mac, name);
}

static int join(const struct shell *sh, char **args, unsigned int count)
{
	struct wt_wifi_ipv4_config config;
	const char *password = "";
	int channel = 0;
	int ret;

	if (count < 2 || count > 4) {
		return -103;
	}
	if (!strcmp(args[1], "2")) {
		if (count < 3 || !*args[2]) {
			return -103;
		}
		password = args[2];
		channel = count == 4 ? number(args[3], 14) : 0;
	} else if (!strcmp(args[1], "1") && count <= 3) {
		channel = count == 3 ? number(args[2], 14) : 0;
	} else {
		return -105;
	}
	if (channel < 0) {
		return -105;
	}
	wt_winc_close_all();
	ret = wt_wifi_station_join_channel(args[0], password, channel);
	if (ret) {
		return ret == -ETIMEDOUT ? -111 : ret == -EBUSY ? -254 : ret == -EINVAL ? -105 : -251;
	}
	if (wt_wifi_ipv4_config_get(&config)) {
		return -206;
	}
	station_connected = true;
	shell_print(sh, "+OK+CONN\n+EVT+DHCP=%s,%s,%s,%s", config.ip, config.subnet,
		    config.gateway, config.dns);
	return 0;
}

static int command(const struct shell *sh, const char *name, char **args,
		   unsigned int count, bool *echo)
{
	if (!strcmp(name, "CONN")) {
		return join(sh, args, count);
	}
	if (!strcmp(name, "SCAN")) {
		struct winc_scan scan = { .sh = sh };
		int channel = count == 1 ? number(args[0], 14) : 0;
		if (count > 1 || channel < 0) {
			return -105;
		}
		int ret = wt_wifi_scan_visit(channel, scan_result, &scan);
		if (ret) {
			return ret == -ETIMEDOUT ? -111 : -250;
		}
		shell_print(sh, "+OK+SCAN=%u", scan.count);
		return 0;
	}
	if (!strcmp(name, "TCPCLI") || !strcmp(name, "UDP")) {
		return open_socket(sh, name[0] == 'U', args, count);
	}
	if (!strcmp(name, "RESDNS")) {
		struct zsock_addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
		struct zsock_addrinfo *result;
		char address[16];

		if (count != 1 || !*args[0]) {
			return -103;
		}
		if (!wt_wifi_has_ipv4()) {
			return -206;
		}
		if (zsock_getaddrinfo(args[0], NULL, &hints, &result)) {
			return -107;
		}
		const struct sockaddr_in *peer = (const struct sockaddr_in *)result->ai_addr;
		zsock_inet_ntop(AF_INET, &peer->sin_addr, address, sizeof(address));
		zsock_freeaddrinfo(result);
		shell_print(sh, "+OK+RESDNS=%s,%s", args[0], address);
		return 0;
	}
	if (!strcmp(name, "CLOSE") || !strcmp(name, "UDPCFG") || !strcmp(name, "UDPCB")) {
		unsigned int expected = !strcmp(name, "CLOSE") ? 1 : !strcmp(name, "UDPCB") ? 2 : 3;
		if (count != expected) {
			return -103;
		}
		int id = existing_socket(args[0]);
		if (id < 0 || (expected > 1 && id < WINC_TCP_COUNT)) {
			return -300;
		}
		if (expected == 1) {
			close_socket(id);
		} else if (expected == 2) {
			int enable = number(args[1], 1);
			if (enable < 0) {
				return -105;
			}
			sockets[id].send_event = enable;
		} else if (parse_peer(&sockets[id].peer, args[1], args[2])) {
			return -301;
		}
	} else if (!strcmp(name, "ECHO")) {
		int mode = count == 1 ? number(args[0], 1) : -1;
		if (mode < 0) {
			return -105;
		}
		*echo = mode;
	} else if (!strcmp(name, "DHCP")) {
		int enable = count == 1 ? number(args[0], 1) : -1;

		if (enable != 1) {
			return enable == 0 ? -104 : -105;
		}
	} else if (count) {
		return -103;
	} else if (!strcmp(name, "VER")) {
		shell_print(sh, "+OK+VER=AT:" WT_WINC_AT_VERSION ",FW:WT02E40E");
		return 0;
	} else if (!strcmp(name, "LIST")) {
		shell_print(sh, "AT");
		for (size_t i = 0; i < ARRAY_SIZE(commands); ++i) {
			shell_print(sh, "AT+%s%s", commands[i].name, commands[i].args);
		}
		shell_print(sh, "+OK+LIST=%u", (unsigned int)ARRAY_SIZE(commands) + 1);
		return 0;
	} else if (!strcmp(name, "ECHO?")) {
		shell_print(sh, "+OK+ECHO=%d", *echo);
		return 0;
	} else if (!strcmp(name, "MODE")) {
		shell_print(sh, "+OK+MODE=%s", wt_wifi_is_associated() ? "STA" : "IDLE");
		return 0;
	} else if (!strcmp(name, "DISMODE")) {
		bool was_connected = wt_wifi_is_associated();
		if (wt_wifi_station_leave()) {
			return -107;
		}
		wt_winc_close_all();
		station_connected = false;
		shell_print(sh, "+OK+DISMODE");
		if (was_connected) {
			shell_print(sh, "+EVT+DISCONN");
		}
		return 0;
	} else if (!strcmp(name, "CLOSEALL")) {
		wt_winc_close_all();
	} else if (!strcmp(name, "GETMAC")) {
		char mac[18];
		if (wt_wifi_mac_get(mac, sizeof(mac))) {
			return -107;
		}
		shell_print(sh, "+OK+GETMAC=%s", mac);
		return 0;
	} else if (!strcmp(name, "GETIP")) {
		struct wt_wifi_ipv4_config config;
		if (wt_wifi_ipv4_config_get(&config)) {
			shell_print(sh, "+OK+GETIP=0.0.0.0");
		} else {
			shell_print(sh, "+OK+GETIP=%s,%s,%s,%s", config.ip, config.subnet,
				    config.gateway, config.dns);
		}
		return 0;
	} else if (!strcmp(name, "RSSI")) {
		struct wifi_iface_status status = { 0 };
		if (!wt_wifi_is_associated() || wt_wifi_station_status(&status)) {
			return -206;
		}
		shell_print(sh, "+OK+RSSI=%d", status.rssi);
		return 0;
	}
	shell_print(sh, "+OK+%s", name);
	return 0;
}

bool wt_winc_command(const struct shell *sh, char *line, bool *echo)
{
	char *args[4];
	char *name = line + 3;
	const char *equals;
	size_t len;

	if (strncasecmp(line, "AT+", 3)) {
		return false;
	}
	equals = strchr(name, '=');
	len = equals ? (size_t)(equals - name) : strlen(name);
	for (size_t index = 0; index < ARRAY_SIZE(commands); ++index) {
		const char *canonical = commands[index].name;
		if (strlen(canonical) != len || strncasecmp(canonical, name, len)) {
			continue;
		}
		unsigned int count = equals ? split_fields(name + len + 1, args, ARRAY_SIZE(args)) : 0;
		int ret = command(sh, canonical, args, count, echo);
		if (ret) {
			shell_print(sh, "+ERROR+%s:%d", canonical, ret);
		}
		return true;
	}
	return false;
}
