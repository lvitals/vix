-- Alias for :sh and :shell to open a terminal
vix:command_register("sh", function()
	vix:command("!$SHELL")
	return true
end, "Open a shell")

vix:command_register("shell", function()
	vix:command("!$SHELL")
	return true
end, "Open a shell")
