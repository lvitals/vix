local complete_filename = function(expand)
	local win = vix.win
	local file = win.file
	local sel = win.selection
	local pos = sel.pos
	if not pos then return end

	local range = file:text_object_longword(pos > 0 and pos-1 or pos);
	if not range then return end
	if range.finish > pos then range.finish = pos end

	local line_num = sel.line
	local full_line = file.lines[line_num]

	local prefix = file:content(range)
	if not prefix then return end

	-- Strip leading delimiters for some programming languages
	local _, j = prefix:find(".*[{[(<'\"]+")
	if not expand and j then prefix = prefix:sub(j + 1) end

	if prefix:match("^%s*$") then
		prefix = ""
		range.start = pos
		range.finish = pos
	end

	-- Expand tilda for the home directory
	_, j = prefix:find('^~')
	if j ~= nil then
		local home = assert(os.getenv("HOME"), "$HOME variable not set!")
		prefix = home .. prefix:sub(j + 1)
	end

	local status, out, err
	local suffix = ""
	local match_prefix = nil

	if full_line:match("^[:/?]") then
		local line_before_cursor = full_line:sub(1, sel.col - 1)
		
		-- Check if we are completing the command itself or its arguments
		local cmd_name, args = line_before_cursor:match("^:([^%s]*)%s*(.*)$")

		if args and args ~= "" or line_before_cursor:match("^:[^%s]+%s+$") then
			-- We have a command and at least one space, so we complete arguments
			if line_before_cursor:match("^:set%s+theme%s+") then
				match_prefix = line_before_cursor:match("^:set%s+theme%s+(.*)$") or ""
				local paths = package.path
				local themes = {}
				for path in paths:gmatch("[^;]+") do
					local dir = path:gsub("%.lua$", ""):gsub("%?", "themes")
					local pipe_cmd = string.format("ls -1 %s 2>/dev/null | grep '^%s' | sed 's/\\.lua$//'", dir, match_prefix)
					local s, o, e = vix:pipe(pipe_cmd)
					if s == 0 and o then
						for theme in o:gmatch("[^\n]+") do
							themes[theme] = true
						end
					end
				end
				local theme_list = ""
				for t in pairs(themes) do theme_list = theme_list .. t .. "\n" end
				status, out, err = vix:pipe(theme_list, "vix-menu -b")
			elseif line_before_cursor:match("^:set%s+syntax%s+") then
				match_prefix = line_before_cursor:match("^:set%s+syntax%s+(.*)$") or ""
				local paths = package.path
				local syntaxes = {}
				for path in paths:gmatch("[^;]+") do
					local dir = path:gsub("%.lua$", ""):gsub("%?", "lexers")
					local pipe_cmd = string.format("ls -1 %s 2>/dev/null | grep '^%s' | sed 's/\\.lua$//'", dir, match_prefix)
					local s, o, e = vix:pipe(pipe_cmd)
					if s == 0 and o then
						for syntax in o:gmatch("[^\n]+") do
							syntaxes[syntax] = true
						end
					end
				end
				local syntax_list = ""
				for s in pairs(syntaxes) do syntax_list = syntax_list .. s .. "\n" end
				status, out, err = vix:pipe(syntax_list, "vix-menu -b")
			elseif line_before_cursor:match("^:set%s+layout%s+") then
				match_prefix = line_before_cursor:match("^:set%s+layout%s+(.*)$") or ""
				status, out, err = vix:pipe("h\nv\n", "vix-menu -b")
			elseif line_before_cursor:match("^:set%s+savemethod%s+") then
				match_prefix = line_before_cursor:match("^:set%s+savemethod%s+(.*)$") or ""
				status, out, err = vix:pipe("auto\natomic\ninplace\n", "vix-menu -b")
			elseif line_before_cursor:match("^:set%s+loadmethod%s+") then
				match_prefix = line_before_cursor:match("^:set%s+loadmethod%s+(.*)$") or ""
				status, out, err = vix:pipe("auto\nread\nmmap\n", "vix-menu -b")
			elseif line_before_cursor:match("^:set%s+([^%s!]+)%s+(.*)$") then
				local opt_name, val_prefix = line_before_cursor:match("^:set%s+([^%s!]+)%s+(.*)$")
				match_prefix = val_prefix
				local info = vix:option_type(opt_name)
				if info and info.type == "bool" then
					status, out, err = vix:pipe("on\noff\n", "vix-menu -b")
				else
					-- Default argument completion: Files
					local cmdfmt = "vix-complete --file '%s'"
					if expand then cmdfmt = "vix-open -- '%s'*" end
					status, out, err = vix:pipe(cmdfmt:format(prefix:gsub("'", "'\\''")))
				end
			elseif line_before_cursor:match("^:set%s+") then
				match_prefix = line_before_cursor:match("^:set%s+(.*)$") or ""
				status, out, err = vix:complete_option(match_prefix)
				suffix = " "
			else
				-- Default argument completion: Files
				local cmdfmt = "vix-complete --file '%s'"
				if expand then cmdfmt = "vix-open -- '%s'*" end
				status, out, err = vix:pipe(cmdfmt:format(prefix:gsub("'", "'\\''")))
			end
		elseif cmd_name then
			-- No space yet, complete the command name
			match_prefix = cmd_name
			status, out, err = vix:complete_command(match_prefix)
			suffix = " "
		end

		if out then
			out = out:gsub("\n$", "")
			local to_insert = out
			if match_prefix then
				to_insert = out:sub(#match_prefix + 1)
			end
			to_insert = to_insert .. suffix
			if to_insert ~= "" then
				file:insert(pos, to_insert)
				win.selection.pos = pos + #to_insert
				vix:redraw()
			end
			return
		end
	else
		-- Not a prompt line, standard file completion
		local cmdfmt = "vix-complete --file '%s'"
		if expand then cmdfmt = "vix-open -- '%s'*" end
		status, out, err = vix:pipe(cmdfmt:format(prefix:gsub("'", "'\\''")))
	end

	if status ~= 0 or not out then
		if err then vix:info(err) end
		return
	end
	out = out:gsub("\n$", "")

	if expand then
		file:delete(range)
		pos = range.start
	end

	file:insert(pos, out)
	win.selection.pos = pos + #out
	vix:redraw()
end

-- complete file path at primary selection location using vix-complete(1)
vix:map(vix.modes.INSERT, "<C-x><C-f>", function()
	complete_filename(false);
end, "Complete file name")

-- complete file path at primary selection location using vix-open(1)
vix:map(vix.modes.INSERT, "<C-x><C-o>", function()
	complete_filename(true);
end, "Complete file name (expands path) or command")
