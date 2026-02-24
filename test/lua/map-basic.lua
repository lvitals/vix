
vix:map(vix.modes.NORMAL, "K", function()
	vix:feedkeys("iNormal Mode<Escape>")
end)

vix:map(vix.modes.INSERT, "K", function()
	vix:feedkeys("Insert Mode<Escape>")
end)

vix:map(vix.modes.VISUAL, "K", function()
	vix:feedkeys("<Escape>iVisual Mode<Escape>")
end)

vix:map(vix.modes.VISUAL_LINE, "K", function()
	vix:feedkeys("<Escape>iVisual Line Mode<Escape>")
end)

vix:map(vix.modes.REPLACE, "K", function()
	vix:feedkeys("Replace Mode<Escape>")
end)

local win = vix.win
local file = win.file

describe("map", function()

	before_each(function()
		win.selection.pos = 0
	end)

	after_each(function()
		file:delete(0, file.size)
	end)

	local same = function(expected)
		local data = file:content(0, file.size)
		assert.are.same(expected, data)
	end

	it("normal mode", function()
		vix:feedkeys("K")
		same("Normal Mode")
	end)

	it("insert mode", function()
		vix:feedkeys("iK")
		same("Insert Mode")
	end)

	it("visual mode", function()
		vix:feedkeys("vK")
		same("Visual Mode")
	end)

	it("visual line mode", function()
		vix:feedkeys("VK")
		same("Visual Line Mode")
	end)

	it("replace mode", function()
		vix:feedkeys("RK")
		same("Replace Mode")
	end)
end)
