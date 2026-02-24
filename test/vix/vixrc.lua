package.path = '../../lua/?.lua;'..package.path
dofile("../../lua/vix.lua")

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
		local file = io.open(string.format("%s.keys", name), "r")
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
	end
end)
