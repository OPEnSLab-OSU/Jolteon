#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wt_winc_wire.h"

static const uint8_t start[] = {
	0xa5, 0x01, 0xf2, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00, 0x61, 0x2f, 0x52, 0xef,
};

static void test_sync(void)
{
	size_t matched = 0;

	assert(wt_winc_sync_feed(&matched, 'A') == WT_WINC_TEXT);
	assert(wt_winc_sync_feed(&matched, start[0]) == WT_WINC_PREFIX);
	assert(wt_winc_sync_feed(&matched, start[1]) == WT_WINC_PREFIX);
	assert(wt_winc_sync_feed(&matched, 'T') == WT_WINC_TEXT);
	assert(matched == 0);
	assert(wt_winc_sync_feed(&matched, start[0]) == WT_WINC_PREFIX);
	assert(wt_winc_sync_feed(&matched, start[1]) == WT_WINC_PREFIX);
	for (unsigned int pass = 0; pass < 2; ++pass) {
		for (size_t i = 0; i + 1 < sizeof(start); ++i) {
			assert(wt_winc_sync_feed(&matched, start[i]) == WT_WINC_PREFIX);
			assert(matched == i + 1);
		}
		assert(wt_winc_sync_feed(&matched, start[sizeof(start) - 1]) == WT_WINC_SYNC);
		assert(matched == 0);
	}
}

static void feed_more(struct wt_winc_frame *frame, const uint8_t *bytes, size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		assert(wt_winc_frame_feed(frame, bytes[i]) == WT_WINC_MORE);
	}
}

static void test_send(void)
{
	struct wt_winc_frame frame = { 0 };
	const uint8_t header[] = "S00006";
	const uint8_t payload[] = { 0, 0xff, 1, 'E', '\r', '\n' };

	feed_more(&frame, header, sizeof(header) - 1);
	feed_more(&frame, payload, sizeof(payload));
	assert(wt_winc_frame_feed(&frame, WT_WINC_SOH) == WT_WINC_MORE);
	assert(wt_winc_frame_feed(&frame, 'E') == WT_WINC_FRAME);
	assert(frame.socket == 0 && frame.directive == 'S' && frame.payload_len == sizeof(payload));
	assert(!memcmp(frame.data + frame.payload_start, payload, sizeof(payload)));
}

static void test_udp_at_limit(void)
{
	struct wt_winc_frame frame = { 0 };
	const uint8_t header[] = "UA1400,255.255.255.255,65535,";

	feed_more(&frame, header, sizeof(header) - 1);
	for (size_t i = 0; i < WT_WINC_PAYLOAD_MAX; ++i) {
		assert(wt_winc_frame_feed(&frame, (uint8_t)i) == WT_WINC_MORE);
	}
	assert(wt_winc_frame_feed(&frame, WT_WINC_SOH) == WT_WINC_MORE);
	assert(wt_winc_frame_feed(&frame, 'E') == WT_WINC_FRAME);
	assert(frame.socket == 10 && frame.directive == 'U' && frame.payload_len == WT_WINC_PAYLOAD_MAX);
	assert(frame.payload_start == sizeof(header) - 1);
}

static void test_receive_and_invalid_frames(void)
{
	struct wt_winc_frame frame = { 0 };

	feed_more(&frame, (const uint8_t *)"R7\x01", 3);
	assert(wt_winc_frame_feed(&frame, 'E') == WT_WINC_FRAME);
	assert(frame.socket == 7 && frame.directive == 'R' && frame.payload_len == 0);
	memset(&frame, 0, sizeof(frame));
	feed_more(&frame, (const uint8_t *)"R", 1);
	assert(wt_winc_frame_feed(&frame, 'B') == WT_WINC_BAD_HEADER);
	memset(&frame, 0, sizeof(frame));
	feed_more(&frame, (const uint8_t *)"S0140", 5);
	assert(wt_winc_frame_feed(&frame, '1') == WT_WINC_BAD_HEADER);
	memset(&frame, 0, sizeof(frame));
	feed_more(&frame, (const uint8_t *)"U70001", 6);
	assert(wt_winc_frame_feed(&frame, '1') == WT_WINC_BAD_HEADER);
	memset(&frame, 0, sizeof(frame));
	feed_more(&frame, (const uint8_t *)"S00001X\x01", 8);
	assert(wt_winc_frame_feed(&frame, 'X') == WT_WINC_BAD_DATA);
}

int main(void)
{
	test_sync();
	test_send();
	test_udp_at_limit();
	test_receive_and_invalid_frames();
	puts("WINC UART sync and binary framing tests passed");
	return 0;
}
