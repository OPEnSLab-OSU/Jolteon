#include "wt_winc_wire.h"

enum wt_winc_sync_result wt_winc_sync_feed(size_t *matched, uint8_t byte)
{
	static const uint8_t start[WT_WINC_SYNC_LENGTH] = {0xa5, 0x01, 0xf2, 0x00, 0x00, 0x0c, 0x00,
					0x0c, 0x00, 0x61, 0x2f, 0x52, 0xef};

	if (byte != start[*matched]) {
		*matched = byte == start[0] ? 1 : 0;
		return *matched ? WT_WINC_PREFIX : WT_WINC_TEXT;
	}
	if (++*matched == sizeof(start)) {
		*matched = 0;
		return WT_WINC_SYNC;
	}
	return WT_WINC_PREFIX;
}

int wt_winc_socket_digit(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	return c == 'A' || c == 'a' ? 10 : -1;
}

/* Consume the declared length before looking for SOH E. Both bytes are
 * legal inside a payload. The caller resets the frame on error or timeout.
 */
enum wt_winc_frame_result wt_winc_frame_feed(struct wt_winc_frame *frame, uint8_t byte)
{
	size_t pos = frame->used;

	if (pos == sizeof(frame->data)) {
		return WT_WINC_BAD_DATA;
	}
	frame->data[frame->used++] = byte;
	if (pos == 0) {
		frame->directive = byte;
		return byte == 'S' || byte == 'U' || byte == 'R' ? WT_WINC_MORE : WT_WINC_BAD_HEADER;
	}
	if (pos == 1) {
		frame->socket = wt_winc_socket_digit(byte);
		if (frame->socket < 0) {
			return WT_WINC_BAD_HEADER;
		}
		if (frame->directive == 'R') {
			frame->payload_start = frame->used;
		}
		return WT_WINC_MORE;
	}
	if (frame->directive != 'R' && pos < 6) {
		if (byte < '0' || byte > '9') {
			return WT_WINC_BAD_HEADER;
		}
		frame->payload_len = frame->payload_len * 10 + byte - '0';
		if (pos == 5) {
			if (!frame->payload_len || frame->payload_len > WT_WINC_PAYLOAD_MAX) {
				return WT_WINC_BAD_HEADER;
			}
			if (frame->directive == 'S') {
				frame->payload_start = frame->used;
			}
		}
		return WT_WINC_MORE;
	}
	if (!frame->payload_start) {
		if (pos >= 30 || (pos == 6 && byte != ',') ||
		    (byte != ',' && byte != '.' && (byte < '0' || byte > '9'))) {
			return WT_WINC_BAD_HEADER;
		}
		if (byte == ',' && ++frame->commas == 3) {
			frame->payload_start = frame->used;
		}
		return WT_WINC_MORE;
	}

	size_t end = frame->payload_start + frame->payload_len;
	if (pos < end) {
		return WT_WINC_MORE;
	}
	if (pos == end) {
		return byte == WT_WINC_SOH ? WT_WINC_MORE : WT_WINC_BAD_DATA;
	}
	return byte == 'E' ? WT_WINC_FRAME : WT_WINC_BAD_DATA;
}
