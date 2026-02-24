-- complete word at primary selection location using vix-complete(1)

vix:map(vix.modes.INSERT, "<C-n>", function()
	local win = vix.win
	local file = win.file
	local pos = win.selection.pos
	if not pos then return end

	local range = file:text_object_word(pos > 0 and pos-1 or pos);
	if not range then return end
	if range.finish > pos then range.finish = pos end
	if range.start == range.finish then return end
	local prefix = file:content(range)
	if not prefix then return end

	vix:feedkeys("<vix-selections-save><Escape><Escape>")
	-- collect words starting with prefix
	vix:command("x/\\b" .. prefix .. "\\w+/")
	local candidates = {}
	for sel in win:selections_iterator() do
		table.insert(candidates, file:content(sel.range))
	end
	vix:feedkeys("<Escape><Escape><vix-selections-restore>")
	if #candidates == 1 and candidates[1] == "\n" then return end
	candidates = table.concat(candidates, "\n")

	local status, out, err = vix:pipe(candidates, "sort -u | vix-menu")
	if status ~= 0 or not out then
		if err then vix:info(err) end
		return
	end
	out = out:sub(#prefix + 1, #out - 1)
	file:insert(pos, out)
	win.selection.pos = pos + #out
	-- restore mode to what it was on entry
	vix.mode = vix.modes.INSERT
end, "Complete word in file")
