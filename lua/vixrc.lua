-- load standard vix module, providing parts of the Lua API
require('vix')

vix.events.subscribe(vix.events.INIT, function()
	-- Your global configuration options
end)

vix.events.subscribe(vix.events.WIN_OPEN, function(win) -- luacheck: no unused args
	-- Your per window configuration options e.g.
	-- vix:command('set number')
end)
