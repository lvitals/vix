#include "vix-core.h"

#if !CONFIG_LUA
bool vix_event_emit(Vix *vix, enum VixEvents id, ...) {
	va_list ap;
	va_start(ap, id);

	if (id == VIX_EVENT_WIN_STATUS) {
		Win *win = va_arg(ap, Win*);
		window_status_update(vix, win);
	}

	va_end(ap);
	return true;
}
#endif
