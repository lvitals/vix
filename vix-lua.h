#ifndef VIX_LUA_H
#define VIX_LUA_H

#if CONFIG_LUA
#define LUA_COMPAT_5_1
#define LUA_COMPAT_5_2
#define LUA_COMPAT_5_3
#define LUA_COMPAT_ALL
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#else
typedef struct lua_State lua_State;
typedef void* lua_CFunction;
#endif

#include "vix.h"
#include "vix-subprocess.h"

/* add a directory to consider when loading lua files */
VIX_INTERNAL bool vix_lua_path_add(Vix*, const char *path);
/* get semicolon separated list of paths to load lua files
 * (*lpath = package.path) and Lua C modules (*cpath = package.cpath)
 * both these pointers need to be free(3)-ed by the caller */
VIX_INTERNAL bool vix_lua_paths_get(Vix*, char **lpath, char **cpath);

/* various event handlers, triggered by the vix core */
#if !CONFIG_LUA
#define vix_event_mode_insert_input  vix_insert_key
#define vix_event_mode_replace_input vix_replace_key
#else
VIX_INTERNAL void vix_event_mode_insert_input(Vix*, const char *key, size_t len);
VIX_INTERNAL void vix_event_mode_replace_input(Vix*, const char *key, size_t len);
#endif
VIX_INTERNAL void vix_lua_process_response(Vix *, const char *, char *, size_t, ResponseType);

#endif
