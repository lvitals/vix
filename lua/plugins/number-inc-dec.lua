-- increment/decrement number in dec/hex/oct format
local lexer = vix.lexers
local lpeg = vix.lpeg
if not lexer.load or not lpeg then return end

local Cp = lpeg.Cp()
local dec_num = lpeg.S('+-')^-1 * lexer.dec_num
local pattern = lpeg.P{ Cp * (lexer.hex_num + lexer.oct_num + dec_num) * Cp + 1 * lpeg.V(1) }

local change = function(delta)

	local win = vix.win
	local file = win.file
	local count = vix.count
	if not count then count = 1 end
	vix.count = nil -- reset count, otherwise it affects next motion

	for selection in win:selections_iterator() do
		local pos = selection.pos
		if pos then
			local range = file:text_object_line(pos)
			if range then
				local data = file:content(range.start, range.finish - range.start)
				if data then
					local relative_pos = pos - range.start + 1
					local best_s, best_e
					local current_pos = 1
					while current_pos <= #data do
						local s, e = pattern:match(data, current_pos)
						if not s then break end
						if e - 1 >= relative_pos then
							best_s, best_e = s, e
							break
						end
						current_pos = e
					end

					if best_s then
						local s, e = best_s, best_e
						data = string.sub(data, s, e-1)
						if #data > 0 then
							-- align start and end for fileindex
							s = range.start + s - 1
							e = range.start + e - 1
							local base, format, padding = 10, 'd', 0
							if lexer.oct_num:match(data) then
								base = 8
								format = 'o'
								padding = #data
							elseif lexer.hex_num:match(data) then
								base = 16
								format = 'x'
								padding = #data - #"0x"
							end
							local number = tonumber(data, base == 8 and 8 or nil)
							if number then
								number = number + delta * count
								-- string.format does not support negative hex/oct values
								if base ~= 10 and number < 0 then number = 0 end
								number = string.format((base == 16 and "0x" or "") .. "%0"..padding..format, number)
								if base == 8 and string.sub(number, 0, 1) ~= "0" then
									number = '0' .. number
								end
								file:delete(s, e - s)
								file:insert(s, number)
								selection.pos = s
							end
						end
					end
				end
			end
		end
	end
end

vix:map(vix.modes.NORMAL, "<C-a>", function() change( 1) end, "Increment number")
vix:map(vix.modes.NORMAL, "<C-x>", function() change(-1) end, "Decrement number")
