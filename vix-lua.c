#include <ctype.h>
#include "util.h"

#include "vix-lua.h"
#include "vix-core.h"
#include "text-motions.h"

#ifndef VIX_PATH
#define VIX_PATH "/usr/local/share/vix"
#endif

#define VIX_LUA_TYPE_VIX "vix"
#define VIX_LUA_TYPE_WIN_OPTS "winoptions"
#define VIX_LUA_TYPE_VIX_OPTS "vixoptions"
#define VIX_LUA_TYPE_FILE "file"
#define VIX_LUA_TYPE_TEXT "text"
#define VIX_LUA_TYPE_MARK "mark"
#define VIX_LUA_TYPE_MARKS "marks"
#define VIX_LUA_TYPE_WINDOW "window"
#define VIX_LUA_TYPE_SELECTION "selection"
#define VIX_LUA_TYPE_SELECTIONS "selections"
#define VIX_LUA_TYPE_UI "ui"
#define VIX_LUA_TYPE_REGISTERS "registers"
#define VIX_LUA_TYPE_KEYACTION "keyaction"

#ifndef DEBUG_LUA
#define DEBUG_LUA 0
#endif

#if DEBUG_LUA
#define debug(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define debug(...) do { } while (0)
#endif


#if CONFIG_LUA
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 502

#define LUA_OK 0

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++) { /* copy upvalues to the top */
			lua_pushvalue(L, -nup);
		}
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}

#define lua_getuservalue(L, i) lua_getfenv(L, i)
#define lua_setuservalue(L, i) lua_setfenv(L, i)
#define luaL_setmetatable(L, t) \
	do { luaL_getmetatable(L, t); lua_setmetatable(L, -2); } while (0)

static void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level) {
	lua_getglobal(L, "debug");
	lua_getfield(L, -1, "traceback");
	lua_remove(L, -2);
	if (L1 != L) {
		lua_pushthread(L1);
	}
	lua_pushstring(L, msg);
	lua_pushinteger(L, level + (L1 == L ? 1 : 0));
	lua_call(L, (L1 == L ? 2 : 3), 1);
}

typedef struct luaL_Stream {
  FILE *f;
  lua_CFunction closef;
} luaL_Stream;

#define LUA_FILEHANDLE "FILE*"

static int luaL_fileresult(lua_State *L, int stat, const char *fname) {
	if (stat) {
		lua_pushboolean(L, 1);
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
}

#endif
#endif

#if !CONFIG_LUA

bool vix_lua_path_add(Vix *vix, const char *path) { return true; }
bool vix_lua_paths_get(Vix *vix, char **lpath, char **cpath) { return false; }
void vix_lua_process_response(Vix *vix, const char *name,
                              char *buffer, size_t len, ResponseType rtype) { }

#else

#if DEBUG_LUA
static void stack_dump_entry(lua_State *L, int i) {
	int t = lua_type(L, i);
	switch (t) {
	case LUA_TNIL:
		printf("nil");
		break;
	case LUA_TBOOLEAN:
		printf(lua_toboolean(L, i) ? "true" : "false");
		break;
	case LUA_TLIGHTUSERDATA:
		printf("lightuserdata(%p)", lua_touserdata(L, i));
		break;
	case LUA_TNUMBER:
		printf("%g", lua_tonumber(L, i));
		break;
	case LUA_TSTRING:
		printf("`%s'", lua_tostring(L, i));
		break;
	case LUA_TTABLE:
		printf("table[");
		lua_pushnil(L); /* first key */
		while (lua_next(L, i > 0 ? i : i - 1)) {
			stack_dump_entry(L, -2);
			printf("=");
			stack_dump_entry(L, -1);
			printf(",");
			lua_pop(L, 1); /* remove value, keep key */
		}
		printf("]");
		break;
	case LUA_TUSERDATA:
		printf("userdata(%p)", lua_touserdata(L, i));
		break;
	default:  /* other values */
		printf("%s", lua_typename(L, t));
		break;
	}
}

static void stack_dump(lua_State *L, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
	int top = lua_gettop(L);
	for (int i = 1; i <= top; i++) {
		printf("%d: ", i);
		stack_dump_entry(L, i);
		printf("\n");
	}
	printf("\n\n");
	fflush(stdout);
}

#endif

static int panic_handler(lua_State *L) {
	void *ud = NULL;
	lua_getallocf(L, &ud);
	if (ud) {
		Vix *vix = ud;
		vix->lua = NULL;
		const char *msg = NULL;
		if (lua_type(L, -1) == LUA_TSTRING) {
			msg = lua_tostring(L, -1);
		}
		vix_info_show(vix, "Fatal Lua error: %s", msg ? msg : "unknown reason");
		lua_close(L);
		if (vix->running) {
			siglongjmp(vix->sigbus_jmpbuf, 1);
		}
	}
	return 0;
}

static int error_handler(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	if (vix->errorhandler) {
		return 1;
	}
	vix->errorhandler = true;
	size_t len;
	const char *msg = lua_tostring(L, 1);
	if (msg) {
		luaL_traceback(L, L, msg, 1);
	}
	msg = lua_tolstring(L, 1, &len);
	vix_message_show(vix, msg);
	vix->errorhandler = false;
	return 1;
}

static int pcall(Vix *vix, lua_State *L, int nargs, int nresults) {
	/* insert a custom error function below all arguments */
	int msgh = lua_gettop(L) - nargs;
	lua_pushlightuserdata(L, vix);
	lua_pushcclosure(L, error_handler, 1);
	lua_insert(L, msgh);
	int ret = lua_pcall(L, nargs, nresults, msgh);
	lua_remove(L, msgh);
	return ret;
}

/* expects a lua function at stack position `narg` and stores a
 * reference to it in the registry. The return value can be used
 * to look it up.
 *
 *   registry["vix.functions"][(void*)(function)] = function
 */
static const void *func_ref_new(lua_State *L, int narg) {
	const void *addr = lua_topointer(L, narg);
	if (!lua_isfunction(L, narg) || !addr) {
		luaL_argerror(L, narg, "function expected");
	}
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.functions");
	lua_pushlightuserdata(L, (void*)addr);
	lua_pushvalue(L, narg);
	lua_settable(L, -3);
	lua_pop(L, 1);
	return addr;
}

/* retrieve function from registry and place it at the top of the stack */
static bool func_ref_get(lua_State *L, const void *addr) {
	if (!addr) {
		return false;
	}
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.functions");
	lua_pushlightuserdata(L, (void*)addr);
	lua_gettable(L, -2);
	lua_remove(L, -2);
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 1);
		return false;
	}
	return true;
}

/* creates a new metatable for a given type and stores a mapping:
 *
 *   registry["vix.types"][metatable] = type
 *
 * leaves the metatable at the top of the stack.
 * IMPORTANT: expects 0 terminated type
 */
static void obj_type_new(lua_State *L, str8 type)
{
	luaL_newmetatable(L, (char *)type.data);
	lua_getglobal(L, "vix");
	if (!lua_isnil(L, -1)) {
		lua_getfield(L, -1, "types");
		lua_pushvalue(L, -3);
		lua_setfield(L, -2, (char *)type.data);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.types");
	lua_pushvalue(L, -2);
	lua_pushlstring(L, (char *)type.data, type.length);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

/* get type of userdatum at the top of the stack:
 *
 *   return registry["vix.types"][getmetatable(userdata)]
 */
const char *obj_type_get(lua_State *L) {
	if (lua_isnil(L, -1)) {
		return "nil";
	}
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.types");
	lua_getmetatable(L, -2);
	lua_gettable(L, -2);
	// XXX: in theory string might become invalid when popped from stack
	const char *type = lua_tostring(L, -1);
	lua_pop(L, 2);
	return type;
}

static void *obj_new(lua_State *L, size_t size, const char *type) {
	void *obj = lua_newuserdata(L, size);
	luaL_getmetatable(L, type);
	lua_setmetatable(L, -2);
	lua_newtable(L);
	lua_setuservalue(L, -2);
	return obj;
}

/* returns registry["vix.objects"][addr] if it is of correct type */
static void *obj_ref_get(lua_State *L, void *addr, const char *type) {
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.objects");
	lua_pushlightuserdata(L, addr);
	lua_gettable(L, -2);
	lua_remove(L, -2);
	if (lua_isnil(L, -1)) {
		debug("get: vix.objects[%p] = nil\n", addr);
		lua_pop(L, 1);
		return NULL;
	}
	if (DEBUG_LUA) {
		const char *actual_type = obj_type_get(L);
		if (strcmp(type, actual_type) != 0) {
			debug("get: vix.objects[%p] = %s (BUG: expected %s)\n", addr, actual_type, type);
		}
		void **handle = luaL_checkudata(L, -1, type);
		if (!handle) {
			debug("get: vix.objects[%p] = %s (BUG: invalid handle)\n", addr, type);
		} else if (*handle != addr) {
			debug("get: vix.objects[%p] = %s (BUG: handle mismatch %p)\n", addr, type, *handle);
		}
	}
	/* verify that obj is correct type then unmodify the stack */
	luaL_checkudata(L, -1, type);
	lua_pop(L, 1);
	return addr;
}

/* expects a userdatum at the top of the stack and sets
 *
 *   registry["vix.objects"][addr] = userdata
 */
static void obj_ref_set(lua_State *L, void *addr) {
	//debug("set: vix.objects[%p] = %s\n", addr, obj_type_get(L));
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.objects");
	lua_pushlightuserdata(L, addr);
	lua_pushvalue(L, -3);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

/* invalidates an object reference
 *
 *   registry["vix.objects"][addr] = nil
 */
static void obj_ref_free(lua_State *L, void *addr) {
	if (DEBUG_LUA) {
		lua_getfield(L, LUA_REGISTRYINDEX, "vix.objects");
		lua_pushlightuserdata(L, addr);
		lua_gettable(L, -2);
		lua_remove(L, -2);
		if (lua_isnil(L, -1)) {
			debug("free-unused: %p\n", addr);
		} else {
			debug("free: vix.objects[%p] = %s\n", addr, obj_type_get(L));
		}
		lua_pop(L, 1);
	}
	lua_pushnil(L);
	obj_ref_set(L, addr);
}

/* creates a new object reference of given type if it does not already exist in the registry:
 *
 *  if (registry["vix.types"][metatable(registry["vix.objects"][addr])] != type) {
 *      // XXX: should not happen
 *      registry["vix.objects"][addr] = new_obj(addr, type)
 *  }
 *  return registry["vix.objects"][addr];
 */
static void *obj_ref_new(lua_State *L, void *addr, const char *type) {
	if (!addr) {
		lua_pushnil(L);
		return NULL;
	}
	lua_getfield(L, LUA_REGISTRYINDEX, "vix.objects");
	lua_pushlightuserdata(L, addr);
	lua_gettable(L, -2);
	lua_remove(L, -2);
	const char *old_type = obj_type_get(L);
	if (strcmp(type, old_type) == 0) {
		debug("new: vix.objects[%p] = %s (returning existing object)\n", addr, old_type);
		void **handle = luaL_checkudata(L, -1, type);
		if (!handle) {
			debug("new: vix.objects[%p] = %s (BUG: invalid handle)\n", addr, old_type);
		} else if (*handle != addr) {
			debug("new: vix.objects[%p] = %s (BUG: handle mismatch %p)\n", addr, old_type, *handle);
		}
		return addr;
	}
	if (!lua_isnil(L, -1)) {
		debug("new: vix.objects[%p] = %s (WARNING: changing object type from %s)\n", addr, type, old_type);
	} else {
		debug("new: vix.objects[%p] = %s (creating new object)\n", addr, type);
	}
	lua_pop(L, 1);
	void **handle = obj_new(L, sizeof(addr), type);
	obj_ref_set(L, addr);
	*handle = addr;
	return addr;
}

/* (type) check validity of object reference at stack location `idx' and retrieve it */
static void *obj_ref_check(lua_State *L, int idx, const char *type) {
	void **addr = luaL_checkudata(L, idx, type);
	if (!obj_ref_get(L, *addr, type)) {
		luaL_argerror(L, idx, "invalid object reference");
	}
	return *addr;
}

static void *obj_ref_check_containerof(lua_State *L, int idx, const char *type, size_t offset) {
	void *obj = obj_ref_check(L, idx, type);
	return obj ? ((char*)obj-offset) : obj;
}

static void *obj_lightref_new(lua_State *L, void *addr, const char *type) {
	if (!addr) {
		return NULL;
	}
	void **handle = obj_new(L, sizeof(addr), type);
	*handle = addr;
	return addr;
}

static void *obj_lightref_check(lua_State *L, int idx, const char *type) {
	void **addr = luaL_checkudata(L, idx, type);
	return *addr;
}

static Vix *lua_get_vix(lua_State *L)
{
	lua_getglobal(L, "vix");
	Vix *result = luaL_checkudata(L, -1, "vix");
	return result;
}

static int index_common(lua_State *L) {
	lua_getmetatable(L, 1);
	lua_pushvalue(L, 2);
	lua_gettable(L, -2);
	if (lua_isnil(L, -1)) {
		lua_getuservalue(L, 1);
		lua_pushvalue(L, 2);
		lua_gettable(L, -2);
	}
	return 1;
}

static int newindex_common(lua_State *L) {
	lua_getuservalue(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, 3);
	lua_settable(L, -3);
	return 0;
}

static size_t getpos(lua_State *L, int narg) {
	return (size_t)lua_tointeger(L, narg);
}

static size_t checkpos(lua_State *L, int narg) {
	lua_Number n = luaL_checknumber(L, narg);
	/* on most systems SIZE_MAX can't be represented in lua_Number.
	 * using < avoids undefined behaviour when n == SIZE_MAX+1
	 * which can be represented in lua_Number
	 */
	if (n >= 0 && n < (lua_Number)SIZE_MAX && n == (size_t)n) {
		return n;
	}
	return luaL_argerror(L, narg, "expected position, got number");
}

static void pushpos(lua_State *L, size_t pos) {
	if (pos == EPOS) {
		lua_pushnil(L);
	} else {
		lua_pushinteger(L, pos);
	}
}

static void pushrange(lua_State *L, Filerange *r) {
	if (!r || !text_range_valid(r)) {
		lua_pushnil(L);
		return;
	}
	lua_createtable(L, 0, 2);
	lua_pushliteral(L, "start");
	lua_pushinteger(L, r->start);
	lua_settable(L, -3);
	lua_pushliteral(L, "finish");
	lua_pushinteger(L, r->end);
	lua_settable(L, -3);
}

static Filerange getrange(lua_State *L, int index) {
	Filerange range = text_range_empty();
	if (lua_istable(L, index)) {
		lua_getfield(L, index, "start");
		range.start = checkpos(L, -1);
		lua_pop(L, 1);
		lua_getfield(L, index, "finish");
		range.end = checkpos(L, -1);
		lua_pop(L, 1);
	} else {
		range.start = checkpos(L, index);
		range.end = range.start + checkpos(L, index+1);
	}
	return range;
}

static const char *keymapping(Vix *vix, const char *keys, const Arg *arg) {
	lua_State *L = vix->lua;
	if (!func_ref_get(L, arg->v)) {
		return keys;
	}
	lua_pushstring(L, keys);
	if (pcall(vix, L, 1, 1) != 0) {
		return keys;
	}
	if (lua_type(L, -1) != LUA_TNUMBER) {
		return keys; /* invalid or no return value, assume zero */
	}
	lua_Number number = lua_tonumber(L, -1);
	lua_Integer integer = lua_tointeger(L, -1);
	if (number != integer) {
		return keys;
	}
	if (integer < 0) {
		return NULL; /* need more input */
	}
	size_t len = integer;
	size_t max = strlen(keys);
	return (len <= max) ? keys+len : keys;
}

/***
 * The main editor object.
 * @type Vix
 */

/***
 * Version information.
 * @tfield string VERSION
 * version information in `git describe` format, same as reported by `vix -v`.
 */
/***
 * Lua API object types
 * @field types meta tables of userdata objects used for type checking
 * @local
 */
/***
 * User interface.
 * @tfield Ui ui the user interface being used
 */
/***
 * Mode constants.
 * @tfield modes modes
 */
/***
 * Events.
 * @tfield events events
 */
/***
 * Registers.
 * @field registers array to access the register by single letter name
 */
/***
 * Scintillua lexer module.
 * @field lexers might be `nil` if module is not found
 */
/***
 * LPeg lexer module.
 * @field lpeg might be `nil` if module is not found
 */
/***
 * Current count.
 * @tfield int count the specified count for the current command or `nil` if none was given
 */

/***
 * Create an iterator over all windows.
 * @function windows
 * @return the new iterator
 * @see win
 * @usage
 * for win in vix:windows() do
 * 	-- do something with win
 * end
 */
static int windows_iter(lua_State *L);
static int windows(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	Win **handle = lua_newuserdata(L, sizeof *handle), *next;
	for (next = vix->windows; next && next->file->internal; next = next->next);
	*handle = next;
	lua_pushcclosure(L, windows_iter, 1);
	return 1;
}

static int windows_iter(lua_State *L) {
	Win **handle = lua_touserdata(L, lua_upvalueindex(1));
	if (!*handle) {
		return 0;
	}
	Win *win = obj_ref_new(L, *handle, VIX_LUA_TYPE_WINDOW), *next;
	if (win) {
		for (next = win->next; next && next->file->internal; next = next->next);
		*handle = next;
	}
	return 1;
}

/***
 * Create an iterator over all files.
 * @function files
 * @return the new iterator
 * @usage
 * for file in vix:files() do
 * 	-- do something with file
 * end
 */
static int files_iter(lua_State *L);
static int files(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	File **handle = lua_newuserdata(L, sizeof *handle);
	*handle = vix->files;
	lua_pushcclosure(L, files_iter, 1);
	return 1;
}

static int files_iter(lua_State *L) {
	File **handle = lua_touserdata(L, lua_upvalueindex(1));
	if (!*handle) {
		return 0;
	}
	File *file = obj_ref_new(L, *handle, VIX_LUA_TYPE_FILE);
	if (file) {
		*handle = file->next;
	}
	return 1;
}

/***
 * Create an iterator over all mark names.
 * @function mark_names
 * @return the new iterator
 * @usage
 * local marks = vix.win.marks
 * for name in vix:mark_names() do
 * 	local mark = marks[name]
 * 	for i = 1, #mark do
 * 		-- do something with: name, mark[i].start, mark[i].finish
 * 	end
 * end
 */
static int mark_names_iter(lua_State *L);
static int mark_names(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	lua_pushlightuserdata(L, vix);
	enum VixMark *handle = lua_newuserdata(L, sizeof *handle);
	*handle = 0;
	lua_pushcclosure(L, mark_names_iter, 2);
	return 1;
}

static int mark_names_iter(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	enum VixMark *handle = lua_touserdata(L, lua_upvalueindex(2));
	char mark = vix_mark_to(vix, *handle);
	if (mark) {
		lua_pushlstring(L, &mark, 1);
		(*handle)++;
		return 1;
	}
	return 0;
}

/***
 * Create an iterator over all register names.
 * @function register_names
 * @return the new iterator
 * @usage
 * for name in vix:register_names() do
 * 	local reg = vix.registers[name]
 * 	for i = 1, #reg do
 * 		-- do something with register value reg[i]
 * 	end
 * end
 */
static int register_names_iter(lua_State *L);
static int register_names(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	lua_pushlightuserdata(L, vix);
	enum VixRegister *handle = lua_newuserdata(L, sizeof *handle);
	*handle = 0;
	lua_pushcclosure(L, register_names_iter, 2);
	return 1;
}

static int register_names_iter(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	enum VixRegister *handle = lua_touserdata(L, lua_upvalueindex(2));
	char reg = vix_register_to(vix, *handle);
	if (reg) {
		lua_pushlstring(L, &reg, 1);
		(*handle)++;
		return 1;
	}
	return 0;
}

/***
 * Execute a `:`-command.
 * @function command
 * @tparam string command the command to execute
 * @treturn bool whether the command succeeded
 * @usage
 * vix:command("set number")
 */
static int command(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *cmd = luaL_checkstring(L, 2);
	bool ret = vix_cmd(vix, cmd);
	lua_pushboolean(L, ret);
	return 1;
}

/***
 * Display a short message.
 *
 * The single line message will be displayed at the bottom of
 * the screen and automatically hidden once a key is pressed.
 *
 * @function info
 * @tparam string message the message to display
 */
static int info(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *msg = luaL_checkstring(L, 2);
	vix_info_show(vix, "%s", msg);
	return 0;
}

/***
 * Display a multi line message.
 *
 * Opens a new window and displays an arbitrarily long message.
 *
 * @function message
 * @tparam string message the message to display
 */
static int message(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *msg = luaL_checkstring(L, 2);
	vix_message_show(vix, msg);
	return 0;
}

/***
 * Register a Lua function as key action.
 * @function action_register
 * @tparam string name the name of the action, can be referred to in key bindings as `<name>` pseudo key
 * @tparam Function func the lua function implementing the key action (see @{keyhandler})
 * @tparam[opt] string help the single line help text as displayed in `:help`
 * @treturn KeyAction action the registered key action
 * @see Vix:map
 * @see Window:map
 */
static int action_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	const void *func = func_ref_new(L, 3);
	const char *help = luaL_optstring(L, 4, NULL);
	KeyAction *action = vix_action_new(vix, name, help, keymapping, (Arg){ .v = func });
	if (!action) {
		goto err;
	}
	if (!vix_action_register(vix, action)) {
		goto err;
	}
	obj_ref_new(L, action, VIX_LUA_TYPE_KEYACTION);
	return 1;
err:
	vix_action_free(vix, action);
	lua_pushnil(L);
	return 1;
}

static int keymap(lua_State *L, Vix *vix, Win *win) {
	int mode = luaL_checkinteger(L, 2);
	const char *key = luaL_checkstring(L, 3);
	const char *help = luaL_optstring(L, 5, NULL);
	KeyBinding *binding = vix_binding_new(vix);
	if (!binding) {
		goto err;
	}
	if (lua_isstring(L, 4)) {
		const char *alias = luaL_checkstring(L, 4);
		if (!(binding->alias = strdup(alias))) {
			goto err;
		}
	} else if (lua_isfunction(L, 4)) {
		const void *func = func_ref_new(L, 4);
		if (!(binding->action = vix_action_new(vix, NULL, help, keymapping, (Arg){ .v = func }))) {
			goto err;
		}
	} else if (lua_isuserdata(L, 4)) {
		binding->action = obj_ref_check(L, 4, VIX_LUA_TYPE_KEYACTION);
	} else {
		goto err;
	}

	if (win) {
		if (!vix_window_mode_map(win, mode, true, key, binding)) {
			goto err;
		}
	} else {
		if (!vix_mode_map(vix, mode, true, key, binding)) {
			goto err;
		}
	}

	lua_pushboolean(L, true);
	return 1;
err:
	vix_binding_free(vix, binding);
	lua_pushboolean(L, false);
	return 1;
}

/***
 * Map a key to a Lua function.
 *
 * Creates a new key mapping in a given mode.
 *
 * @function map
 * @tparam int mode the mode to which the mapping should be added
 * @tparam string key the key to map
 * @tparam function func the Lua function to handle the key mapping (see @{keyhandler})
 * @tparam[opt] string help the single line help text as displayed in `:help`
 * @treturn bool whether the mapping was successfully established
 * @see Window:map
 * @usage
 * vix:map(vix.modes.INSERT, "<C-k>", function(keys)
 * 	if #keys < 2 then
 * 		return -1 -- need more input
 * 	end
 * 	local digraph = keys:sub(1, 2)
 * 	if digraph == "l*" then
 * 		vix:feedkeys('λ')
 * 		return 2 -- consume 2 bytes of input
 * 	end
 * end, "Insert digraph")
 */
/***
 * Setup a key alias.
 *
 * This is equivalent to `vix:command('map! mode key alias')`.
 *
 * Mappings are always recursive!
 * @function map
 * @tparam int mode the mode to which the mapping should be added
 * @tparam string key the key to map
 * @tparam string alias the key to map to
 * @treturn bool whether the mapping was successfully established
 * @see Window:map
 * @usage
 * vix:map(vix.modes.NORMAL, "j", "k")
 */
/***
 * Map a key to a key action.
 *
 * @function map
 * @tparam int mode the mode to which the mapping should be added
 * @tparam string key the key to map
 * @param action the action to map
 * @treturn bool whether the mapping was successfully established
 * @see Window:map
 * @usage
 * local action = vix:action_register("info", function()
 *   vix:info("Mapping works!")
 * end, "Info message help text")
 * vix:map(vix.modes.NORMAL, "gh", action)
 * vix:map(vix.modes.NORMAL, "gl", action)
 */
static int map(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	return keymap(L, vix, NULL);
}

/***
 * Unmap a global key binding.
 *
 * @function unmap
 * @tparam int mode the mode from which the mapping should be removed
 * @tparam string key the mapping to remove
 * @treturn bool whether the mapping was successfully removed
 * @see Window:unmap
 */
static int keyunmap(lua_State *L, Vix *vix, Win *win) {
	enum VixMode mode = luaL_checkinteger(L, 2);
	const char *key = luaL_checkstring(L, 3);
	bool ret;
	if (!win) {
		ret = vix_mode_unmap(vix, mode, key);
	} else {
		ret = vix_window_mode_unmap(win, mode, key);
	}
	lua_pushboolean(L, ret);
	return 1;
}

static int unmap(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	return keyunmap(L, vix, NULL);
}

/***
 * Get all currently active mappings of a mode.
 *
 * @function mappings
 * @tparam int mode the mode to query
 * @treturn table the active mappings and their associated help texts
 * @usage
 * local bindings = vix:mappings(vix.modes.NORMAL)
 * for key, help in pairs(bindings) do
 * 	-- do something
 * end
 * @see Vix:map
 */
static bool binding_collect(const char *key, void *value, void *ctx) {
	lua_State *L = ctx;
	KeyBinding *binding = value;
	lua_getfield(L, -1, key);
	bool new = lua_isnil(L, -1);
	lua_pop(L, 1);
	if (new) {
		const char *help = binding->alias ? binding->alias : VIX_HELP_USE(binding->action->help);
		lua_pushstring(L, help ? help : "");
		lua_setfield(L, -2, key);
	}
	return true;
}

static int mappings(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	lua_newtable(L);
	for (Mode *mode = mode_get(vix, luaL_checkinteger(L, 2)); mode; mode = mode->parent) {
		if (!mode->bindings) {
			continue;
		}
		map_iterate(mode->bindings, binding_collect, vix->lua);
	}
	return 1;
}

/***
 * Execute a motion.
 *
 * @function motion
 * @tparam int id the id of the motion to execute
 * @treturn bool whether the id was valid
 * @local
 */
static int motion(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	enum VixMotion id = luaL_checkinteger(L, 2);
	// TODO handle var args?
	lua_pushboolean(L, vix && vix_motion(vix, id));
	return 1;
}

static size_t motion_lua(Vix *vix, Win *win, void *data, size_t pos) {
	lua_State *L = vix->lua;
	if (!L || !func_ref_get(L, data) || !obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW)) {
		return EPOS;
	}

	lua_pushinteger(L, pos);
	if (pcall(vix, L, 2, 1) != 0) {
		return EPOS;
	}
	return getpos(L, -1);
}

/***
 * Register a custom motion.
 *
 * @function motion_register
 * @tparam function motion the Lua function implementing the motion
 * @treturn int the associated motion id
 * @see motion, motion_new
 * @local
 * @usage
 * -- custom motion advancing to the next byte
 * local id = vix:motion_register(function(win, pos)
 * 	return pos+1
 * end)
 */
static int motion_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const void *func = func_ref_new(L, 2);
	int id = vix_motion_register(vix, (void*)func, motion_lua);
	lua_pushinteger(L, id);
	return 1;
}

/***
 * Execute an operator.
 *
 * @function operator
 * @tparam int id the id of the operator to execute
 * @treturn bool whether the id was valid
 * @local
 */
static int operator(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	enum VixOperator id = luaL_checkinteger(L, 2);
	// TODO handle var args?
	lua_pushboolean(L, vix && vix_operator(vix, id));
	return 1;
}

static size_t operator_lua(Vix *vix, Text *text, OperatorContext *c) {
	lua_State *L = vix->lua;
	if (!L || !func_ref_get(L, c->context)) {
		return EPOS;
	}
	File *file = vix->files;
	while (file && (file->internal || file->text != text)) {
		file = file->next;
	}
	if (!file || !obj_ref_new(L, file, VIX_LUA_TYPE_FILE)) {
		return EPOS;
	}
	pushrange(L, &c->range);
	pushpos(L, c->pos);
	if (pcall(vix, L, 3, 1) != 0) {
		return EPOS;
	}
	return getpos(L, -1);
}

/***
 * Register a custom operator.
 *
 * @function operator_register
 * @tparam function operator the Lua function implementing the operator
 * @treturn int the associated operator id
 * @see operator, operator_new
 * @local
 * @usage
 * -- custom operator replacing every 'a' with 'b'
 * local id = vix:operator_register(function(file, range, pos)
 * 	local data = file:content(range)
 * 	data = data:gsub("a", "b")
 * 	file:delete(range)
 * 	file:insert(range.start, data)
 * 	return range.start -- new cursor location
 * end)
 */
static int operator_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const void *func = func_ref_new(L, 2);
	int id = vix_operator_register(vix, operator_lua, (void*)func);
	lua_pushinteger(L, id);
	return 1;
}

/***
 * Execute a text object.
 *
 * @function textobject
 * @tparam int id the id of the text object to execute
 * @treturn bool whether the id was valid
 * @see textobject_register, textobject_new
 * @local
 */
static int textobject(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	enum VixTextObject id = luaL_checkinteger(L, 2);
	lua_pushboolean(L, vix_textobject(vix, id));
	return 1;
}

static Filerange textobject_lua(Vix *vix, Win *win, void *data, size_t pos) {
	lua_State *L = vix->lua;
	if (!L || !func_ref_get(L, data) || !obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW)) {
		return text_range_empty();
	}
	lua_pushinteger(L, pos);
	if (pcall(vix, L, 2, 2) != 0 || lua_isnil(L, -1)) {
		return text_range_empty();
	}
	return text_range_new(getpos(L, -2), getpos(L, -1));
}

/***
 * Register a custom text object.
 *
 * @function textobject_register
 * @tparam function textobject the Lua function implementing the text object
 * @treturn int the associated text object id, or `-1` on failure
 * @see textobject, textobject_new
 * @local
 * @usage
 * -- custom text object covering the next byte
 * local id = vix:textobject_register(function(win, pos)
 * 	return pos, pos+1
 * end)
 */
static int textobject_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const void *func = func_ref_new(L, 2);
	int id = vix_textobject_register(vix, 0, (void*)func, textobject_lua);
	lua_pushinteger(L, id);
	return 1;
}

static bool option_lua(Vix *vix, Win *win, void *context, bool toggle,
                       enum VixOption flags, const char *name, Arg *value) {
	lua_State *L = vix->lua;
	if (!L || !func_ref_get(L, context)) {
		return false;
	}
	if (flags & VIX_OPTION_TYPE_BOOL) {
		lua_pushboolean(L, value->b);
	} else if (flags & VIX_OPTION_TYPE_STRING) {
		lua_pushstring(L, value->s);
	} else if (flags & VIX_OPTION_TYPE_NUMBER) {
		lua_pushinteger(L, value->i);
	} else {
		return false;
	}
	lua_pushboolean(L, toggle);
	if (pcall(vix, L, 2, 1) != 0) {
		return false;
	}
	bool ret = !lua_isboolean(L, -1) || lua_toboolean(L, -1);
	lua_pop(L, 1);

	if (ret) {
		lua_getfield(L, LUA_REGISTRYINDEX, "vix_option_values");
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_setfield(L, LUA_REGISTRYINDEX, "vix_option_values");
		}
		lua_pushstring(L, name);
		if (flags & VIX_OPTION_TYPE_BOOL) {
			lua_pushboolean(L, value->b);
		} else if (flags & VIX_OPTION_TYPE_STRING) {
			lua_pushstring(L, value->s);
		} else if (flags & VIX_OPTION_TYPE_NUMBER) {
			lua_pushinteger(L, value->i);
		}
		lua_settable(L, -3);
		lua_pop(L, 1);
	}

	return ret;
}

/***
 * Register a custom `:set` option.
 *
 * @function option_register
 * @tparam string name the option name
 * @tparam string type the option type (`bool`, `string` or `number`)
 * @tparam function handler the Lua function being called when the option is changed
 * @tparam[opt] string help the single line help text as displayed in `:help`
 * @treturn bool whether the option was successfully registered
 * @usage
 * vix:option_register("foo", "bool", function(value, toggle)
 * 	if not vix.win then return false end
 * 	vix.win.foo = toggle and not vix.win.foo or value
 * 	vix:info("Option foo = " .. tostring(vix.win.foo))
 * 	return true
 * end, "Foo enables superpowers")
 */
static int option_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	const char *type = luaL_checkstring(L, 3);
	const void *func = func_ref_new(L, 4);
	const char *help = luaL_optstring(L, 5, NULL);
	const char *names[] = { name, NULL };
	enum VixOption flags = 0;
	if (strcmp(type, "string") == 0) {
		flags |= VIX_OPTION_TYPE_STRING;
	} else if (strcmp(type, "number") == 0) {
		flags |= VIX_OPTION_TYPE_NUMBER;
	} else {
		flags |= VIX_OPTION_TYPE_BOOL;
	}
	bool ret = vix_option_register(vix, names, flags, option_lua, (void*)func, help);
	lua_pushboolean(L, ret);
	return 1;
}

/***
 * Unregister a `:set` option.
 *
 * @function option_unregister
 * @tparam string name the option name
 * @treturn bool whether the option was successfully unregistered
 */
static int option_unregister(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	bool ret = vix_option_unregister(vix, name);
	lua_pushboolean(L, ret);
	return 1;
}

static bool command_lua(Vix *vix, Win *win, void *data, bool force, const char *argv[], Selection *sel, Filerange *range) {
	lua_State *L = vix->lua;
	if (!L || !func_ref_get(L, data)) {
		return false;
	}
	lua_newtable(L);
	for (size_t i = 0; argv[i]; i++) {
		lua_pushinteger(L, i);
		lua_pushstring(L, argv[i]);
		lua_settable(L, -3);
	}
	lua_pushboolean(L, force);
	if (!obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW)) {
		return false;
	}
	if (!sel) {
		sel = view_selections_primary_get(&win->view);
	}
	if (!obj_lightref_new(L, sel, VIX_LUA_TYPE_SELECTION)) {
		return false;
	}
	pushrange(L, range);
	if (pcall(vix, L, 5, 1) != 0) {
		return false;
	}
	return lua_toboolean(L, -1);
}

/***
 * Register a custom `:`-command.
 *
 * @function command_register
 * @tparam string name the command name
 * @tparam function command the Lua function implementing the command
 * @tparam[opt] string help the single line help text as displayed in `:help`
 * @treturn bool whether the command has been successfully registered
 * @usage
 * vix:command_register("foo", function(argv, force, win, selection, range)
 * 	 for i,arg in ipairs(argv) do
 * 		 print(i..": "..arg)
 * 	 end
 * 	 print("was command forced with ! "..(force and "yes" or "no"))
 * 	 print(win.file.name)
 * 	 print(selection.pos)
 * 	 print(range ~= nil and ('['..range.start..', '..range.finish..']') or "invalid range")
 * 	 return true;
 * end)
 */
static int command_register(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	const void *func = func_ref_new(L, 3);
	const char *help = luaL_optstring(L, 4, "");
	bool ret = vix_cmd_register(vix, name, help, (void*)func, command_lua);
	lua_pushboolean(L, ret);
	return 1;
}

/***
 * Let user pick a command matching the given prefix.
 *
 * The editor core will be blocked while the external process is running.
 *
 * @function complete_command
 * @tparam string prefix the prefix of the command to be completed
 * @treturn int code the exit status of the executed command
 * @treturn string stdout the data written to stdout
 * @treturn string stderr the data written to stderr
 */
static int complete_command(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *prefix = luaL_checkstring(L, 2);
	char *out = NULL, *err = NULL;

	Buffer buf = {0};
	vix_print_cmds(vix, &buf, prefix);
	int status = vix_pipe_buf_collect(vix, buffer_content0(&buf), (const char*[]){"vix-menu", "-b", 0},
	                                  &out, &err, false);

	lua_pushinteger(L, status);
	if (out) {
		lua_pushstring(L, out);
	} else {
		lua_pushnil(L);
	}
	if (err) {
		lua_pushstring(L, err);
	} else {
		lua_pushnil(L);
	}

	free(out);
	free(err);
	buffer_release(&buf);
	return 3;
}

/***
 * Complete option name.
 *
 * This function uses @{vix-menu} to interactively select a matching option.
 *
 * @function complete_option
 * @tparam string prefix the prefix of the option to be completed
 * @treturn int code the exit status of the executed command
 * @treturn string stdout the data written to stdout
 * @treturn string stderr the data written to stderr
 */
static int complete_option(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *prefix = luaL_checkstring(L, 2);
	char *out = NULL, *err = NULL;

	Buffer buf = {0};
	vix_print_options(vix, &buf, prefix);
	int status = vix_pipe_buf_collect(vix, buffer_content0(&buf), (const char*[]){"vix-menu", "-b", 0},
	                                  &out, &err, false);

	lua_pushinteger(L, status);
	if (out) {
		lua_pushstring(L, out);
	} else {
		lua_pushnil(L);
	}
	if (err) {
		lua_pushstring(L, err);
	} else {
		lua_pushnil(L);
	}

	free(out);
	free(err);
	buffer_release(&buf);
	return 3;
}

/***
 * Get option type.
 *
 * @function option_type
 * @tparam string name the name of the option
 * @treturn string the type of the option ("bool", "number", "string") or nil
 */
static int lua_option_type(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	OptionDef *opt = map_get(vix->options, name);
	if (!opt) {
		lua_pushnil(L);
		return 1;
	}
	
	lua_newtable(L);
	if (opt->flags & VIX_OPTION_TYPE_BOOL) {
		lua_pushstring(L, "bool");
	} else if (opt->flags & VIX_OPTION_TYPE_NUMBER) {
		lua_pushstring(L, "number");
	} else if (opt->flags & VIX_OPTION_TYPE_STRING) {
		lua_pushstring(L, "string");
	} else {
		lua_pushnil(L);
	}
	lua_setfield(L, -2, "type");
	
	lua_pushboolean(L, (opt->flags & VIX_OPTION_NEED_WINDOW) != 0);
	lua_setfield(L, -2, "need_window");

	lua_newtable(L);
	for (int i = 0; i < 4 && opt->names[i]; i++) {
		lua_pushstring(L, opt->names[i]);
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "aliases");
	
	return 1;
}

/***
 * Get option value.
 *
 * @function option_value
 * @tparam string name the name of the option
 * @treturn string the current value of the option or nil
 */
static int lua_option_value(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	Buffer buf = {0};
	vix_print_option_value(vix, name, &buf);
	if (buf.len > 0) {
		lua_pushstring(L, buffer_content0(&buf));
	} else {
		lua_pushnil(L);
	}
	buffer_release(&buf);
	return 1;
}

/***
 * Push keys to input queue and interpret them.
 *
 * The keys are processed as if they were read from the keyboard.
 *
 * @function feedkeys
 * @tparam string keys the keys to interpret
 */
static int feedkeys(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *keys = luaL_checkstring(L, 2);
	vix_keys_feed(vix, keys);
	return 0;
}

/***
 * Insert keys at all cursor positions of active window.
 *
 * This function behaves as if the keys were entered in insert mode,
 * but in contrast to @{Vix:feedkeys} it bypasses the input queue,
 * meaning mappings do not apply and the keys will not be recorded in macros.
 *
 * @function insert
 * @tparam string keys the keys to insert
 * @see Vix:feedkeys
 */
static int insert(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	size_t len;
	const char *keys = luaL_checklstring(L, 2, &len);
	vix_insert_key(vix, keys, len);
	return 0;
}

/***
 * Replace keys at all cursor positions of active window.
 *
 * This function behaves as if the keys were entered in replace mode,
 * but in contrast to @{Vix:feedkeys} it bypasses the input queue,
 * meaning mappings do not apply and the keys will not be recorded in macros.
 *
 * @function replace
 * @tparam string keys the keys to insert
 * @see Vix:feedkeys
 */
static int replace(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	size_t len;
	const char *keys = luaL_checklstring(L, 2, &len);
	vix_replace_key(vix, keys, len);
	return 0;
}

/***
 * Terminate editor process.
 *
 * Termination happens upon the next iteration of the main event loop.
 * This means the calling Lua code will be executed further until it
 * eventually hands over control to the editor core. The exit status
 * of the most recent call is used.
 *
 * All unsaved changes will be lost!
 *
 * @function exit
 * @tparam int code the exit status returned to the operating system
 */
static int exit_func(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	int code = luaL_checkinteger(L, 2);
	vix_exit(vix, code);
	return 0;
}

/***
 * Pipe file range to external process and collect output.
 *
 * The editor core will be blocked while the external process is running.
 * File and Range can be omitted or nil to indicate empty input.
 *
 * @function pipe
 * @tparam[opt] File file the file to which the range applies
 * @tparam[opt] Range range the range to pipe
 * @tparam string command the command to execute
 * @tparam[opt] bool fullscreen whether command is a fullscreen program (e.g. curses based)
 * @treturn int code the exit status of the executed command
 * @treturn string stdout the data written to stdout
 * @treturn string stderr the data written to stderr
 */
/***
 * Pipe a string to external process and collect output.
 *
 * The editor core will be blocked while the external process is running.
 *
 * @function pipe
 * @tparam string text the text written to the external command
 * @tparam string command the command to execute
 * @tparam[opt] bool fullscreen whether command is a fullscreen program (e.g. curses based)
 * @treturn int code the exit status of the executed command
 * @treturn string stdout the data written to stdout
 * @treturn string stderr the data written to stderr
 */
static int pipe_func(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	int cmd_idx = 4;
	char *out = NULL, *err = NULL;
	const char *text = NULL;
	File *file = vix->win ? vix->win->file : NULL;
	Filerange range = text_range_new(0, 0);
	if (lua_gettop(L) == 2) { // vix:pipe(cmd)
		cmd_idx = 2;
	} else if (lua_gettop(L) == 3) {
		if (lua_isboolean(L, 3)) { // vix:pipe(cmd, fullscreen)
			cmd_idx = 2;
		} else { // vix:pipe(text, cmd)
			text = luaL_checkstring(L, 2);
			cmd_idx = 3;
		}
	} else if (lua_gettop(L) == 4 && lua_isboolean(L, 4)) { // vix:pipe(text, cmd, fullscreen)
		text = luaL_checkstring(L, 2);
		cmd_idx = 3;
	} else if (!(lua_isnil(L, 2) && lua_isnil(L, 3))) { // vix:pipe(file, range, cmd, [fullscreen])
		file = obj_ref_check(L, 2, VIX_LUA_TYPE_FILE);
		range = getrange(L, 3);
	}
	const char *cmd = luaL_checkstring(L, cmd_idx);
	bool fullscreen = lua_isboolean(L, cmd_idx + 1) && lua_toboolean(L, cmd_idx + 1);

	if (!text && !file) {
		return luaL_error(L, "vix:pipe(cmd = '%s'): win not open, file can't be nil", cmd);
	}

	int status;
	if (text) {
		status = vix_pipe_buf_collect(vix, text, (const char*[]){ cmd, NULL }, &out, &err, fullscreen);
	} else {
		status = vix_pipe_collect(vix, file, &range, (const char*[]){ cmd, NULL }, &out, &err, fullscreen);
	}
	lua_pushinteger(L, status);
	if (out) {
		lua_pushstring(L, out);
	} else {
		lua_pushnil(L);
	}
	free(out);
	if (err) {
		lua_pushstring(L, err);
	} else {
		lua_pushnil(L);
	}
	free(err);
	vix_draw(vix);
	return 3;
}

/***
 * Redraw complete user interface.
 *
 * Will trigger redraw events, make sure to avoid recursive events.
 *
 * @function redraw
 */
static int redraw(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	vix_redraw(vix);
	return 0;
}
/***
 * Closes a stream returned by @{Vix:communicate}.
 *
 * @function close
 * @tparam io.file inputfd the stream to be closed
 * @treturn bool identical to @{io.close}
 */
static int close_subprocess(lua_State *L) {
	luaL_Stream *file = luaL_checkudata(L, -1, "FILE*");
	int result = fclose(file->f);
	if (result == 0) {
		file->f = NULL;
		file->closef = NULL;
	}
	return luaL_fileresult(L, result == 0, NULL);
}
/***
 * Open new process and return its input stream (stdin).
 * If the stream is closed (by calling the close method or by being removed by a garbage collector)
 * the spawned process will be killed by SIGTERM.
 * When the process will quit or will output anything to stdout or stderr,
 * the @{process_response} event will be fired.
 *
 * The editor core won't be blocked while the external process is running.
 *
 * @function communicate
 * @tparam string name the name of subprocess (to distinguish processes in the @{process_response} event)
 * @tparam string command the command to execute
 * @return the file handle to write data to the process, in case of error the return values are equivalent to @{io.open} error values.
 */
static int communicate_func(lua_State *L) {

	typedef struct {
		/* Lua stream structure for the process input stream */
		luaL_Stream stream;
		Process *handler;
	} ProcessStream;

	Vix *vix = obj_ref_check(L, 1, "vix");
	const char *name = luaL_checkstring(L, 2);
	const char *cmd = luaL_checkstring(L, 3);
	ProcessStream *inputfd = (ProcessStream *)lua_newuserdata(L, sizeof(ProcessStream));
	luaL_setmetatable(L, LUA_FILEHANDLE);
	inputfd->handler = vix_process_communicate(vix, name, cmd, &(inputfd->stream.closef));
	if (inputfd->handler) {
		inputfd->stream.f = fdopen(inputfd->handler->inpfd, "w");
		inputfd->stream.closef = &close_subprocess;
	}
	return inputfd->stream.f ? 1 : luaL_fileresult(L, 0, name);
}
/***
 * Currently active window.
 * @tfield Window win
 * @see windows
 */
/***
 * Currently active mode.
 * @tfield modes mode
 */
/***
 * Whether a macro is being recorded.
 * @tfield bool recording
 */
/***
 * Currently unconsumed keys in the input queue.
 * @tfield string input_queue
 */
/***
 * Register name in use.
 * @tfield string register
 */
/***
 * Mark name in use.
 * @tfield string mark
 */
static int vix_index(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "win") == 0) {
			if (vix->win) {
				obj_ref_new(L, vix->win, VIX_LUA_TYPE_WINDOW);
			} else {
				lua_pushnil(L);
			}
			return 1;
		}

		if (strcmp(key, "mode") == 0) {
			lua_pushinteger(L, vix->mode->id);
			return 1;
		}

		if (strcmp(key, "input_queue") == 0) {
			lua_pushstring(L, buffer_content0(&vix->input_queue));
			return 1;
		}

		if (strcmp(key, "recording") == 0) {
			lua_pushboolean(L, vix_macro_recording(vix));
			return 1;
		}

		if (strcmp(key, "count") == 0) {
			int count = vix->action.count;
			if (count == VIX_COUNT_UNKNOWN) {
				lua_pushnil(L);
			} else {
				lua_pushinteger(L, count);
			}
			return 1;
		}

		if (strcmp(key, "register") == 0) {
			char name = vix_register_to(vix, vix_register_used(vix));
			lua_pushlstring(L, &name, 1);
			return 1;
		}

		if (strcmp(key, "registers") == 0) {
			obj_ref_new(L, &vix->ui, VIX_LUA_TYPE_REGISTERS);
			return 1;
		}

		if (strcmp(key, "mark") == 0) {
			char name = vix_mark_to(vix, vix_mark_used(vix));
			lua_pushlstring(L, &name, 1);
			return 1;
		}

		if (strcmp(key, "options") == 0) {
			obj_ref_new(L, &vix->options, VIX_LUA_TYPE_VIX_OPTS);
			return 1;
		}

		if (strcmp(key, "ui") == 0) {
			obj_ref_new(L, &vix->ui, VIX_LUA_TYPE_UI);
			return 1;
		}
	}

	return index_common(L);
}

static int vix_options_assign(Vix *vix, lua_State *L, const char *key, int next) {
	if (strcmp(key, "autoindent") == 0 || strcmp(key, "ai") == 0) {
		vix->autoindent = lua_toboolean(L, next);
	} else if (strcmp(key, "opentab") == 0) {
		vix->opentab = lua_toboolean(L, next);
		if (vix->opentab) {
			vix->ui.tabview = true;
		}
	} else if (strcmp(key, "changecolors") == 0) {
		vix->change_colors = lua_toboolean(L, next);
	} else if (strcmp(key, "escdelay") == 0) {
		termkey_set_waittime(vix->ui.termkey, luaL_checkinteger(L, next));
	} else if (strcmp(key, "ignorecase") == 0 || strcmp(key, "ic") == 0) {
		vix->ignorecase = lua_toboolean(L, next);
	} else if (strcmp(key, "loadmethod") == 0) {
		if (!lua_isstring(L, next)) {
			return newindex_common(L);
		}
		const char *lm = lua_tostring(L, next);
		if (strcmp(lm, "auto") == 0) {
			vix->load_method = TEXT_LOAD_AUTO;
		} else if (strcmp(lm, "read") == 0) {
			vix->load_method = TEXT_LOAD_READ;
		} else if (strcmp(lm, "mmap") == 0) {
			vix->load_method = TEXT_LOAD_MMAP;
		}
	} else if (strcmp(key, "shell") == 0) {
		if (!lua_isstring(L, next)) {
			return newindex_common(L);
		}
		vix_shell_set(vix, lua_tostring(L, next));
	}
	return 0;
}

static int vix_newindex(lua_State *L) {
	Vix *vix = obj_ref_check(L, 1, "vix");
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "mode") == 0) {
			enum VixMode mode = luaL_checkinteger(L, 3);
			vix_mode_switch(vix, mode);
			return 0;
		}

		if (strcmp(key, "count") == 0) {
			int count;
			if (lua_isnil(L, 3)) {
				count = VIX_COUNT_UNKNOWN;
			} else {
				count = luaL_checkinteger(L, 3);
			}
			vix->action.count = count;
			return 0;
		}

		if (strcmp(key, "win") == 0) {
			vix_window_focus(obj_ref_check(L, 3, VIX_LUA_TYPE_WINDOW));
			return 0;
		}

		if (strcmp(key, "register") == 0) {
			const char *name = luaL_checkstring(L, 3);
			if (strlen(name) == 1) {
				vix_register(vix, vix_register_from(vix, name[0]));
			}
			return 0;
		}

		if (strcmp(key, "mark") == 0) {
			const char *name = luaL_checkstring(L, 3);
			if (strlen(name) == 1) {
				vix_mark(vix, vix_mark_from(vix, name[0]));
			}
			return 0;
		}

		if (strcmp(key, "options") == 0 && lua_istable(L, 3)) {
			int ret = 0;
			/* since we don't know which keys are in the table we push
			 * a nil then use lua_next() to remove it and push the
			 * table's key-value pairs to the stack. these can then be
			 * used to assign options
			 */
			lua_pushnil(L);
			while (lua_next(L, 3)) {
				if (lua_isstring(L, 4)) {
					ret += vix_options_assign(vix, L, lua_tostring(L, 4), 5);
				} else {
					ret += newindex_common(L);
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
			return ret;
		}
	}
	return newindex_common(L);
}

/***
 * Get options changed in current session.
 * @function session_changes
 * @treturn table a map of option names to boolean true
 */
static int lua_session_changes(lua_State *L) {
	lua_getfield(L, LUA_REGISTRYINDEX, "vix_session_changes");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
	}
	return 1;
}

static const struct luaL_Reg vix_lua[] = {
	{ "files", files },
	{ "windows", windows },
	{ "mark_names", mark_names },
	{ "register_names", register_names },
	{ "command", command },
	{ "info", info },
	{ "message", message },
	{ "map", map },
	{ "unmap", unmap },
	{ "mappings", mappings },
	{ "operator", operator },
	{ "operator_register", operator_register },
	{ "motion", motion },
	{ "motion_register", motion_register },
	{ "textobject", textobject },
	{ "textobject_register", textobject_register },
	{ "option_register", option_register },
	{ "option_unregister", option_unregister },
	{ "command_register", command_register },
	{ "complete_command", complete_command },
	{ "complete_option", complete_option },
	{ "option_type", lua_option_type },
	{ "option_value", lua_option_value },
	{ "session_changes", lua_session_changes },
	{ "feedkeys", feedkeys },
	{ "insert", insert },
	{ "replace", replace },
	{ "action_register", action_register },
	{ "exit", exit_func },
	{ "pipe", pipe_func },
	{ "redraw", redraw },
	{ "communicate", communicate_func },
	{ "__index", vix_index },
	{ "__newindex", vix_newindex },
	{ NULL, NULL },
};

/***
 * Vix Options
 * @table options
 * @tfield[opt=false] boolean autoindent {ai}
 * @tfield[opt=false] boolean changecolors
 * @tfield[opt=50] int escdelay
 * @tfield[opt=false] boolean ignorecase {ic}
 * @tfield[opt="auto"] string loadmethod `"auto"`, `"read"`, or `"mmap"`.
 * @tfield[opt="/bin/sh"] string shell
 * @see Window.options
 */

static int vix_options_index(lua_State *L) {
	Vix *vix = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_VIX_OPTS, offsetof(Vix, options));
	if (!vix) {
		return -1;
	}
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "autoindent") == 0 || strcmp(key, "ai") == 0) {
			lua_pushboolean(L, vix->autoindent);
			return 1;
		} else if (strcmp(key, "opentab") == 0) {
			lua_pushboolean(L, vix->opentab);
			return 1;
		} else if (strcmp(key, "changecolors") == 0) {
			lua_pushboolean(L, vix->change_colors);
			return 1;
		} else if (strcmp(key, "escdelay") == 0) {
			lua_pushinteger(L, termkey_get_waittime(vix->ui.termkey));
			return 1;
		} else if (strcmp(key, "ignorecase") == 0 || strcmp(key, "ic") == 0) {
			lua_pushboolean(L, vix->ignorecase);
			return 1;
		} else if (strcmp(key, "loadmethod") == 0) {
			switch (vix->load_method) {
			case TEXT_LOAD_AUTO:
				lua_pushliteral(L, "auto");
				break;
			case TEXT_LOAD_READ:
				lua_pushliteral(L, "read");
				break;
			case TEXT_LOAD_MMAP:
				lua_pushliteral(L, "mmap");
				break;
			}
			return 1;
		} else if (strcmp(key, "shell") == 0) {
			lua_pushstring(L, vix->shell);
			return 1;
		}
	}
	return index_common(L);
}

static int vix_options_newindex(lua_State *L) {
	Vix *vix = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_VIX_OPTS, offsetof(Vix, options));
	if (!vix) {
		return 0;
	}
	if (lua_isstring(L, 2)) {
		return vix_options_assign(vix, L, lua_tostring(L, 2), 3);
	}
	return newindex_common(L);
}

static const struct luaL_Reg vix_option_funcs[] = {
	{ "__index", vix_options_index },
	{ "__newindex", vix_options_newindex},
	{ NULL, NULL },
};

/***
 * The user interface.
 *
 * @type Ui
 */
/***
 * Number of available colors.
 * @tfield int colors
 */
/***
 * Current layout.
 * @tfield layouts layout current window layout.
 */

static int ui_index(lua_State *L) {
	Ui *ui = obj_ref_check(L, 1,  VIX_LUA_TYPE_UI);

	if (lua_isstring(L, 2)) {
		const char *key  = lua_tostring(L, 2);

		if (strcmp(key, "layout") == 0) {
			lua_pushinteger(L, ui->layout);
			return 1;
		}
		if (strcmp(key, "cur_row") == 0) {
			lua_pushinteger(L, ui->cur_row);
			return 1;
		}
		if (strcmp(key, "cur_col") == 0) {
			lua_pushinteger(L, ui->cur_col);
			return 1;
		}
	}

	return index_common(L);
}

static int ui_newindex(lua_State *L) {
	Ui *ui = obj_ref_check(L, 1,  VIX_LUA_TYPE_UI);

	if (lua_isstring(L, 2)) {
		const char *key  = lua_tostring(L, 2);

		if (strcmp(key, "layout") == 0) {
			ui_arrange(ui, luaL_checkinteger(L, 3));
			return 0;
		}
	}
	return newindex_common(L);
}

static const struct luaL_Reg ui_funcs[] = {
	{ "__index", ui_index },
	{ "__newindex", ui_newindex },
	{ NULL, NULL },
};

static int registers_index(lua_State *L) {
	lua_newtable(L);
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	const char *symbol = luaL_checkstring(L, 2);
	if (strlen(symbol) != 1) {
		return 1;
	}
	enum VixRegister reg = vix_register_from(vix, symbol[0]);
	if (reg >= VIX_REG_INVALID) {
		return 1;
	}
	str8_list strings = vix_register_get(vix, reg);
	for (VixDACount i = 0; i < strings.count; i++) {
		str8 string = strings.data[i];
		lua_pushinteger(L, i+1);
		lua_pushlstring(L, (char *)string.data, string.length);
		lua_settable(L, -3);
	}
	da_release(&strings);
	return 1;
}

static int registers_newindex(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	const char *symbol = luaL_checkstring(L, 2);
	if (strlen(symbol) != 1) {
		return 0;
	}
	enum VixRegister reg = vix_register_from(vix, symbol[0]);

	str8_list strings = {0};
	if (lua_istable(L, 3)) {
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			size_t length;
			str8   string;
			string.data   = (uint8_t *)luaL_checklstring(L, -1, &length);
			string.length = (ptrdiff_t)length;
			*da_push(vix, &strings) = string;
			lua_pop(L, 1);
		}
	}

	vix_register_set(vix, reg, strings);
	da_release(&strings);
	return 0;
}

static int registers_len(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	lua_pushinteger(L, LENGTH(vix->registers));
	return 1;
}

static const struct luaL_Reg registers_funcs[] = {
	{ "__index", registers_index },
	{ "__newindex", registers_newindex },
	{ "__len", registers_len },
	{ NULL, NULL },
};

/***
 * A window object.
 * @type Window
 */

/***
 * Viewport currently being displayed.
 * Changing these values will not move the viewport.
 * @table viewport
 * @tfield Range bytes file bytes, from 0, at the start and end of the viewport
 * @tfield Range lines file lines, from 1, at the top and bottom of the viewport
 * @tfield int height lines in viewport, accounting for window decoration
 * @tfield int width columns in viewport, accounting for window decoration
 */
/***
 * The window width.
 * @tfield int width
 */
/***
 * The window height.
 * @tfield int height
 */
/***
 * The file being displayed in this window.
 * Changing the value to a file path will replace the current file with a new
 * one for the specified path.
 * @tfield File file
 */
/***
 * The primary selection of this window.
 * @tfield Selection selection
 */
/***
 * The selections of this window.
 * @tfield Array(Selection) selections
 */
/***
 * Window marks.
 * Most of these marks are stored in the associated File object, meaning they
 * are the same in all windows displaying the same file.
 * @field marks array to access the marks of this window by single letter name
 * @see Vix:mark_names
 */
static int window_index(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);

		if (strcmp(key, "viewport") == 0) {
			Filerange b = VIEW_VIEWPORT_GET(win->view);
			Filerange l;
			l.start = win->view.topline->lineno;
			l.end   = win->view.lastline->lineno;

			lua_createtable(L, 0, 4);
			lua_pushliteral(L, "bytes");
			pushrange(L, &b);
			lua_settable(L, -3);
			lua_pushliteral(L, "lines");
			pushrange(L, &l);
			lua_settable(L, -3);
			lua_pushliteral(L, "width");
			lua_pushinteger(L, win->view.width);
			lua_settable(L, -3);
			lua_pushliteral(L, "height");
			lua_pushinteger(L, win->view.height);
			lua_settable(L, -3);
			return 1;
		}

		if (strcmp(key, "width") == 0) {
			lua_pushinteger(L, win->width);
			return 1;
		}

		if (strcmp(key, "height") == 0) {
			lua_pushinteger(L, win->height);
			return 1;
		}

		if (strcmp(key, "weight") == 0) {
			lua_pushinteger(L, win->weight);
			return 1;
		}

		if (strcmp(key, "file") == 0) {
			obj_ref_new(L, win->file, VIX_LUA_TYPE_FILE);
			return 1;
		}

		if (strcmp(key, "selection") == 0) {
			Selection *sel = view_selections_primary_get(&win->view);
			obj_lightref_new(L, sel, VIX_LUA_TYPE_SELECTION);
			return 1;
		}

		if (strcmp(key, "selections") == 0) {
			obj_ref_new(L, &win->view, VIX_LUA_TYPE_SELECTIONS);
			return 1;
		}

		if (strcmp(key, "marks") == 0) {
			obj_ref_new(L, &win->saved_selections, VIX_LUA_TYPE_MARKS);
			return 1;
		}
		if (strcmp(key, "options") == 0) {
			obj_ref_new(L, &win->view, VIX_LUA_TYPE_WIN_OPTS);
			return 1;
		}
	}

	return index_common(L);
}

static int window_options_assign(Win *win, lua_State *L, const char *key, int next) {
	enum UiOption flags = win->options;
	if (strcmp(key, "breakat") == 0 || strcmp(key, "brk") == 0) {
		if (lua_isstring(L, next)) {
			view_breakat_set(&win->view, lua_tostring(L, next));
		}
	} else if (strcmp(key, "colorcolumn") == 0 || strcmp(key, "cc") == 0) {
		win->view.colorcolumn = luaL_checkinteger(L, next);
	} else if (strcmp(key, "cursorline") == 0 || strcmp(key, "cul") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_CURSOR_LINE;
		} else {
			flags &= ~UI_OPTION_CURSOR_LINE;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "numbers") == 0 || strcmp(key, "nu") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_LINE_NUMBERS_ABSOLUTE;
		} else {
			flags &= ~UI_OPTION_LINE_NUMBERS_ABSOLUTE;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "relativenumbers") == 0 || strcmp(key, "rnu") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_LINE_NUMBERS_RELATIVE;
		} else {
			flags &= ~UI_OPTION_LINE_NUMBERS_RELATIVE;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "showeof") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_SYMBOL_EOF;
		} else {
			flags &= ~UI_OPTION_SYMBOL_EOF;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "shownewlines") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_SYMBOL_EOL;
		} else {
			flags &= ~UI_OPTION_SYMBOL_EOL;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "showspaces") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_SYMBOL_SPACE;
		} else {
			flags &= ~UI_OPTION_SYMBOL_SPACE;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "showtabs") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_SYMBOL_TAB;
		} else {
			flags &= ~UI_OPTION_SYMBOL_TAB;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "statusbar") == 0) {
		if (lua_toboolean(L, next)) {
			flags |= UI_OPTION_STATUSBAR;
		} else {
			flags &= ~UI_OPTION_STATUSBAR;
		}
		win_options_set(win, flags);
	} else if (strcmp(key, "wrapcolumn") == 0 || strcmp(key, "wc") == 0) {
		win->view.wrapcolumn = luaL_checkinteger(L, next);
	} else if (strcmp(key, "tabwidth") == 0 || strcmp(key, "tw") == 0) {
		view_tabwidth_set(&win->view, luaL_checkinteger(L, next));
	} else if (strcmp(key, "expandtab") == 0 || strcmp(key, "et") == 0) {
		win->expandtab = lua_toboolean(L, next);
	}
	return 0;
}

static int window_newindex(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "options") == 0 && lua_istable(L, 3)) {
			int ret = 0;
			/* since we don't know which keys are in the table we push
			 * a nil then use lua_next() to remove it and push the
			 * table's key-value pairs to the stack. these can then be
			 * used to assign options
			 */
			lua_pushnil(L);
			while (lua_next(L, 3)) {
				if (lua_isstring(L, 4)) {
					ret += window_options_assign(win, L, lua_tostring(L, 4), 5);
				} else {
					ret += newindex_common(L);
				}
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
			return ret;
		} else if (strcmp(key, "file") == 0 && lua_isstring(L, 3)) {
			const char* filename = lua_tostring(L, 3);
			if (!vix_window_change_file(win, filename)) {
				return luaL_argerror(L, 3, "failed to open");
			}
			return 0;
		} else if (strcmp(key, "weight") == 0) {
			win->weight = luaL_checkinteger(L, 3);
			ui_arrange(&win->vix->ui, win->vix->ui.seltab->layout);
			vix_draw(win->vix);
			return 0;
		}
	}

	return newindex_common(L);
}

static int window_selections_iterator_next(lua_State *L) {
	Selection **handle = lua_touserdata(L, lua_upvalueindex(1));
	if (!*handle) {
		return 0;
	}
	Selection *sel = obj_lightref_new(L, *handle, VIX_LUA_TYPE_SELECTION);
	if (!sel) {
		return 0;
	}
	*handle = view_selections_next(sel);
	return 1;
}

/***
 * Create an iterator over all selections of this window.
 * @function selections_iterator
 * @return the new iterator
 */
static int window_selections_iterator(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	Selection **handle = lua_newuserdata(L, sizeof *handle);
	*handle = view_selections(&win->view);
	lua_pushcclosure(L, window_selections_iterator_next, 1);
	return 1;
}

/***
 * Set up a window local key mapping.
 * The function signatures are the same as for @{Vix:map}.
 * @function map
 * @param ...
 * @see Vix:map
 */
static int window_map(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	return keymap(L, win->vix, win);
}

/***
 * Remove a window local key mapping.
 * The function signature is the same as for @{Vix:unmap}.
 * @function unmap
 * @param ...
 * @see Vix:unmap
 */
static int window_unmap(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	return keyunmap(L, win->vix, win);
}

/***
 * Define a display style.
 * @function style_define
 * @tparam int id the style id to use
 * @tparam string style the style definition
 * @treturn bool whether the style definition has been successfully
 *  associated with the given id
 * @see style
 * @usage
 * win:style_define(win.STYLE_DEFAULT, "fore:red")
 */
static int window_style_define(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	enum UiStyle id = luaL_checkinteger(L, 2);
	const char *style = luaL_checkstring(L, 3);
	bool ret = ui_style_define(win, id, style);
	lua_pushboolean(L, ret);
	return 1;
}

/***
 * Style a window range.
 *
 * The style will be cleared after every window redraw.
 * @function style
 * @tparam      int  id the display style as registered with @{style_define}
 * @tparam      int  start the absolute file position in bytes
 * @tparam      int  finish the end position
 * @tparam[opt] bool keep_non_default whether non-default style values should be kept
 * @see style_define
 * @usage
 * win:style(win.STYLE_DEFAULT, 0, 10)
 */
static int window_style(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	enum UiStyle style = luaL_checkinteger(L, 2);
	size_t start = checkpos(L, 3);
	size_t end = checkpos(L, 4);
	bool keep_non_default = lua_isboolean(L, 5) && lua_toboolean(L, 5);
	win_style(win, style, start, end, keep_non_default);
	return 0;
}

/***
 * Style the single terminal cell at the given coordinates, relative to this window.
 *
 * Completely independent of the file buffer, and can be used to style UI elements,
 * such as the status bar.
 * The style will be cleared after every window redraw.
 * @function style_pos
 * @tparam      int  id display style registered with @{style_define}
 * @tparam      int  x 0-based x coordinate within Win, where (0,0) is the top left corner
 * @tparam      int  y See above
 * @tparam[opt] bool keep_non_default whether non-default style values should be kept
 * @treturn bool false if the coordinates would be outside the window's dimensions
 * @see style_define
 * @usage
 * win:style_pos(win.STYLE_COLOR_COLUMN, 0, win.height - 1)
 * -- Styles the first character of the status bar (or the last line, if disabled)
 */
static int window_style_pos(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	enum UiStyle style = luaL_checkinteger(L, 2);
	size_t x = checkpos(L, 3);
	size_t y = checkpos(L, 4);
	bool keep_non_default = lua_isboolean(L, 5) && lua_toboolean(L, 5);
	bool ret = ui_window_style_set_pos(win, (int)x, (int)y, style, keep_non_default);
	lua_pushboolean(L, ret);
	return 1;
}

/***
 * Set window status line.
 *
 * @function status
 * @tparam string left the left aligned part of the status line
 * @tparam[opt] string right the right aligned part of the status line
 */
static int window_status(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	char status[1024] = "";
	int width = win->width;
	const char *left = luaL_checkstring(L, 2);
	const char *right = luaL_optstring(L, 3, "");
	int left_width = text_string_width(left, strlen(left));
	int right_width = text_string_width(right, strlen(right));
	int spaces = width - left_width - right_width;
	if (spaces < 0) {
		spaces = 0;
	}
	snprintf(status, sizeof(status)-1, "%s%*s%s", left, spaces, " ", right);
	ui_window_status(win, status);
	return 0;
}

/***
 * Redraw window content.
 *
 * @function draw
 */
static int window_draw(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	view_draw(&win->view);
	return 0;
}

/***
 * Close window.
 *
 * After a successful call the Window reference becomes invalid and
 * must no longer be used. Attempting to close the last window will
 * always fail.
 *
 * @function close
 * @see exit
 * @tparam bool force whether unsaved changes should be discarded
 * @treturn bool whether the window was closed
 */
static int window_close(lua_State *L) {
	Win *win = obj_ref_check(L, 1, VIX_LUA_TYPE_WINDOW);
	int count = 0;
	for (Win *w = win->vix->windows; w; w = w->next) {
		if (!w->file->internal) {
			count++;
		}
	}
	bool force = lua_isboolean(L, 2) && lua_toboolean(L, 2);
	bool close = count > 1 && (force || vix_window_closable(win));
	if (close) {
		vix_window_close(win);
	}
	lua_pushboolean(L, close);
	return 1;
}

static const struct luaL_Reg window_funcs[] = {
	{ "__index", window_index },
	{ "__newindex", window_newindex },
	{ "selections_iterator", window_selections_iterator },
	{ "map", window_map },
	{ "unmap", window_unmap },
	{ "style_define", window_style_define },
	{ "style", window_style },
	{ "style_pos", window_style_pos },
	{ "status", window_status },
	{ "draw", window_draw },
	{ "close", window_close },
	{ NULL, NULL },
};

/***
 * Window Options
 * @table options
 * @tfield[opt=""] string breakat {brk}
 * @tfield[opt=0] int colorcolumn {cc}
 * @tfield[opt=false] boolean cursorline {cul}
 * @tfield[opt=false] boolean expandtab {et}
 * @tfield[opt=false] boolean numbers {nu}
 * @tfield[opt=false] boolean relativenumbers {rnu}
 * @tfield[opt=true] boolean showeof
 * @tfield[opt=false] boolean shownewlines
 * @tfield[opt=false] boolean showspaces
 * @tfield[opt=false] boolean showtabs
 * @tfield[opt=true] boolean statusbar
 * @tfield[opt=8] int tabwidth {tw}
 * @tfield[opt=0] int wrapcolumn {wc}
 * @see Vix.options
 */

static int window_options_index(lua_State *L) {
	Win *win = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_WIN_OPTS, offsetof(Win, view));
	if (!win) {
		return -1;
	}
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "breakat") == 0 || strcmp(key, "brk") == 0) {
			lua_pushstring(L, win->view.breakat);
			return 1;
		} else if (strcmp(key, "colorcolumn") == 0 || strcmp(key, "cc") == 0) {
			lua_pushinteger(L, win->view.colorcolumn);
			return 1;
		} else if (strcmp(key, "cursorline") == 0 || strcmp(key, "cul") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_CURSOR_LINE);
			return 1;
		} else if (strcmp(key, "expandtab") == 0 || strcmp(key, "et") == 0) {
			lua_pushboolean(L, win->expandtab);
			return 1;
		} else if (strcmp(key, "numbers") == 0 || strcmp(key, "nu") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_LINE_NUMBERS_ABSOLUTE);
			return 1;
		} else if (strcmp(key, "relativenumbers") == 0 || strcmp(key, "rnu") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_LINE_NUMBERS_RELATIVE);
			return 1;
		} else if (strcmp(key, "showeof") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_SYMBOL_EOF);
			return 1;
		} else if (strcmp(key, "shownewlines") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_SYMBOL_EOL);
			return 1;
		} else if (strcmp(key, "showspaces") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_SYMBOL_SPACE);
			return 1;
		} else if (strcmp(key, "showtabs") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_SYMBOL_TAB);
			return 1;
		} else if (strcmp(key, "statusbar") == 0) {
			lua_pushboolean(L, win->options & UI_OPTION_STATUSBAR);
			return 1;
		} else if (strcmp(key, "tabwidth") == 0 || strcmp(key, "tw") == 0) {
			lua_pushinteger(L, win->view.tabwidth);
			return 1;
		} else if (strcmp(key, "wrapcolumn") == 0 || strcmp(key, "wc") == 0) {
			lua_pushinteger(L, win->view.wrapcolumn);
			return 1;
		}
	}
	return index_common(L);
}

static int window_options_newindex(lua_State *L) {
	Win *win = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_WIN_OPTS, offsetof(Win, view));
	if (!win) {
		return 0;
	}
	if (lua_isstring(L, 2)) {
		return window_options_assign(win, L, lua_tostring(L, 2), 3);
	}
	return newindex_common(L);
}

static const struct luaL_Reg window_option_funcs[] = {
	{ "__index", window_options_index },
	{ "__newindex", window_options_newindex},
	{ NULL, NULL },
};

static int window_selections_index(lua_State *L) {
	View *view = obj_ref_check(L, 1, VIX_LUA_TYPE_SELECTIONS);
	size_t index = luaL_checkinteger(L, 2);
	size_t count = view->selection_count;
	if (index == 0 || index > count) {
		goto err;
	}
	for (Selection *s = view_selections(view); s; s = view_selections_next(s)) {
		if (!--index) {
			obj_lightref_new(L, s, VIX_LUA_TYPE_SELECTION);
			return 1;
		}
	}
err:
	lua_pushnil(L);
	return 1;
}

static int window_selections_len(lua_State *L) {
	View *view = obj_ref_check(L, 1, VIX_LUA_TYPE_SELECTIONS);
	lua_pushinteger(L, view->selection_count);
	return 1;
}

static const struct luaL_Reg window_selections_funcs[] = {
	{ "__index", window_selections_index },
	{ "__len", window_selections_len },
	{ NULL, NULL },
};

/***
 * A selection object.
 *
 * A selection is a non-empty, directed range with two endpoints called
 * *cursor* and *anchor*. A selection can be anchored in which case
 * the anchor remains fixed while only the position of the cursor is
 * adjusted. For non-anchored selections both endpoints are updated. A
 * singleton selection covers one character on which both cursor and
 * anchor reside. There always exists a primary selection which remains
 * visible (i.e. changes to its position will adjust the viewport).
 *
 * The range covered by a selection is represented as an interval whose
 * endpoints are absolute byte offsets from the start of the file.
 * Valid addresses are within the closed interval `[0, file.size]`.
 *
 * Selections are currently implemented using character marks into
 * the underlying persistent
 * [text management data structure](https://github.com/martanne/vix/wiki/Text-management-using-a-piece-chain).
 *
 * This has a few consequences you should be aware of:
 *
 *  - A selection becomes invalid when the delimiting boundaries of the underlying
 *    text it is referencing is deleted:
 *
 *        -- leaves selection in an invalid state
 *        win.file:delete(win.selection.pos, 1)
 *        assert(win.selection.pos == nil)
 *
 *    Like a regular mark it will become valid again when the text is reverted
 *    to the state before the deletion.
 *
 *  - Inserts after the selection position (`> selection.pos`) will not affect the
 *    selection position.
 *
 *        local pos = win.selection.pos
 *        win.file:insert(pos+1, "-")
 *        assert(win.selection.pos == pos)
 *
 *  - Non-cached inserts before the selection position (`<= selection.pos`) will
 *    affect the mark and adjust the selection position by the number of bytes
 *    which were inserted.
 *
 *        local pos = win.selection.pos
 *        win.file:insert(pos, "-")
 *        assert(win.selection.pos == pos+1)
 *
 *  - Cached inserts before the selection position (`<= selection.pos`) will
 *    not affect the selection position because the underlying text is replaced
 *    inplace.
 *
 * For these reasons it is generally recommended to update the selection position
 * after a modification. The general procedure amounts to:
 *
 * 1. Read out the current selection position
 * 2. Perform text modifications
 * 3. Update the selection position
 *
 * This is what @{Vix:insert} and @{Vix:replace} do internally.
 *
 * @type Selection
 * @usage
 * local data = "new text"
 * local pos = win.selection.pos
 * win.file:insert(pos, data)
 * win.selection.pos = pos + #data
 */

/***
 * The zero based byte position in the file.
 *
 * Might be `nil` if the selection is in an invalid state.
 * Setting this field will move the cursor endpoint of the
 * selection to the given position.
 * @tfield int pos
 */
/***
 * The 1-based line the cursor of this selection resides on.
 *
 * @tfield int line
 * @see to
 */
/***
 * The 1-based column position the cursor of this selection resides on.
 * @tfield int col
 * @see to
 */
/***
 * The 1-based selection index.
 * @tfield int number
 */
/***
 * The range covered by this selection.
 * @tfield Range range
 */
/***
 * Whether this selection is anchored.
 * @tfield bool anchored
 */
static int window_selection_index(lua_State *L) {
	Selection *sel = obj_lightref_check(L, 1, VIX_LUA_TYPE_SELECTION);
	if (!sel) {
		lua_pushnil(L);
		return 1;
	}

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "pos") == 0) {
			pushpos(L, view_cursors_pos(sel));
			return 1;
		}

		if (strcmp(key, "line") == 0) {
			lua_pushinteger(L, view_cursors_line(sel));
			return 1;
		}

		if (strcmp(key, "col") == 0) {
			lua_pushinteger(L, view_cursors_col(sel));
			return 1;
		}

		if (strcmp(key, "number") == 0) {
			lua_pushinteger(L, view_selections_number(sel)+1);
			return 1;
		}

		if (strcmp(key, "range") == 0) {
			Filerange range = view_selections_get(sel);
			pushrange(L, &range);
			return 1;
		}

		if (strcmp(key, "anchored") == 0) {
			lua_pushboolean(L, sel->anchored);
			return 1;
		}

	}

	return index_common(L);
}

static int window_selection_newindex(lua_State *L) {
	Selection *sel = obj_lightref_check(L, 1, VIX_LUA_TYPE_SELECTION);
	if (!sel) {
		return 0;
	}
	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "pos") == 0) {
			size_t pos = checkpos(L, 3);
			view_cursors_to(sel, pos);
			return 0;
		}

		if (strcmp(key, "range") == 0) {
			Filerange range = getrange(L, 3);
			if (text_range_valid(&range)) {
				view_selections_set(sel, &range);
				sel->anchored = true;
			} else {
				view_selection_clear(sel);
			}
			return 0;
		}

		if (strcmp(key, "anchored") == 0) {
			sel->anchored = lua_toboolean(L, 3);
			return 0;
		}
	}
	return newindex_common(L);
}

/***
 * Move cursor of selection.
 * @function to
 * @tparam int line the 1-based line number
 * @tparam int col the 1-based column number
 */
static int window_selection_to(lua_State *L) {
	Selection *sel = obj_lightref_check(L, 1, VIX_LUA_TYPE_SELECTION);
	if (sel) {
		size_t line = checkpos(L, 2);
		size_t col = checkpos(L, 3);
		view_cursors_place(sel, line, col);
	}
	return 0;
}

/***
 * Remove selection.
 * @function remove
 */
static int window_selection_remove(lua_State *L) {
	Selection *sel = obj_lightref_check(L, 1, VIX_LUA_TYPE_SELECTION);
	if (sel) {
		view_selections_dispose(sel);
	}
	return 0;
}

static const struct luaL_Reg window_selection_funcs[] = {
	{ "__index", window_selection_index },
	{ "__newindex", window_selection_newindex },
	{ "to", window_selection_to },
	{ "remove", window_selection_remove },
	{ NULL, NULL },
};

/***
 * A file object.
 * @type File
 */
/***
 * File name.
 * @tfield string name the file name relative to current working directory or `nil` if not yet named
 */
/***
 * File path.
 * @tfield string path the absolute file path or `nil` if not yet named
 */
/***
 * File content by logical lines.
 *
 * Assigning to array element `0` (`#lines+1`) will insert a new line at
 * the beginning (end) of the file.
 * @tfield Array(string) lines the file content accessible as 1-based array
 * @see content
 * @usage
 * local lines = vix.win.file.lines
 * for i=1, #lines do
 * 	lines[i] = i .. ": " .. lines[i]
 * end
 */
/***
 * File save method
 * @tfield[opt="auto"] string savemethod `"auto"`, `"atomic"`, or `"inplace"`.
 */
/***
 * File size in bytes.
 * @tfield int size the current file size in bytes
 */
/***
 * File state.
 * @tfield bool modified whether the file contains unsaved changes
 */
/***
 * File permission.
 * @tfield int permission the file permission bits as of the most recent load/save
 */
static int file_index(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);
		if (strcmp(key, "name") == 0) {
			lua_pushstring(L, file_name_get(file));
			return 1;
		}

		if (strcmp(key, "path") == 0) {
			lua_pushstring(L, file->name);
			return 1;
		}

		if (strcmp(key, "lines") == 0) {
			obj_ref_new(L, file->text, VIX_LUA_TYPE_TEXT);
			return 1;
		}

		if (strcmp(key, "size") == 0) {
			lua_pushinteger(L, text_size(file->text));
			return 1;
		}

		if (strcmp(key, "modified") == 0) {
			lua_pushboolean(L, text_modified(file->text));
			return 1;
		}

		if (strcmp(key, "permission") == 0) {
			struct stat stat = text_stat(file->text);
			lua_pushinteger(L, stat.st_mode & 0777);
			return 1;
		}

		if (strcmp(key, "savemethod") == 0) {
			switch (file->save_method) {
			case TEXT_SAVE_AUTO:
				lua_pushliteral(L, "auto");
				break;
			case TEXT_SAVE_ATOMIC:
				lua_pushliteral(L, "atomic");
				break;
			case TEXT_SAVE_INPLACE:
				lua_pushliteral(L, "inplace");
				break;
			}
			return 1;
		}
	}

	return index_common(L);
}

static int file_newindex(lua_State *L)
{
	Vix  *vix  = lua_get_vix(L);
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);

	if (lua_isstring(L, 2)) {
		const char *key = lua_tostring(L, 2);

		if (strcmp(key, "modified") == 0) {
			bool modified = lua_isboolean(L, 3) && lua_toboolean(L, 3);
			if (modified) {
				text_insert(vix, file->text, 0, " ", 1);
				text_delete(file->text, 0, 1);
			} else {
				text_mark_current_revision(file->text);
			}
			return 0;
		}

		if (strcmp(key, "savemethod") == 0) {
			if (!lua_isstring(L, 3)) {
				return newindex_common(L);
			}
			const char *sm = lua_tostring(L, 3);
			if (strcmp(sm, "auto") == 0) {
				file->save_method = TEXT_SAVE_AUTO;
			} else if (strcmp(sm, "atomic") == 0) {
				file->save_method = TEXT_SAVE_ATOMIC;
			} else if (strcmp(sm, "inplace") == 0) {
				file->save_method = TEXT_SAVE_INPLACE;
			}
			return 0;
		}
	}

	return newindex_common(L);
}

/***
 * Insert data at position.
 * @function insert
 * @tparam int pos the 0-based file position in bytes
 * @tparam string data the data to insert
 * @treturn bool whether the file content was successfully changed
 */
static int file_insert(lua_State *L)
{
	Vix  *vix  = lua_get_vix(L);
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	size_t pos = checkpos(L, 2);
	size_t len;
	luaL_checkstring(L, 3);
	const char *data = lua_tolstring(L, 3, &len);
	lua_pushboolean(L, text_insert(vix, file->text, pos, data, len));
	return 1;
}

/***
 * Delete data at position.
 *
 * @function delete
 * @tparam int pos the 0-based file position in bytes
 * @tparam int len the length in bytes to delete
 * @treturn bool whether the file content was successfully changed
 */
/***
 * Delete file range.
 *
 * @function delete
 * @tparam Range range the range to delete
 * @treturn bool whether the file content was successfully changed
 */
static int file_delete(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	Filerange range = getrange(L, 2);
	lua_pushboolean(L, text_delete_range(file->text, &range));
	return 1;
}

/***
 * Create a restore point for undo/redo.
 *
 * @function snapshot
 */
static int file_snapshot(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	text_snapshot(file->text);
	return 0;
}

/***
 * Create an iterator over all lines of the file.
 *
 * For large files this is probably faster than @{lines}.
 * @function lines_iterator
 * @return the new iterator
 * @see lines
 * @usage
 * for line in file:lines_iterator() do
 * 	-- do something with line
 * end
 */
static int file_lines_iterator_it(lua_State *L);
static int file_lines_iterator(lua_State *L) {
	Vix *vix = lua_get_vix(L);
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	size_t line = luaL_optinteger(L, 2, 1);
	size_t *pos = lua_newuserdata(L, sizeof *pos);
	*pos = text_pos_by_lineno(vix, file->text, line);
	lua_pushcclosure(L, file_lines_iterator_it, 2);
	return 1;
}

static int file_lines_iterator_it(lua_State *L) {
	File *file = *(File**)lua_touserdata(L, lua_upvalueindex(1));
	size_t *start = lua_touserdata(L, lua_upvalueindex(2));
	if (*start == text_size(file->text)) {
		return 0;
	}
	size_t end = text_line_end(file->text, *start);
	size_t len = end - *start;
	char *buf = lua_newuserdata(L, len);
	if (!buf && len) {
		return 0;
	}
	len = text_bytes_get(file->text, *start, len, buf);
	lua_pushlstring(L, buf, len);
	*start = text_line_next(file->text, end);
	return 1;
}

/***
 * Get file content of position and length.
 *
 * @function content
 * @tparam int pos the 0-based file position in bytes
 * @tparam int len the length in bytes to read
 * @treturn string the file content corresponding to the range
 * @see lines
 * @usage
 * local file = vix.win.file
 * local text = file:content(0, file.size)
 */
/***
 * Get file content of range.
 *
 * @function content
 * @tparam Range range the range to read
 * @treturn string the file content corresponding to the range
 */
static int file_content(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	Filerange range = getrange(L, 2);
	if (!text_range_valid(&range)) {
		goto err;
	}
	size_t len = text_range_size(&range);
	char *data = lua_newuserdata(L, len);
	if (!data) {
		goto err;
	}
	len = text_bytes_get(file->text, range.start, len, data);
	lua_pushlstring(L, data, len);
	return 1;
err:
	lua_pushnil(L);
	return 1;
}

/***
 * Set mark.
 * @function mark_set
 * @tparam int pos the position to set the mark to, must be in [0, file.size]
 * @treturn Mark mark the mark which can be looked up later
 */
static int file_mark_set(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	size_t pos = checkpos(L, 2);
	Mark mark = text_mark_set(file->text, pos);
	if (mark) {
		obj_lightref_new(L, (void*)mark, VIX_LUA_TYPE_MARK);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/***
 * Get position of mark.
 * @function mark_get
 * @tparam Mark mark the mark to look up
 * @treturn int pos the position of the mark, or `nil` if invalid
 */
static int file_mark_get(lua_State *L) {
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	Mark mark = (Mark)obj_lightref_check(L, 2, VIX_LUA_TYPE_MARK);
	size_t pos = text_mark_get(file->text, mark);
	if (pos == EPOS) {
		lua_pushnil(L);
	} else {
		lua_pushinteger(L, pos);
	}
	return 1;
}

/***
 * Word text object.
 *
 * @function text_object_word
 * @tparam int pos the position which must be part of the word
 * @treturn Range range the range
 */

/***
 * WORD text object.
 *
 * @function text_object_longword
 * @tparam int pos the position which must be part of the word
 * @treturn Range range the range
 */

static int file_text_object(lua_State *L) {
	Filerange range = text_range_empty();
	Vix *vix = lua_get_vix(L);
	File *file = obj_ref_check(L, 1, VIX_LUA_TYPE_FILE);
	size_t pos = checkpos(L, 2);
	size_t idx = lua_tointeger(L, lua_upvalueindex(1));
	if (idx < LENGTH(vix_textobjects)) {
		const TextObject *txtobj = &vix_textobjects[idx];
		if (txtobj->vix) {
			range = txtobj->vix(vix, file->text, pos);
		} else if (txtobj->txt) {
			range = txtobj->txt(file->text, pos);
		}
	}
	pushrange(L, &range);
	return 1;
}

static const struct luaL_Reg file_funcs[] = {
	{ "__index", file_index },
	{ "__newindex", file_newindex },
	{ "insert", file_insert },
	{ "delete", file_delete },
	{ "snapshot", file_snapshot },
	{ "lines_iterator", file_lines_iterator },
	{ "content", file_content },
	{ "mark_set", file_mark_set },
	{ "mark_get", file_mark_get },
	{ NULL, NULL },
};

static int file_lines_index(lua_State *L) {
	Vix *vix = lua_get_vix(L);
	Text *txt = obj_ref_check(L, 1, VIX_LUA_TYPE_TEXT);
	size_t line = luaL_checkinteger(L, 2);
	size_t start = text_pos_by_lineno(vix, txt, line);
	size_t end = text_line_end(txt, start);
	if (start != EPOS && end != EPOS) {
		size_t size = end - start;
		char *data = lua_newuserdata(L, size);
		if (!data && size) {
			goto err;
		}
		size = text_bytes_get(txt, start, size, data);
		lua_pushlstring(L, data, size);
		return 1;
	}
err:
	lua_pushnil(L);
	return 1;
}

static int file_lines_newindex(lua_State *L)
{
	Vix  *vix = lua_get_vix(L);
	Text *txt = obj_ref_check(L, 1, VIX_LUA_TYPE_TEXT);
	size_t line = luaL_checkinteger(L, 2);
	size_t size;
	const char *data = luaL_checklstring(L, 3, &size);
	if (line == 0) {
		text_insert(vix, txt, 0, data, size);
		text_insert(vix, txt, size, "\n", 1);
		return 0;
	}
	size_t start = text_pos_by_lineno(vix, txt, line);
	size_t end = text_line_end(txt, start);
	if (start != EPOS && end != EPOS) {
		text_delete(txt, start, end - start);
		text_insert(vix, txt, start, data, size);
		if (text_size(txt) == start + size) {
			text_insert(vix, txt, text_size(txt), "\n", 1);
		}
	}
	return 0;
}

static int file_lines_len(lua_State *L) {
	Vix *vix = lua_get_vix(L);
	Text *txt = obj_ref_check(L, 1, VIX_LUA_TYPE_TEXT);
	size_t lines = 0;
	char lastchar;
	size_t size = text_size(txt);
	if (size > 0) {
		lines = text_lineno_by_pos(vix, txt, size);
	}
	if (lines > 1 && text_byte_get(txt, size-1, &lastchar) && lastchar == '\n') {
		lines--;
	}
	lua_pushinteger(L, lines);
	return 1;
}

static const struct luaL_Reg file_lines_funcs[] = {
	{ "__index", file_lines_index },
	{ "__newindex", file_lines_newindex },
	{ "__len", file_lines_len },
	{ NULL, NULL },
};

static int window_marks_index(lua_State *L) {
	lua_newtable(L);
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	Win *win = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_MARKS, offsetof(Win, saved_selections));
	if (!win) {
		return 1;
	}
	const char *symbol = luaL_checkstring(L, 2);
	if (strlen(symbol) != 1) {
		return 1;
	}
	enum VixMark mark = vix_mark_from(vix, symbol[0]);
	if (mark == VIX_MARK_INVALID) {
		return 1;
	}

	FilerangeList ranges = vix_mark_get(vix, win, mark);
	for (VixDACount i = 0; i < ranges.count; i++) {
		lua_pushinteger(L, i+1);
		pushrange(L, ranges.data + i);
		lua_settable(L, -3);
	}
	da_release(&ranges);
	return 1;
}

static int window_marks_newindex(lua_State *L) {
	Vix *vix = lua_touserdata(L, lua_upvalueindex(1));
	Win *win = obj_ref_check_containerof(L, 1, VIX_LUA_TYPE_MARKS, offsetof(Win, saved_selections));
	if (!win) {
		return 0;
	}
	const char *symbol = luaL_checkstring(L, 2);
	if (strlen(symbol) != 1) {
		return 0;
	}
	enum VixMark mark = vix_mark_from(vix, symbol[0]);
	if (mark == VIX_MARK_INVALID) {
		return 0;
	}

	FilerangeList ranges = {0};

	if (lua_istable(L, 3)) {
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			Filerange range = getrange(L, -1);
			if (text_range_valid(&range)) {
				*da_push(vix, &ranges) = range;
			}
			lua_pop(L, 1);
		}
	}

	vix_mark_set(vix, win, mark, ranges);
	da_release(&ranges);
	return 0;
}

static int window_marks_len(lua_State *L) {
	lua_pushinteger(L, VIX_MARK_INVALID);
	return 1;
}

static const struct luaL_Reg window_marks_funcs[] = {
	{ "__index", window_marks_index },
	{ "__newindex", window_marks_newindex },
	{ "__len", window_marks_len },
	{ NULL, NULL },
};

/***
 * A file range.
 *
 * For a valid range `start <= finish` holds.
 * An invalid range is represented as `nil`.
 * @type Range
 */
/***
 * The beginning of the range.
 * @tfield int start
 */
/***
 * The end of the range.
 * @tfield int finish
 */

/***
 * Layouts.
 * @section Layouts
 */

/***
 * Layout Constants.
 * @table layouts
 * @tfield int HORIZONTAL
 * @tfield int VERTICAL
 */

/***
 * Modes.
 * @section Modes
 */

/***
 * Mode constants.
 * @table modes
 * @tfield int NORMAL
 * @tfield int OPERATOR_PENDING
 * @tfield int INSERT
 * @tfield int REPLACE
 * @tfield int VISUAL
 * @tfield int VISUAL_LINE
 * @see Vix:map
 * @see Window:map
 */

/***
 * Key Handling.
 *
 * This section describes the contract between the editor core and Lua
 * key handling functions mapped to symbolic keys using either @{Vix:map}
 * or @{Window:map}.
 *
 * @section Key_Handling
 */

/***
 * Example of a key handling function.
 *
 * The keyhandler is invoked with the pending content of the input queue
 * given as argument. This might be the empty string if no further input
 * is available.
 *
 * The function is expected to return the number of *bytes* it has
 * consumed from the passed input keys. A negative return value is
 * interpreted as an indication that not enough input was available. The
 * function will be called again once the user has provided more input. A
 * missing return value (i.e. `nil`) is interpreted as zero, meaning
 * no further input was consumed but the function completed successfully.
 *
 * @function keyhandler
 * @tparam string keys the keys following the mapping
 * @treturn int the number of *bytes* being consumed by the function (see above)
 * @see Vix:action_register
 * @see Vix:map
 * @see Window:map
 * @usage
 * vix:map(vix.modes.INSERT, "<C-k>", function(keys)
 * 	if #keys < 2 then
 * 		return -1 -- need more input
 * 	end
 * 	local digraph = keys:sub(1, 2)
 * 	if digraph == "l*" then
 * 		vix:feedkeys('λ')
 * 		return 2 -- consume 2 bytes of input
 * 	end
 * end, "Insert digraph")
 */

/***
 * Core Events.
 *
 * These events are invoked from the editor core.
 * The following functions are invoked if they are registered in the
 * `vix.events` table. Users scripts should generally use the [Events](#events)
 * mechanism instead which multiplexes these core events.
 *
 * @section Core_Events
 */

static void vix_lua_event_get(lua_State *L, const char *name) {
	lua_getglobal(L, "vix");
	lua_getfield(L, -1, "events");
	if (lua_istable(L, -1)) {
		lua_getfield(L, -1, name);
	}
	lua_remove(L, -2);
}

static void vix_lua_event_call(Vix *vix, const char *name) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, name);
	if (lua_isfunction(L, -1)) {
		pcall(vix, L, 0, 0);
	}
	lua_pop(L, 1);
}

#if LUA_VERSION_NUM >= 502
#define VIX_LUA_VERSION LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
#else
#define VIX_LUA_VERSION "5.1"
#endif

static bool vix_lua_path_strip(Vix *vix) {
	lua_State *L = vix->lua;
	lua_getglobal(L, "package");

	for (const char **var = (const char*[]){ "path", "cpath", NULL }; *var; var++) {

		lua_getfield(L, -1, *var);
		const char *path = lua_tostring(L, -1);
		lua_pop(L, 1);
		if (!path) {
			return false;
		}

		char *copy = strdup(path), *stripped = calloc(1, strlen(path)+2);
		if (!copy || !stripped) {
			free(copy);
			free(stripped);
			return false;
		}

		for (char *elem = copy, *stripped_elem = stripped, *next; elem; elem = next) {
			if ((next = (strstr)(elem, ";"))) {
				*next++ = '\0';
			}
			if ((strstr)(elem, "./")) {
				continue; /* skip relative path entries */
			}

			/* skip paths with mismatched lua version */
			char *ver = (strstr)(elem, "/lua/");
			if (ver) {
				ver += 5; // skip "/lua/"
				/* if it contains a version string, check if it matches ours.
				 * we allow it if it starts with our version string (e.g. 5.1)
				 * followed by a separator (/ or ;) or end of string.
				 */
				if (isdigit((unsigned char)ver[0])) {
					size_t vlen = strlen(VIX_LUA_VERSION);
					if (strncmp(ver, VIX_LUA_VERSION, vlen) != 0 ||
					    (ver[vlen] != '\0' && ver[vlen] != '/' && ver[vlen] != ';')) {
						continue;
					}
				}
			}

			stripped_elem += sprintf(stripped_elem, "%s;", elem);
		}

		lua_pushstring(L, stripped);
		lua_setfield(L, -2, *var);

		free(copy);
		free(stripped);
	}

	lua_pop(L, 1); /* package */
	return true;
}

bool vix_lua_path_add(Vix *vix, const char *path) {
	if (!path) {
		return false;
	}
	lua_State *L = vix->lua;
	lua_getglobal(L, "package");
	lua_pushstring(L, path);
	lua_pushstring(L, "/?.lua;");
	lua_pushstring(L, path);
	lua_pushstring(L, "/?/init.lua;");
	lua_getfield(L, -5, "path");
	lua_concat(L, 5);
	lua_setfield(L, -2, "path");
	lua_pop(L, 1); /* package */
	return true;
}

bool vix_lua_cpath_add(Vix *vix, const char *path) {
	if (!path) {
		return false;
	}
	lua_State *L = vix->lua;
	lua_getglobal(L, "package");
	lua_pushstring(L, path);
	lua_pushstring(L, "/?.so;");
	lua_getfield(L, -3, "cpath");
	lua_concat(L, 3);
	lua_setfield(L, -2, "cpath");
	lua_pop(L, 1); /* package */
	return true;
}

bool vix_lua_paths_get(Vix *vix, char **lpath, char **cpath) {
	lua_State *L = vix->lua;
	const char *s;
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "path");
	s = lua_tostring(L, -1);
	*lpath = s ? strdup(s) : NULL;
	lua_getfield(L, -2, "cpath");
	s = lua_tostring(L, -1);
	*cpath = s ? strdup(s) : NULL;
	return true;
}

static bool package_exist(Vix *vix, lua_State *L, str8 name)
{
	const char lua[] =
		"local name = ...\n"
		"for _, searcher in ipairs(package.searchers or package.loaders) do\n"
			"local loader = searcher(name)\n"
			"if type(loader) == 'function' then\n"
				"return true\n"
			"end\n"
		"end\n"
		"return false\n";
	if (luaL_loadstring(L, lua) != LUA_OK) {
		return false;
	}
	lua_pushlstring(L, (char *)name.data, name.length);
	lua_insert(L, -2);
	/* an error indicates package exists */
	bool ret = lua_pcall(L, 1, 1, 0) != LUA_OK || lua_toboolean(L, -1);
	lua_pop(L, 1);
	return ret;
}

static void *alloc_lua(void *ud, void *ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		free(ptr);
		return NULL;
	} else {
		return realloc(ptr, nsize);
	}
}

/***
 * Editor initialization completed.
 * This event is emitted immediately after `vixrc.lua` has been sourced, but
 * before any other events have occurred, in particular the command line arguments
 * have not yet been processed.
 *
 * Can be used to set *global* configuration options.
 * @function init
 */
static void vix_lua_init(Vix *vix) {
	lua_State *L;
#if LUA_VERSION_NUM >= 505
	L = lua_newstate(alloc_lua, vix, luaL_makeseed(0));
#else
	L = lua_newstate(alloc_lua, vix);
#endif

	if (!L) {
		vix_die(vix, "Failed to start Lua\n");
	}
	vix->lua = L;
	lua_atpanic(L, &panic_handler);

	luaL_openlibs(L);

#if CONFIG_LPEG
	extern int luaopen_lpeg(lua_State *L);
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "preload");
	lua_pushcfunction(L, luaopen_lpeg);
	lua_setfield(L, -2, "lpeg");
	lua_pop(L, 2);
#endif

	/* remove any relative paths from lua's default package.path */
	vix_lua_path_strip(vix);

	/* extends lua's package.path with:
	 * - $VIX_PATH
	 * - ./lua (relative path to the binary location)
	 * - $XDG_CONFIG_HOME/vix (defaulting to $HOME/.config/vix)
	 * - /etc/vix (for system-wide configuration provided by administrator)
	 * - /usr/(local/)?share/vix (or whatever is specified during ./configure)
	 * - package.path (standard lua search path)
	 */
	char path[PATH_MAX];

	vix_lua_path_add(vix, VIX_PATH);

	/* try to get users home directory */
	const char *home = getenv("HOME");
	if (!home || !*home) {
		struct passwd *pw = getpwuid(getuid());
		if (pw) {
			home = pw->pw_dir;
		}
	}

	vix_lua_path_add(vix, "/etc/vix");

	/* add standard system paths for the current lua version */
	snprintf(path, sizeof path, "/usr/local/lib/lua/%s", VIX_LUA_VERSION);
	vix_lua_path_add(vix, path);
	vix_lua_cpath_add(vix, path);
	snprintf(path, sizeof path, "/usr/local/share/lua/%s", VIX_LUA_VERSION);
	vix_lua_path_add(vix, path);
	snprintf(path, sizeof path, "/usr/lib/lua/%s", VIX_LUA_VERSION);
	vix_lua_path_add(vix, path);
	vix_lua_cpath_add(vix, path);
	snprintf(path, sizeof path, "/usr/share/lua/%s", VIX_LUA_VERSION);
	vix_lua_path_add(vix, path);

	if (home && *home) {
		snprintf(path, sizeof path, "%s/.luarocks/share/lua/%s", home, VIX_LUA_VERSION);
		vix_lua_path_add(vix, path);
		snprintf(path, sizeof path, "%s/.luarocks/lib/lua/%s", home, VIX_LUA_VERSION);
		vix_lua_cpath_add(vix, path);
	}

	const char *xdg_config = getenv("XDG_CONFIG_HOME");
	if (xdg_config) {
		snprintf(path, sizeof path, "%s/vix", xdg_config);
		vix_lua_path_add(vix, path);
	} else if (home && *home) {
		snprintf(path, sizeof path, "%s/.config/vix", home);
		vix_lua_path_add(vix, path);
	}

	ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
	if (len > 0) {
		str8 dir, tail = str8("/lua");
		path_split((str8){.length = len, .data = (uint8_t *)path}, &dir, 0);
		if (dir.length + tail.length + 1 < sizeof(path)) {
			memcpy(path + dir.length, tail.data, tail.length + 1);
			vix_lua_path_add(vix, path);
		}
	}

	vix_lua_path_add(vix, getenv("VIX_PATH"));

	/* table in registry to lookup object type, stores metatable -> type mapping */
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "vix.types");
	/* table in registry to track lifetimes of C objects */
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "vix.objects");
	/* table in registry to store references to Lua functions */
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "vix.functions");
	/* metatable used to type check user data */
	obj_type_new(L, str8(VIX_LUA_TYPE_VIX));
	luaL_setfuncs(L, vix_lua, 0);
	lua_newtable(L);
	lua_setfield(L, -2, "types");
	/* create reference to main vix object, such that the further
	 * calls to obj_type_new can register the type meta tables in
	 * vix.types[name] */
	obj_ref_new(L, vix, "vix");
	lua_setglobal(L, "vix");

	obj_type_new(L, str8(VIX_LUA_TYPE_FILE));

	const struct {
		enum VixTextObject id;
		const char *name;
	} textobjects[] = {
		{ VIX_TEXTOBJECT_INNER_WORD, "text_object_word" },
		{ VIX_TEXTOBJECT_INNER_LONGWORD, "text_object_longword" },
	};

	for (size_t i = 0; i < LENGTH(textobjects); i++) {
		lua_pushinteger(L, textobjects[i].id);
		lua_pushcclosure(L, file_text_object, 1);
		lua_setfield(L, -2, textobjects[i].name);
	}

	luaL_setfuncs(L, file_funcs, 0);

	obj_type_new(L, str8(VIX_LUA_TYPE_TEXT));
	luaL_setfuncs(L, file_lines_funcs, 0);
	obj_type_new(L, str8(VIX_LUA_TYPE_WINDOW));
	luaL_setfuncs(L, window_funcs, 0);

	const struct {
		enum UiStyle id;
		const char *name;
	} styles[] = {
		{ UI_STYLE_LEXER_MAX,         "STYLE_LEXER_MAX"         },
		{ UI_STYLE_DEFAULT,           "STYLE_DEFAULT"           },
		{ UI_STYLE_CURSOR,            "STYLE_CURSOR"            },
		{ UI_STYLE_CURSOR_PRIMARY,    "STYLE_CURSOR_PRIMARY"    },
		{ UI_STYLE_CURSOR_LINE,       "STYLE_CURSOR_LINE"       },
		{ UI_STYLE_SELECTION,         "STYLE_SELECTION"         },
		{ UI_STYLE_LINENUMBER,        "STYLE_LINENUMBER"        },
		{ UI_STYLE_LINENUMBER_CURSOR, "STYLE_LINENUMBER_CURSOR" },
		{ UI_STYLE_COLOR_COLUMN,      "STYLE_COLOR_COLUMN"      },
		{ UI_STYLE_STATUS,            "STYLE_STATUS"            },
		{ UI_STYLE_STATUS_FOCUSED,    "STYLE_STATUS_FOCUSED"    },
		{ UI_STYLE_TAB,               "STYLE_TAB"               },
		{ UI_STYLE_TAB_FOCUSED,       "STYLE_TAB_FOCUSED"       },
		{ UI_STYLE_SEPARATOR,         "STYLE_SEPARATOR"         },
		{ UI_STYLE_INFO,              "STYLE_INFO"              },
		{ UI_STYLE_EOF,               "STYLE_EOF"               },
		{ UI_STYLE_WHITESPACE,        "STYLE_WHITESPACE"        },
	};

	for (size_t i = 0; i < LENGTH(styles); i++) {
		lua_pushinteger(L, styles[i].id);
		lua_setfield(L, -2, styles[i].name);
	}

	obj_type_new(L, str8(VIX_LUA_TYPE_WIN_OPTS));
	luaL_setfuncs(L, window_option_funcs, 0);

	obj_type_new(L, str8(VIX_LUA_TYPE_MARK));
	obj_type_new(L, str8(VIX_LUA_TYPE_MARKS));
	lua_pushlightuserdata(L, vix);
	luaL_setfuncs(L, window_marks_funcs, 1);

	obj_type_new(L, str8(VIX_LUA_TYPE_SELECTION));
	luaL_setfuncs(L, window_selection_funcs, 0);
	obj_type_new(L, str8(VIX_LUA_TYPE_SELECTIONS));
	luaL_setfuncs(L, window_selections_funcs, 0);

	obj_type_new(L, str8(VIX_LUA_TYPE_UI));
	luaL_setfuncs(L, ui_funcs, 0);
	lua_pushinteger(L, ui_terminal_colors());
	lua_setfield(L, -2, "colors");
	lua_newtable(L);
	static const struct {
		enum UiLayout id;
		const char *name;
	} layouts[] = {
		{ UI_LAYOUT_HORIZONTAL, "HORIZONTAL" },
		{ UI_LAYOUT_VERTICAL, "VERTICAL" },
	};
	for (size_t i = 0; i <  LENGTH(layouts); i++) {
		lua_pushinteger(L, layouts[i].id);
		lua_setfield(L, -2, layouts[i].name);
	}
	lua_setfield(L, -2, "layouts");

	obj_type_new(L, str8(VIX_LUA_TYPE_REGISTERS));
	lua_pushlightuserdata(L, vix);
	luaL_setfuncs(L, registers_funcs, 1);

	obj_type_new(L, str8(VIX_LUA_TYPE_KEYACTION));

	lua_getglobal(L, "vix");
	lua_getmetatable(L, -1);

	lua_pushliteral(L, VERSION);
	lua_setfield(L, -2, "VERSION");

	lua_newtable(L);
	static const struct {
		enum VixMode id;
		const char *name;
	} modes[] = {
		{ VIX_MODE_NORMAL,           "NORMAL"           },
		{ VIX_MODE_OPERATOR_PENDING, "OPERATOR_PENDING" },
		{ VIX_MODE_VISUAL,           "VISUAL"           },
		{ VIX_MODE_VISUAL_LINE,      "VISUAL_LINE"      },
		{ VIX_MODE_INSERT,           "INSERT"           },
		{ VIX_MODE_REPLACE,          "REPLACE"          },
		{ VIX_MODE_WINDOW,           "WINDOW"           },
	};
	for (size_t i = 0; i < LENGTH(modes); i++) {
		lua_pushinteger(L, modes[i].id);
		lua_setfield(L, -2, modes[i].name);
	}
	lua_setfield(L, -2, "modes");

	obj_type_new(L, str8(VIX_LUA_TYPE_VIX_OPTS));
	luaL_setfuncs(L, vix_option_funcs, 0);

	if (!package_exist(vix, L, str8("vixrc"))) {
		vix_info_show(vix, "WARNING: failed to load vixrc.lua");
	} else {
		lua_getglobal(L, "require");
		lua_pushliteral(L, "vixrc");
		pcall(vix, L, 1, 0);
		vix_lua_event_call(vix, "init");
	}
}

/***
 * Editor startup completed.
 * This event is emitted immediately before the main loop starts.
 * At this point all files are loaded and corresponding windows are created.
 * We are about to process interactive keyboard input.
 * @function start
 */
static void vix_lua_start(Vix *vix) {
	vix_lua_event_call(vix, "start");
}

/**
 * Editor is about to terminate.
 * @function quit
 */
static void vix_lua_quit(Vix *vix) {
	vix_lua_event_call(vix, "quit");
	lua_close(vix->lua);
	vix->lua = NULL;
}

/***
 * Input key event in either input or replace mode.
 * @function input
 * @tparam string key
 * @treturn bool whether the key was consumed or not
 */
static bool vix_lua_input(Vix *vix, const char *key, size_t len) {
	if (!vix->win || vix->win->file->internal) {
		return false;
	}
	bool ret = false;
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "input");
	if (lua_isfunction(L, -1)) {
		lua_pushlstring(L, key, len);
		if (pcall(vix, L, 1, 1) == 0) {
			ret = lua_isboolean(L, -1) && lua_toboolean(L, -1);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	return ret;
}

void vix_event_mode_insert_input(Vix *vix, const char *key, size_t len) {
	if (!vix_lua_input(vix, key, len)) {
		vix_insert_key(vix, key, len);
	}
}

void vix_event_mode_replace_input(Vix *vix, const char *key, size_t len) {
	if (!vix_lua_input(vix, key, len)) {
		vix_replace_key(vix, key, len);
	}
}

/***
 * File open.
 * @function file_open
 * @tparam File file the file to be opened
 */
static void vix_lua_file_open(Vix *vix, File *file) {
	debug("event: file-open: %s %p %p\n", file->name ? file->name : "unnamed", (void*)file, (void*)file->text);
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "file_open");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, file, VIX_LUA_TYPE_FILE);
		pcall(vix, L, 1, 0);
	}
	lua_pop(L, 1);
}

/***
 * File pre save.
 * Triggered *before* the file is being written.
 * @function file_save_pre
 * @tparam File file the file being written
 * @tparam string path the absolute path to which the file will be written, `nil` if standard output
 * @treturn bool whether the write operation should be proceeded
 */
static bool vix_lua_file_save_pre(Vix *vix, File *file, const char *path) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "file_save_pre");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, file, VIX_LUA_TYPE_FILE);
		lua_pushstring(L, path);
		if (pcall(vix, L, 2, 1) != 0) {
			return false;
		}
		return !lua_isboolean(L, -1) || lua_toboolean(L, -1);
	}
	lua_pop(L, 1);
	return true;
}

/***
 * File post save.
 * Triggered *after* a successful write operation.
 * @function file_save_post
 * @tparam File file the file which was written
 * @tparam string path the absolute path to which it was written, `nil` if standard output
 */
static void vix_lua_file_save_post(Vix *vix, File *file, const char *path) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "file_save_post");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, file, VIX_LUA_TYPE_FILE);
		lua_pushstring(L, path);
		pcall(vix, L, 2, 0);
	}
	lua_pop(L, 1);
}

/***
 * File close.
 * The last window displaying the file has been closed.
 * @function file_close
 * @tparam File file the file being closed
 */
static void vix_lua_file_close(Vix *vix, File *file) {
	debug("event: file-close: %s %p %p\n", file->name ? file->name : "unnamed", (void*)file, (void*)file->text);
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "file_close");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, file, VIX_LUA_TYPE_FILE);
		pcall(vix, L, 1, 0);
	}
	obj_ref_free(L, file->marks);
	obj_ref_free(L, file->text);
	obj_ref_free(L, file);
	lua_pop(L, 1);
}

/***
 * Window open.
 * A new window has been created.
 * @function win_open
 * @tparam Window win the window being opened
 */
static void vix_lua_win_open(Vix *vix, Win *win) {
	debug("event: win-open: %s %p %p\n", win->file->name ? win->file->name : "unnamed", (void*)win, (void*)win->view);
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "win_open");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW);
		pcall(vix, L, 1, 0);
	}
	lua_pop(L, 1);
}

/***
 * Window close.
 * An window is being closed.
 * @function win_close
 * @tparam Window win the window being closed
 */
static void vix_lua_win_close(Vix *vix, Win *win) {
	debug("event: win-close: %s %p %p\n", win->file->name ? win->file->name : "unnamed", (void*)win, (void*)win->view);
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "win_close");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW);
		pcall(vix, L, 1, 0);
	}
	obj_ref_free(L, &win->view);
	obj_ref_free(L, win);
	lua_pop(L, 1);
}

/**
 * Window highlight.
 * The window has been redrawn and the syntax highlighting needs to be performed.
 * @function win_highlight
 * @tparam Window win the window being redrawn
 * @see style
 */
static void vix_lua_win_highlight(Vix *vix, Win *win) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "win_highlight");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW);
		pcall(vix, L, 1, 0);
	}
	lua_pop(L, 1);
}

/***
 * Window status bar redraw.
 * @function win_status
 * @tparam Window win the affected window
 * @see status
 */
static void vix_lua_win_status(Vix *vix, Win *win) {
	if (win->file->internal) {
		window_status_update(vix, win);
		return;
	}
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "win_status");
	if (lua_isfunction(L, -1)) {
		obj_ref_new(L, win, VIX_LUA_TYPE_WINDOW);
		pcall(vix, L, 1, 0);
	} else {
		window_status_update(vix, win);
	}
	lua_pop(L, 1);
}

/***
 * CSI command received from terminal.
 * @function term_csi
 * @param List of CSI parameters
 */
static void vix_lua_term_csi(Vix *vix, const long *csi) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "term_csi");
	if (lua_isfunction(L, -1)) {
		int nargs = csi[1];
		lua_pushinteger(L, csi[0]);
		for (int i = 0; i < nargs; i++) {
			lua_pushinteger(L, csi[2 + i]);
		}
		pcall(vix, L, 1 + nargs, 0);
	}
	lua_pop(L, 1);
}
/***
 * The response received from the process started via @{Vix:communicate}.
 * @function process_response
 * @tparam string name the name of process given to @{Vix:communicate}
 * @tparam string response_type can be "STDOUT" or "STDERR" if new output was received in corresponding channel, "SIGNAL" if the process was terminated by a signal or "EXIT" when the process terminated normally
 * @tparam int code the exit code number if response_type is "EXIT", or the signal number if response_type is "SIGNAL"
 * @tparam string buffer the available content sent by the process
 */
void vix_lua_process_response(Vix *vix, const char *name,
                              char *buffer, size_t len, ResponseType rtype) {
	lua_State *L = vix->lua;
	vix_lua_event_get(L, "process_response");
	if (lua_isfunction(L, -1)) {
		lua_pushstring(L, name);
		switch (rtype) {
		case EXIT:   lua_pushliteral(L, "EXIT");   break;
		case SIGNAL: lua_pushliteral(L, "SIGNAL"); break;
		case STDERR: lua_pushliteral(L, "STDERR"); break;
		case STDOUT: lua_pushliteral(L, "STDOUT"); break;
		}
		switch (rtype) {
		case EXIT:
		case SIGNAL:
			lua_pushinteger(L, len);
			lua_pushnil(L);
			break;
		default:
			lua_pushnil(L);
			lua_pushlstring(L, buffer, len);
		}
		pcall(vix, L, 4, 0);
	}
	lua_pop(L, 1);
}

/***
 * Emitted immediately before the UI is drawn to the screen.
 * Allows last-minute overrides to the styling of UI elements.
 *
 * *WARNING:* This is emitted every screen draw!
 * Use sparingly and check for `nil` values!
 * @function ui_draw
 */
static void vix_lua_ui_draw(Vix *vix) {
	vix_lua_event_call(vix, "ui_draw");
}

bool vix_event_emit(Vix *vix, enum VixEvents id, ...) {
	va_list ap;
	va_start(ap, id);
	bool ret = true;

	switch (id) {
	case VIX_EVENT_INIT:
		vix_lua_init(vix);
		break;
	case VIX_EVENT_START:
		vix_lua_start(vix);
		break;
	case VIX_EVENT_FILE_OPEN:
	case VIX_EVENT_FILE_SAVE_PRE:
	case VIX_EVENT_FILE_SAVE_POST:
	case VIX_EVENT_FILE_CLOSE:
	{
		File *file = va_arg(ap, File*);
		if (file->internal) {
			break;
		}
		if (id == VIX_EVENT_FILE_OPEN) {
			vix_lua_file_open(vix, file);
		} else if (id == VIX_EVENT_FILE_SAVE_PRE) {
			const char *path = va_arg(ap, const char*);
			ret = vix_lua_file_save_pre(vix, file, path);
		} else if (id == VIX_EVENT_FILE_SAVE_POST) {
			const char *path = va_arg(ap, const char*);
			vix_lua_file_save_post(vix, file, path);
		} else if (id == VIX_EVENT_FILE_CLOSE) {
			vix_lua_file_close(vix, file);
		}
		break;
	}
	case VIX_EVENT_WIN_STATUS: {
		vix_lua_win_status(vix, va_arg(ap, Win*));
	} break;
	case VIX_EVENT_WIN_OPEN:
	case VIX_EVENT_WIN_CLOSE:
	case VIX_EVENT_WIN_HIGHLIGHT:
	{
		Win *win = va_arg(ap, Win*);
		if (win->file->internal) {
			break;
		}
		if (id == VIX_EVENT_WIN_OPEN) {
			vix_lua_win_open(vix, win);
		} else if (id == VIX_EVENT_WIN_CLOSE) {
			vix_lua_win_close(vix, win);
		} else if (id == VIX_EVENT_WIN_HIGHLIGHT) {
			vix_lua_win_highlight(vix, win);
		}
		break;
	}
	case VIX_EVENT_QUIT:
		vix_lua_quit(vix);
		break;
	case VIX_EVENT_TERM_CSI:
		vix_lua_term_csi(vix, va_arg(ap, const long *));
		break;
	case VIX_EVENT_UI_DRAW:
		vix_lua_ui_draw(vix);
		break;
	}

	va_end(ap);
	return ret;
}

#endif
