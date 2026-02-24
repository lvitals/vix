vix.events = {}
vix.events.win_open = function(win)
	-- test.in file passed to vix
	local in_file = win.file.name
	if in_file then
		-- use the corresponding test.cmd file
		local cmd_file_name = string.gsub(in_file, '%.in$', '.cmd');
		local cmd_file = io.open(cmd_file_name)
		local cmd = cmd_file:read('*all')
		vix:command(string.format(",{\n %s\n }", cmd))
		local out_file_name = string.gsub(in_file, '%.in$', '.vix.out')
		vix:command(string.format("w! %s", out_file_name))
	end
end
