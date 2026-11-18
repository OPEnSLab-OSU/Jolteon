#ifndef WT_WINC_AT_H
#define WT_WINC_AT_H

#include <stdbool.h>
#include <zephyr/shell/shell.h>

#include "wt_winc_wire.h"

#define WT_WINC_AT_VERSION "0.1.0"

bool wt_winc_command(const struct shell *sh, char *line, bool *echo);
void wt_winc_stream_command(const struct shell *sh, const struct wt_winc_frame *frame);
void wt_winc_close_all(void);
void wt_winc_service(const struct shell *sh);

#endif
