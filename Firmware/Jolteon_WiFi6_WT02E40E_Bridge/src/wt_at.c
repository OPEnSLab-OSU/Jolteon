/* ESP-AT and WINC1500 UART compatibility on the bridge's shell transport. */
#include <errno.h>
#include <string.h>
#include <strings.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/version.h>

#include "wt_app.h"
#include "wt_ble.h"
#include "wt_config.h"
#include "wt_wifi.h"
#include "wt_winc_at.h"

#define WINC_SYNC_IDLE_MS 250
#define WINC_FRAME_IDLE_MS 500
#define WINC_POLL_MS 40

/* Only the UART shell thread owns this state. */
static char at_line[256];
static size_t at_length;
static bool at_discard;
static bool at_echo;
static bool at_active;
static bool at_winc;
static bool at_service_enabled;
static bool sync_reply_pending;
static bool service_pending;
static bool frame_timeout_pending;
static atomic_t poll_due;
static bool stream_active;
static bool stream_discard;
static bool stream_end_prefix;
static struct wt_winc_frame stream_frame;
static int64_t stream_last;
static struct shell_transport_api uart_at_api;
static shell_transport_handler_t uart_handler;
static void *uart_context;
#if defined(CONFIG_SHELL_LOG_BACKEND)
static bool restore_logs;
#endif

extern struct shell_transport shell_transport_uart;
static void at_receive(const struct shell *sh, uint8_t *data, size_t length, void *user);

static void winc_poll(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	atomic_set(&poll_due, 1);
	uart_handler(SHELL_TRANSPORT_EVT_RX_RDY, uart_context);
}
K_TIMER_DEFINE(winc_timer, winc_poll, NULL);

static void enable_winc_service(void)
{
	if (!at_service_enabled) {
		at_service_enabled = true;
		k_timer_start(&winc_timer, K_MSEC(WINC_POLL_MS), K_MSEC(WINC_POLL_MS));
	}
}

/* Parse one ESP-AT quoted field in place, leaving the cursor at its delimiter. */
static char *quoted_arg(char **cursor)
{
	char *source = *cursor;
	char *value;
	char *dest;

	if (*source++ != '"') {
		return NULL;
	}
	value = dest = source;
	while (*source && *source != '"') {
		if (*source == '\\') {
			++source;
			if (*source != '\\' && *source != '"' && *source != ',') {
				return NULL;
			}
		}
		*dest++ = *source++;
	}
	if (*source != '"') {
		return NULL;
	}
	*cursor = source + 1;
	*dest = '\0';
	return value;
}

static void escape_field(const char *input, char *output, size_t size)
{
	while (*input && size > 2) {
		unsigned char c = *input++;

		if (c == '"' || c == ',' || c == '\\') {
			*output++ = '\\';
			--size;
		}
		*output++ = (c < 32 || c == 127) ? '?' : c;
		--size;
	}
	*output = '\0';
}

static int wifi_security_number(enum wifi_security_type security)
{
	switch (security) {
	case WIFI_SECURITY_TYPE_NONE: return 0;
	case WIFI_SECURITY_TYPE_WEP:
	case WIFI_SECURITY_TYPE_WEP_OPEN:
	case WIFI_SECURITY_TYPE_WEP_SHARED: return 1;
	case WIFI_SECURITY_TYPE_WPA_PSK: return 2;
	case WIFI_SECURITY_TYPE_PSK:
	case WIFI_SECURITY_TYPE_PSK_SHA256:
	case WIFI_SECURITY_TYPE_FT_PSK: return 3;
	case WIFI_SECURITY_TYPE_EAP: return 5;
	case WIFI_SECURITY_TYPE_SAE:
	case WIFI_SECURITY_TYPE_SAE_H2E:
	case WIFI_SECURITY_TYPE_SAE_AUTO:
	case WIFI_SECURITY_TYPE_FT_SAE: return 6;
	case WIFI_SECURITY_TYPE_WAPI: return 8;
	case WIFI_SECURITY_TYPE_DPP: return 13;
	default: return -1;
	}
}

static void format_mac(char mac[18], const uint8_t *addr)
{
	snprintk(mac, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
		 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_scan_entry(const char *ssid, int rssi, unsigned int channel,
			     enum wifi_security_type security, const uint8_t *bssid, void *user)
{
	const struct shell *sh = user;
	char escaped[2 * WIFI_SSID_MAX_LEN + 1];
	char mac[18] = "";

	escape_field(ssid, escaped, sizeof(escaped));
	if (bssid) {
		format_mac(mac, bssid);
	}
	shell_print(sh, "+CWLAP:(%d,\"%s\",%d,\"%s\",%u)",
		    wifi_security_number(security), escaped, rssi, mac, channel);
}

static int wifi_query(const struct shell *sh, bool join_query)
{
	struct wifi_iface_status status = { 0 };
	char ssid[2 * WIFI_SSID_MAX_LEN + 1] = "";
	bool requested = wt_wifi_is_requested();
	int state = 0;

	if (requested) {
		int ret = wt_wifi_station_status(&status);

		if (ret) {
			return ret;
		}
		if (wt_wifi_is_associated()) {
			state = wt_wifi_has_ipv4() ? 2 : 1;
		} else if (status.state >= WIFI_STATE_SCANNING && status.state <= WIFI_STATE_COMPLETED) {
			state = 3;
		} else {
			state = 4;
		}
		status.ssid[WIFI_SSID_MAX_LEN] = '\0';
		escape_field(status.ssid, ssid, sizeof(ssid));
	}
	if (!join_query) {
		shell_print(sh, "+CWSTATE:%d,\"%s\"", state, ssid);
		return 0;
	}
	if (state == 1 || state == 2) {
		char mac[18];

		format_mac(mac, (const uint8_t *)status.bssid);
		shell_print(sh, "+CWJAP:\"%s\",\"%s\",%u,%d", ssid, mac, status.channel, status.rssi);
	} else {
		shell_print(sh, "No AP");
	}
	return 0;
}

static int wifi_join(const struct shell *sh, char *args)
{
	char *ssid = quoted_arg(&args);
	char *password;
	int ret;

	if (!ssid || *args++ != ',') {
		return -EINVAL;
	}
	password = quoted_arg(&args);
	if (!password || *args) {
		return -EINVAL;
	}
	if (!wt_wifi_is_requested()) {
		return -ENODEV;
	}
	wt_winc_close_all();
	ret = wt_wifi_station_join(ssid, password);
	if (!ret) {
		shell_print(sh, "WIFI CONNECTED\nWIFI GOT IP");
	} else if (ret == -ETIMEDOUT) {
		shell_print(sh, "+CWJAP:1");
	} else {
		shell_print(sh, "+CWJAP:4");
	}
	return ret;
}

static int at_execute(const struct shell *sh, char *line)
{
	if (!strcmp(line, "AT")) {
		return 0;
	}
	if (!strcmp(line, "ATE0") || !strcmp(line, "ATE1")) {
		at_echo = line[3] == '1';
		return 0;
	}
	if (!strcmp(line, "AT+GMR")) {
		shell_print(sh, "AT version:WT02E40E subset 1\nSDK version:Zephyr " KERNEL_VERSION_STRING
			    "\nBin version:" WT_APP_FW_VERSION);
		return 0;
	}
	if (!strcmp(line, "AT+RST")) {
		return wt_app_reboot_schedule(false, WT_APP_REBOOT_DELAY_MS);
	}
	if (!strcmp(line, "AT+CWMODE?")) {
		shell_print(sh, "+CWMODE:%d", wt_wifi_is_requested() ? 1 : 0);
		return 0;
	}
	if (!strcmp(line, "AT+CWMODE=0") || !strcmp(line, "AT+CWMODE=1")) {
		if (line[10] == '0') {
			wt_winc_close_all();
		}
		return wt_wifi_service_set(line[10] == '1');
	}
	if (!strcmp(line, "AT+CWSTATE?") || !strcmp(line, "AT+CWJAP?")) {
		return wifi_query(sh, !strcmp(line, "AT+CWJAP?"));
	}
	if (!strncmp(line, "AT+CWJAP=", 9)) {
		return wifi_join(sh, line + 9);
	}
	if (!strcmp(line, "AT+CWQAP")) {
		wt_winc_close_all();
		return wt_wifi_station_leave();
	}
	if (!strcmp(line, "AT+CWLAP")) {
		return wt_wifi_is_requested() ? wt_wifi_scan_visit(0, print_scan_entry, (void *)sh) : -ENODEV;
	}
	if (!strcmp(line, "AT+CIFSR") || !strcmp(line, "AT+CIPSTAMAC?")) {
		char mac[18];
		char ip[16];
		int ret = wt_wifi_mac_get(mac, sizeof(mac));

		if (ret) {
			return ret;
		}
		if (!strcmp(line, "AT+CIFSR")) {
			wt_wifi_ipv4_get(ip, sizeof(ip));
			shell_print(sh, "+CIFSR:STAIP,\"%s\"\n+CIFSR:STAMAC,\"%s\"", ip, mac);
		} else {
			shell_print(sh, "+CIPSTAMAC:\"%s\"", mac);
		}
		return 0;
	}
	if (!strcmp(line, "AT+BLEINIT?")) {
		shell_print(sh, "+BLEINIT:%d", wt_ble_is_requested() && wt_ble_is_ready() ? 2 : 0);
		return 0;
	}
	if (!strcmp(line, "AT+BLEINIT=2")) {
		return wt_ble_peripheral_enable();
	}
	if (!strcmp(line, "AT+BLEINIT=0")) {
		return wt_ble_service_stop();
	}
	if (!strncmp(line, "AT+BLE", 6) && (!wt_ble_is_requested() || !wt_ble_is_ready())) {
		return -ENOTSUP;
	}
	if (!strcmp(line, "AT+BLEADVSTART") || !strcmp(line, "AT+BLEADVSTOP")) {
		return wt_ble_advertise_set(!strcmp(line, "AT+BLEADVSTART"));
	}
	if (!strcmp(line, "AT+BLEADDR?")) {
		char address[18];
		int ret = wt_ble_address_get(address, sizeof(address));

		if (!ret) {
			shell_print(sh, "+BLEADDR:\"%s\"", address);
		}
		return ret;
	}
	if (!strcmp(line, "AT+BLENAME?")) {
		char name[2 * WT_BLE_NAME_MAX + 1];

		escape_field(wt_ble_name_get(), name, sizeof(name));
		shell_print(sh, "+BLENAME:%s", name);
		return 0;
	}
	if (!strncmp(line, "AT+BLENAME=", 11)) {
		char *args = line + 11;
		char *name = quoted_arg(&args);

		/* The legacy scan response has 31 bytes, including two for the AD header. */
		return name && !*args && strlen(name) <= 29 ? wt_ble_name_set(name) : -EINVAL;
	}
	return -ENOEXEC;
}

static void enter_at(const struct shell *sh, bool winc)
{
	if (!at_active) {
#if defined(CONFIG_SHELL_LOG_BACKEND)
		restore_logs = sh->log_backend && log_backend_is_active(sh->log_backend->backend);
		if (restore_logs) {
			log_backend_deactivate(sh->log_backend->backend);
		}
#endif
		at_active = true;
	}
	memset(at_line, 0, sizeof(at_line));
	at_length = 0;
	at_discard = false;
	at_echo = false;
	at_winc = winc;
	stream_active = false;
	shell_set_bypass(sh, at_receive, NULL);
	if (winc) {
		enable_winc_service();
	}
}

static void line_reply(const struct shell *sh)
{
	int ret;

	if (at_discard) {
		shell_print(sh, "%s", at_winc ? "+ERROR+AT:-106" : "ERROR");
		return;
	}
	if (at_echo) {
		shell_print(sh, "%s", at_line);
	}
	if (at_winc && !strcasecmp(at_line, "AT")) {
		shell_print(sh, "+OK+AT=Ver:" WT_WINC_AT_VERSION);
		return;
	}
	if (wt_winc_command(sh, at_line, &at_echo)) {
		enable_winc_service();
		return;
	}
	ret = at_execute(sh, at_line);
	if (at_winc && ret == -ENOEXEC) {
		const char *name = !strncasecmp(at_line, "AT+", 3) ? at_line + 3 : "AT";
		size_t len = strspn(name, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

		shell_print(sh, "+ERROR+%.*s:-102", (int)MIN(len, 20), len ? name : "AT");
	} else {
		shell_print(sh, "%s", ret ? "ERROR" : "OK");
	}
}

static void stream_receive(const struct shell *sh, uint8_t c)
{
	enum wt_winc_frame_result result;

	stream_last = k_uptime_get();
	if (stream_discard) {
		if (stream_end_prefix && c == 'E') {
			stream_active = false;
		}
		stream_end_prefix = c == WT_WINC_SOH;
		return;
	}

	result = wt_winc_frame_feed(&stream_frame, c);
	switch (result) {
	case WT_WINC_FRAME:
		stream_active = false;
		wt_winc_stream_command(sh, &stream_frame);
		break;
	case WT_WINC_BAD_HEADER:
	case WT_WINC_BAD_DATA:
		shell_print(sh, "+ERROR+STREAM:%d", result == WT_WINC_BAD_HEADER ? -109 : -110);
		stream_discard = true;
		stream_end_prefix = c == WT_WINC_SOH;
		break;
	case WT_WINC_MORE:
		break;
	}
}

static void at_receive(const struct shell *sh, uint8_t *data, size_t length, void *user)
{
	ARG_UNUSED(user);
	if (sync_reply_pending) {
		sync_reply_pending = false;
		wt_winc_close_all();
		shell_print(sh, "+OK+AT=AT:" WT_WINC_AT_VERSION ",FW:WT02E40E,ChipID:5340,RFRev0");
		return;
	}
	if (service_pending) {
		service_pending = false;
		wt_winc_service(sh);
		return;
	}
	if (frame_timeout_pending) {
		frame_timeout_pending = false;
		shell_print(sh, "+ERROR+STREAM:-110");
		return;
	}
	for (size_t i = 0; i < length; ++i) {
		uint8_t c = data[i];

		if (stream_active) {
			stream_receive(sh, c);
		} else if (c == WT_WINC_SOH && !at_length && !at_discard) {
			memset(&stream_frame, 0, sizeof(stream_frame));
			stream_active = true;
			stream_discard = false;
			stream_end_prefix = false;
			stream_last = k_uptime_get();
		} else if (c == '\r' || c == '\n') {
			if (!at_length && !at_discard) {
				continue;
			}
			at_line[at_length] = '\0';
			if (!at_discard && !(at_winc ? strcasecmp(at_line, "ATO") : strcmp(at_line, "ATO"))) {
				memset(at_line, 0, sizeof(at_line));
				wt_winc_close_all();
				shell_print(sh, "OK");
#if defined(CONFIG_SHELL_LOG_BACKEND)
				if (restore_logs) {
					log_backend_activate(sh->log_backend->backend, (void *)sh);
				}
#endif
				at_active = false;
				at_winc = false;
				at_service_enabled = false;
				k_timer_stop(&winc_timer);
				atomic_clear(&poll_due);
				shell_set_bypass(sh, NULL, NULL);
				return;
			}
			line_reply(sh);
			memset(at_line, 0, sizeof(at_line));
			at_length = 0;
			at_discard = false;
		} else if (c < 32 || c == 127 || at_length == sizeof(at_line) - 1) {
			at_discard = true;
		} else if (!at_discard) {
			at_line[at_length++] = c;
		}
	}
}

/* Check for WINC startup before the native shell consumes binary control
 * characters. Take one byte per read so state changes never swallow a chunk.
 */
static int uart_at_read(const struct shell_transport *transport, void *buffer, size_t len,
			size_t *count)
{
	static uint8_t pending[WT_WINC_SYNC_LENGTH];
	static size_t matched, replay_pos, replay_end;
	static int64_t sync_last;
	uint8_t *data = buffer;

	*count = 0;
	if (!len) {
		return 0;
	}
	if (sync_reply_pending) {
		*data = '\n'; /* Schedule the response in the unlocked bypass callback. */
		*count = 1;
		return 0;
	}
	if (replay_pos < replay_end) {
		*data = pending[replay_pos++];
		*count = 1;
		return 0;
	}
	for (;;) {
		int ret = shell_uart_transport_api.read(transport, data, 1, count);
		int64_t now = k_uptime_get();
		size_t previous = matched;

		if (ret) {
			return ret;
		}
		if (stream_active && now - stream_last > WINC_FRAME_IDLE_MS) {
			stream_active = false;
			if (!stream_discard && !*count) {
				*data = '\n';
				*count = 1;
				frame_timeout_pending = true;
				return 0;
			}
		}
		if (!*count) {
			if (at_service_enabled && !stream_active && !at_length && !at_discard &&
			    atomic_cas(&poll_due, 1, 0)) {
				*data = '\n';
				*count = 1;
				service_pending = true;
			}
			return 0;
		}
		if (stream_active) {
			return 0;
		}
		if (matched && now - sync_last > WINC_SYNC_IDLE_MS) {
			matched = 0;
			previous = 0;
		}
		sync_last = now;
		switch (wt_winc_sync_feed(&matched, *data)) {
		case WT_WINC_SYNC:
			enter_at(shell_backend_uart_get_ptr(), true);
			sync_reply_pending = true;
			uart_handler(SHELL_TRANSPORT_EVT_RX_RDY, uart_context);
			*count = 0;
			return 0;
		case WT_WINC_PREFIX:
			if (!previous || matched > 1) {
				pending[matched - 1] = *data;
				break;
			}
			/* A second A5 starts a new prefix. The old one was not sync. */
			replay_end = previous;
			replay_pos = 1;
			*data = pending[0];
			return 0;
		case WT_WINC_TEXT:
			if (previous) {
				pending[previous] = *data;
				replay_end = previous + 1;
				replay_pos = 1;
				*data = pending[0];
			}
			return 0;
		}
	}
}

static int uart_at_init(const struct shell_transport *transport, const void *config,
			shell_transport_handler_t handler, void *context)
{
	uart_handler = handler;
	uart_context = context;
	return shell_uart_transport_api.init(transport, config, handler, context);
}

static int uart_at_install(void)
{
	uart_at_api = shell_uart_transport_api;
	uart_at_api.init = uart_at_init;
	uart_at_api.read = uart_at_read;
	shell_transport_uart.api = &uart_at_api;
	return 0;
}
SYS_INIT(uart_at_install, PRE_KERNEL_2, 50);

static int at_start(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	if (sh != shell_backend_uart_get_ptr()) {
		shell_print(sh, "ERROR");
		return -ENOTSUP;
	}
	enter_at(sh, false);
	shell_print(sh, "OK");
	return 0;
}

SHELL_CMD_ARG_REGISTER(AT, NULL, "Enter UART AT mode. ATO returns to the WT shell.", at_start, 1, 0);
