#include "vix-core.h"

static Buffer *register_buffer(Vix *vix, Register *reg, VixDACount slot)
{
	if (slot >= reg->capacity) {
		da_reserve(vix, reg, slot);
	}
	if (slot >= reg->count) {
		reg->count = slot + 1;
	}
	return reg->data + slot;
}

const char *register_slot_get(Vix *vix, Register *reg, size_t slot, size_t *len)
{
	if (len) {
		*len = 0;
	}
	const char *result = 0;
	switch (reg->type) {
	case REGISTER_NORMAL:{
		if ((int)slot < reg->count) {
			Buffer *b = reg->data + slot;
			buffer_terminate(b);
			if (len) {
				*len = buffer_length0(b);
			}
			result = buffer_content0(b);
		}
	}break;
	case REGISTER_NUMBER:{
		if (reg->count > 0) {
			Buffer *b = reg->data;
			b->len = 0;
			buffer_appendf(b, "%zu", slot + 1);
			if (len) {
				*len = buffer_length0(b);
			}
			result = buffer_content0(b);
		}
	}break;
	case REGISTER_CLIPBOARD:{
		if ((VixDACount)slot < reg->count) {
			Buffer *b      = reg->data + slot;
			Buffer  buferr = {0};
			enum VixRegister id = reg - vix->registers;
			const char *cmd[] = {VIX_CLIPBOARD, "--paste", "--selection", 0, 0};
			if (id == VIX_REG_PRIMARY) {
				cmd[3] = "primary";
			} else {
				cmd[3] = "clipboard";
			}

			b->len = 0;
			int status = vix_pipe(vix, vix->win->file, &(Filerange){0}, cmd, b, read_into_buffer,
			                      &buferr, read_into_buffer, false);

			if (status != 0) {
				vix_info_show(vix, "Command failed %s", buffer_content0(&buferr));
			}
			buffer_release(&buferr);
			if (len) {
				*len = buffer_length0(b);
			}
			result = buffer_content0(b);
		}
	}break;
	case REGISTER_BLACKHOLE:
	default:
		break;
	}
	return result;
}

const char *register_get(Vix *vix, Register *reg, size_t *len)
{
	return register_slot_get(vix, reg, 0, len);
}

bool register_slot_put(Vix *vix, Register *reg, size_t slot, const char *data, size_t len)
{
	if (reg->type != REGISTER_NORMAL) {
		return false;
	}
	Buffer *buf = register_buffer(vix, reg, slot);
	return buffer_put(buf, data, len);
}

bool register_put(Vix *vix, Register *reg, const char *data, size_t len) {
	return register_slot_put(vix, reg, 0, data, len) &&
	       register_resize(reg, 1);
}

bool register_put0(Vix *vix, Register *reg, const char *data)
{
	return register_put(vix, reg, data, strlen(data)+1);
}

static bool register_slot_append_range(Vix *vix, Register *reg, size_t slot, Text *txt, Filerange *range)
{
	switch (reg->type) {
	case REGISTER_NORMAL:
	{
		Buffer *buf = register_buffer(vix, reg, slot);
		size_t len = text_range_size(range);
		if (len == SIZE_MAX || !buffer_grow(buf, len+1)) {
			return false;
		}
		if (buf->len > 0 && buf->data[buf->len-1] == '\0') {
			buf->len--;
		}
		buf->len += text_bytes_get(txt, range->start, len, buf->data + buf->len);
		return buffer_append(buf, "\0", 1);
	}
	default:
		return false;
	}
}

bool register_slot_put_range(Vix *vix, Register *reg, size_t slot, Text *txt, Filerange *range)
{
	if (reg->append) {
		return register_slot_append_range(vix, reg, slot, txt, range);
	}

	switch (reg->type) {
	case REGISTER_NORMAL:
	{
		Buffer *buf = register_buffer(vix, reg, slot);
		size_t len = text_range_size(range);
		if (len == SIZE_MAX || !buffer_reserve(buf, len+1)) {
			return false;
		}
		buf->len = text_bytes_get(txt, range->start, len, buf->data);
		return buffer_append(buf, "\0", 1);
	}
	case REGISTER_CLIPBOARD:
	{
		Buffer buferr = {0};
		const char *cmd[] = { VIX_CLIPBOARD, "--copy", "--selection", NULL, NULL };
		enum VixRegister id = reg - vix->registers;

		if (id == VIX_REG_PRIMARY) {
			cmd[3] = "primary";
		} else {
			cmd[3] = "clipboard";
		}

		int status = vix_pipe(vix, vix->win->file, range,
			cmd, NULL, NULL, &buferr, read_into_buffer, false);

		if (status != 0) {
			vix_info_show(vix, "Command failed %s", buffer_content0(&buferr));
		}
		buffer_release(&buferr);
		return status == 0;
	}
	case REGISTER_BLACKHOLE:
		return true;
	default:
		return false;
	}
}

bool register_put_range(Vix *vix, Register *reg, Text *txt, Filerange *range)
{
	return register_slot_put_range(vix, reg, 0, txt, range) &&
	       register_resize(reg, 1);
}

size_t vix_register_count(Vix *vix, Register *reg)
{
	if (reg->type == REGISTER_NUMBER) {
		return vix->win ? vix->win->view.selection_count : 0;
	}
	return reg->count;
}

bool register_resize(Register *reg, size_t count)
{
	bool result = (VixDACount)count < reg->count;
	if (result) {
		reg->count = count;
	}
	return result;
}

enum VixRegister vix_register_from(Vix *vix, char reg) {

	if (reg == '@') {
		return VIX_MACRO_LAST_RECORDED;
	}

	if ('a' <= reg && reg <= 'z') {
		return VIX_REG_a + reg - 'a';
	}
	if ('A' <= reg && reg <= 'Z') {
		return VIX_REG_A + reg - 'A';
	}

	for (size_t i = 0; i < LENGTH(vix_registers); i++) {
		if (vix_registers[i].name == reg) {
			return i;
		}
	}
	return VIX_REG_INVALID;
}

char vix_register_to(Vix *vix, enum VixRegister reg) {

	if (reg == VIX_MACRO_LAST_RECORDED) {
		return '@';
	}

	if (VIX_REG_a <= reg && reg <= VIX_REG_z) {
		return 'a' + reg - VIX_REG_a;
	}
	if (VIX_REG_A <= reg && reg <= VIX_REG_Z) {
		return 'A' + reg - VIX_REG_A;
	}

	if (reg < LENGTH(vix_registers)) {
		return vix_registers[reg].name;
	}

	return '\0';
}

void vix_register(Vix *vix, enum VixRegister reg) {
	if (VIX_REG_A <= reg && reg <= VIX_REG_Z) {
		vix->action.reg = &vix->registers[VIX_REG_a + reg - VIX_REG_A];
		vix->action.reg->append = true;
	} else if (reg < LENGTH(vix->registers)) {
		vix->action.reg = &vix->registers[reg];
		vix->action.reg->append = false;
	}
}

enum VixRegister vix_register_used(Vix *vix) {
	if (!vix->action.reg) {
		return VIX_REG_DEFAULT;
	}
	return vix->action.reg - vix->registers;
}

static Register *register_from(Vix *vix, enum VixRegister id) {
	if (VIX_REG_A <= id && id <= VIX_REG_Z) {
		id = VIX_REG_a + id - VIX_REG_A;
	}
	if (id < LENGTH(vix->registers)) {
		return &vix->registers[id];
	}
	return NULL;
}

bool vix_register_set(Vix *vix, enum VixRegister id, str8_list strings)
{
	Register *reg = register_from(vix, id);
	if (!reg) {
		return false;
	}
	for (VixDACount i = 0; i < strings.count; i++) {
		Buffer *buf = register_buffer(vix, reg, i);
		str8 string = strings.data[i];
		if (!buffer_put(buf, string.data, string.length)) {
			return false;
		}
	}
	return register_resize(reg, strings.count);
}

str8_list vix_register_get(Vix *vix, enum VixRegister id)
{
	str8_list result = {0};
	Register *reg = register_from(vix, id);
	if (reg) {
		da_reserve(vix, &result, reg->count);
		for (VixDACount i = 0; i < reg->count; i++) {
			*da_push(vix, &result) = (str8){
				.length = reg->data[i].len,
				.data   = (uint8_t *)reg->data[i].data,
			};
		}
	}
	return result;
}

const RegisterDef vix_registers[] = {
	[VIX_REG_DEFAULT]    = { '"', VIX_HELP("Unnamed register")                                 },
	[VIX_REG_ZERO]       = { '0', VIX_HELP("Yank register")                                    },
	[VIX_REG_1]          = { '1', VIX_HELP("1st sub-expression match")                         },
	[VIX_REG_2]          = { '2', VIX_HELP("2nd sub-expression match")                         },
	[VIX_REG_3]          = { '3', VIX_HELP("3rd sub-expression match")                         },
	[VIX_REG_4]          = { '4', VIX_HELP("4th sub-expression match")                         },
	[VIX_REG_5]          = { '5', VIX_HELP("5th sub-expression match")                         },
	[VIX_REG_6]          = { '6', VIX_HELP("6th sub-expression match")                         },
	[VIX_REG_7]          = { '7', VIX_HELP("7th sub-expression match")                         },
	[VIX_REG_8]          = { '8', VIX_HELP("8th sub-expression match")                         },
	[VIX_REG_9]          = { '9', VIX_HELP("9th sub-expression match")                         },
	[VIX_REG_AMPERSAND]  = { '&', VIX_HELP("Last regex match")                                 },
	[VIX_REG_BLACKHOLE]  = { '_', VIX_HELP("/dev/null register")                               },
	[VIX_REG_PRIMARY]    = { '*', VIX_HELP("Primary clipboard register, see vix-clipboard(1)") },
	[VIX_REG_CLIPBOARD]  = { '+', VIX_HELP("System clipboard register, see vix-clipboard(1)")  },
	[VIX_REG_DOT]        = { '.', VIX_HELP("Last inserted text")                               },
	[VIX_REG_SEARCH]     = { '/', VIX_HELP("Last search pattern")                              },
	[VIX_REG_COMMAND]    = { ':', VIX_HELP("Last :-command")                                   },
	[VIX_REG_SHELL]      = { '!', VIX_HELP("Last shell command given to either <, >, |, or !") },
	[VIX_REG_NUMBER]     = { '#', VIX_HELP("Register number")                                  },
};
