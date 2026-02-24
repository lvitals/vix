io.stderr:write("DEBUG: vixrc.lua starting\n")
package.path = '../../lua/?.lua;'..package.path
io.stderr:write("DEBUG: vixrc.lua loading...\n")
require('vix')

local function run_if_exists(luafile)
	local f = io.open(luafile, "r")
	if f ~= nil then
		f:close()
		dofile(luafile)
	end
end

vix.events.subscribe(vix.events.WIN_OPEN, function(win)
	-- test.in file passed to vix
	local name = win.file.name
	if name then
		-- use the corresponding test.lua file
		name = string.gsub(name, '%.in$', '')
		run_if_exists(string.format("%s.lua", name))
		local keys_file = string.format("%s.keys", name)
		local file = io.open(keys_file, "r")
		if file then
			local keys = file:read('*all')
			file:close()
			if keys then
				keys = string.gsub(keys, '%s*\n', '')
				keys = string.gsub(keys, '<Space>', ' ')
				vix:feedkeys(keys..'<Escape>')
			end
		end
		vix:command(string.format("w! '%s.out'", name))
		io.stderr:write(string.format("DEBUG: vixrc.lua: WIN_OPEN for '%s' finished\n", name))
	end
end)
