#ifndef WT_WINC_WIRE_H
#define WT_WINC_WIRE_H

#include <stddef.h>
#include <stdint.h>

#define WT_WINC_SOH 0x01
#define WT_WINC_ETB 0x17
#define WT_WINC_SYNC_LENGTH 13
#define WT_WINC_PAYLOAD_MAX 1400

enum wt_winc_sync_result { WT_WINC_TEXT, WT_WINC_PREFIX, WT_WINC_SYNC };
enum wt_winc_frame_result { WT_WINC_MORE, WT_WINC_FRAME, WT_WINC_BAD_HEADER, WT_WINC_BAD_DATA };

struct wt_winc_frame {
	uint8_t data[WT_WINC_PAYLOAD_MAX + 32];
	size_t used;
	size_t payload_start;
	size_t payload_len;
	unsigned int commas;
	char directive;
	int socket;
};

enum wt_winc_sync_result wt_winc_sync_feed(size_t *matched, uint8_t byte);
enum wt_winc_frame_result wt_winc_frame_feed(struct wt_winc_frame *frame, uint8_t byte);
int wt_winc_socket_digit(char c);

#endif
