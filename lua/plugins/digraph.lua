-- insert digraphs using vix-digraph(1)

vix:map(vix.modes.INSERT, "<C-k>", function(keys)
	if #keys < 2 then
		return -1 -- need more input
	end
	local file = io.popen(string.format("vix-digraph '%s' 2>&1", keys:gsub("'", "'\\''")))
	local output = file:read('*all')
	local success, msg, status = file:close()
	if success then
		if vix.mode == vix.modes.INSERT then
			vix:insert(output)
		elseif vix.mode == vix.modes.REPLACE then
			vix:replace(output)
		end
	elseif msg == 'exit' then
		if status == 2 then
			return -1 -- prefix need more input
		end
		vix:info(output)
	end
	return #keys
end, "Insert digraph")
