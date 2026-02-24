local win = vix.win

describe("lexer integration", function()

	it("can load C lexer", function()
		assert.truthy(vix.lexers.load)
		local lex, err = vix.lexers.load('c', nil, true)
		if not lex then
			error("Failed to load C lexer: " .. tostring(err))
		end
		assert.truthy(lex)
		assert.are.equal('c', lex._name)
	end)

	it("can lex C code", function()
		local lex, err = vix.lexers.load('c', nil, true)
		assert.truthy(lex, "Lexer should be loaded")
		local code = "int main() { return 0; }"
		local tokens = lex:lex(code)
		assert.truthy(#tokens > 0)
		assert.are.equal('keyword', tokens[1])
	end)

	it("stress lexer with repeated calls", function()
		local lexer = vix.lexers.load('c', nil, true)
		local code = "void func() { int x = 42; if (x > 0) return; }"
		for i=1, 100 do
			local tokens = lexer:lex(code)
			assert.truthy(#tokens > 0)
		end
	end)

	it("can load other lexers", function()
		local languages = {'lua', 'bash', 'makefile', 'python', 'rust'}
		for _, lang in ipairs(languages) do
			local lexer = vix.lexers.load(lang, nil, true)
			assert.truthy(lexer)
			assert.are.equal(lang, lexer._name)
		end
	end)

end)
