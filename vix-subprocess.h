#ifndef VIX_SUBPROCESS_H
#define VIX_SUBPROCESS_H
#include "vix-core.h"
#include "vix-lua.h"

typedef struct Process Process;
#if CONFIG_LUA
typedef int Invalidator(lua_State*);
#else
typedef void Invalidator;
#endif

struct Process {
	char *name;
	int outfd;
	int errfd;
	int inpfd;
	pid_t pid;
	Invalidator** invalidator;
	Process *next;
};

typedef enum { STDOUT, STDERR, SIGNAL, EXIT } ResponseType;

VIX_INTERNAL Process *vix_process_communicate(Vix *, const char *command, const char *name,
                                              Invalidator **invalidator);
VIX_INTERNAL int vix_process_before_tick(fd_set *);
VIX_INTERNAL void vix_process_tick(Vix *, fd_set *);
VIX_INTERNAL void vix_process_waitall(Vix *);
#endif
