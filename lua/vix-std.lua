-- standard vix event handlers

vix.events.subscribe(vix.events.INIT, function()
	if os.getenv("TERM_PROGRAM") == "Apple_Terminal" then
		vix:command("set change256colors false")
	end
	vix:command("set theme default")
end)

vix:option_register("theme", "string", function(name)
	if name ~= nil then
		local theme = 'themes/'..name
		package.loaded[theme] = nil
		if not pcall(require, theme) then
			vix:info("Theme not found: " .. name)
			return false
		end
	end

	local lexers = vix.lexers
	lexers.lexers = {}

	if not lexers.load then return false end
	if not lexers.property then lexers.load("text") end
	local colors = lexers.colors
	local default_colors = { "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white" }
	for _, c in ipairs(default_colors) do
		if not colors[c] or colors[c] == '' then
			colors[c] = c
		end
	end

	for win in vix:windows() do
		win:set_syntax(win.syntax)
	end
	return true
end, "Color theme to use, filename without extension")

vix:option_register("syntax", "string", function(name)
	if not vix.win then return false end
	if not vix.win:set_syntax(name) then
		vix:info(string.format("Unknown syntax definition: `%s'", name))
		return false
	end
	return true
end, "Syntax highlighting lexer to use")

vix:option_register("horizon", "number", function(horizon)
	if not vix.win then return false end
	vix.win.horizon = horizon
	return true
end, "Number of bytes to consider for syntax highlighting")

vix.events.subscribe(vix.events.WIN_HIGHLIGHT, function(win)
	if not win.syntax or not vix.lexers.load then return end
	local lexer = vix.lexers.load(win.syntax, nil, true)
	if not lexer then return end

	-- TODO: improve heuristic for initial style
	local viewport = win.viewport.bytes
	if not viewport then return end
	local horizon_max = win.horizon or 32768
	local horizon = viewport.start < horizon_max and viewport.start or horizon_max
	local view_start = viewport.start
	local lex_start = viewport.start - horizon
	viewport.start = lex_start
	local data = win.file:content(viewport)
	local token_styles = lexer._TAGS
	local tokens = lexer:lex(data, 1)
	local token_end = lex_start + (tokens[#tokens] or 1) - 1

	for i = #tokens - 1, 1, -2 do
		local token_start = lex_start + (tokens[i-1] or 1) - 1
		if token_end < view_start then
			break
		end
		local name = tokens[i]
		local style = token_styles[name]
		if style ~= nil then
			win:style(style, token_start, token_end)
		end
		token_end = token_start - 1
	end
end)

local modes = {
	[vix.modes.NORMAL] = '',
	[vix.modes.OPERATOR_PENDING] = '',
	[vix.modes.VISUAL] = 'VISUAL',
	[vix.modes.VISUAL_LINE] = 'VISUAL-LINE',
	[vix.modes.INSERT] = 'INSERT',
	[vix.modes.REPLACE] = 'REPLACE',
	[vix.modes.WINDOW] = 'WINDOW',
}

vix.events.subscribe(vix.events.WIN_STATUS, function(win)
	local left_parts = {}
	local right_parts = {}
	local file = win.file
	local selection = win.selection

	local mode = modes[vix.mode]
	if mode ~= '' and vix.win == win then
		table.insert(left_parts, mode)
	end

	table.insert(left_parts, (file.name or '[No Name]') ..
		(file.modified and ' [+]' or '') .. (vix.recording and ' @' or ''))

	if win.syntax then
		table.insert(left_parts, win.syntax)
	end

	local count = vix.count
	local keys = vix.input_queue
	if keys ~= '' then
		if count then
			table.insert(right_parts, count .. keys)
		else
			table.insert(right_parts, keys)
		end
	elseif count then
		table.insert(right_parts, count)
	end

	if #win.selections > 1 then
		table.insert(right_parts, selection.number..'/'..#win.selections)
	end

	local size = file.size
	local pos = selection.pos
	if not pos then pos = 0 end
	table.insert(right_parts, (size == 0 and "0" or math.ceil(pos/size*100)).."%")

	if not win.large then
		local col = selection.col
		table.insert(right_parts, selection.line..', '..col)
		if size > 33554432 or col > 65536 then
			win.large = true
		end
	end

	local left = ' ' .. table.concat(left_parts, " » ") .. ' '
	local right = ' ' .. table.concat(right_parts, " « ") .. ' '
	win:status(left, right);
end)

vix:command_register("lua", function(argv, force, win, selection, range)
	local code = table.concat(argv, " ")
	local func, err = load(code)
	if func then
		local status, err = pcall(func)
		if not status then vix:info(err) end
	else
		vix:info(err)
	end
	return true
end, "Execute Lua code")

vix:command_register("wrc", function()
	local home = os.getenv("HOME")
	local config_dir = os.getenv("XDG_CONFIG_HOME") or (home .. "/.config")
	local local_rc_path = config_dir .. "/vix/vixrc.lua"
	
	local read_path = package.searchpath('vixrc', package.path)
	
	local lines = {}
	if read_path then
		local f = io.open(read_path, "r")
		if f then
			for line in f:lines() do table.insert(lines, line) end
			f:close()
		end
	end

	local changes = vix:session_changes()
	local changed_count = 0
	
	-- Helper to find or create block and insert/replace
	local function update_block(block_name, opt, cmd, aliases)
		local block_start = nil
		local block_end = nil
		for i, line in ipairs(lines) do
			if line:match("vix.events.subscribe%(vix.events." .. block_name) then
				block_start = i
				for j = i + 1, #lines do
					if lines[j]:match("^end%)") then
						block_end = j
						break
					end
				end
				break
			end
		end

		local found = false
		if block_start and block_end then
			for i = block_start + 1, block_end - 1 do
				local matched = false
				for _, alias in ipairs(aliases or {opt}) do
					if lines[i]:match("set%s+" .. alias .. "[%s']") or lines[i]:match("set%s+" .. alias .. '[%s"]') then
						matched = true
						break
					end
				end
				if matched then
					lines[i] = "\t" .. cmd
					found = true
					break
				end
			end
			if not found then
				table.insert(lines, block_end, "\t" .. cmd)
			end
			return true
		end
		return false
	end

	for opt, _ in pairs(changes) do
		local val = vix:option_value(opt)
		if val then
			local info = vix:option_type(opt)
			local cmd = string.format("vix:command('set %s %s')", opt, val)
			local block = (info and info.need_window) and "WIN_OPEN" or "INIT"
			
			if not update_block(block, opt, cmd, info and info.aliases) then
				-- Fallback: if block not found, just append to end
				table.insert(lines, cmd)
			end
			changed_count = changed_count + 1
		end
	end

	if changed_count == 0 then
		vix:info("No session changes to save.")
		return true
	end

	os.execute("mkdir -p " .. config_dir .. "/vix")
	local f = io.open(local_rc_path, "w")
	if f then
		for _, line in ipairs(lines) do f:write(line .. "\n") end
		f:close()
		vix:info(string.format("Saved %d options to %s", changed_count, local_rc_path))
	else
		vix:info("Failed to write to " .. local_rc_path)
	end
	return true
end, "Save session changes to vixrc.lua")

-- default plugins

require('plugins/filetype')
require('plugins/textobject-lexer')
require('plugins/digraph')
require('plugins/number-inc-dec')
require('plugins/complete-word')
require('plugins/complete-filename')
require('plugins/shell-alias')
