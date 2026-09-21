-- Alef LPeg lexer (Plan 9 Alef, as implemented by the alefc toolchain).

local lexer = lexer
local P, S, B = lpeg.P, lpeg.S, lpeg.B

local lex = lexer.new(...)

-- Keywords.
lex:add_rule('keyword', lex:tag(lexer.KEYWORD, lex:word_match(lexer.KEYWORD)))

-- Types.
lex:add_rule('type', lex:tag(lexer.TYPE, lex:word_match(lexer.TYPE)))

-- Functions.
local builtin_func = -(B('.') + B('->')) *
	lex:tag(lexer.FUNCTION_BUILTIN, lex:word_match(lexer.FUNCTION_BUILTIN))
local func = lex:tag(lexer.FUNCTION, lexer.word)
local method = (B('.') + B('->')) * lex:tag(lexer.FUNCTION_METHOD, lexer.word)
lex:add_rule('function', (builtin_func + method + func) * #(lexer.space^0 * '('))

-- Constants.
lex:add_rule('constants', lex:tag(lexer.CONSTANT_BUILTIN,
	-(B('.') + B('->')) * lex:word_match(lexer.CONSTANT_BUILTIN)))

-- Labels.
lex:add_rule('label', lex:tag(lexer.LABEL, lexer.starts_line(lexer.word * ':' * -P(':'))))

-- Strings.
local sq_str = lexer.range("'", true)
local dq_str = lexer.range('"', true)
lex:add_rule('string', lex:tag(lexer.STRING, sq_str + dq_str))

-- Identifiers.
lex:add_rule('identifier', lex:tag(lexer.IDENTIFIER, lexer.word))

-- Comments.
local line_comment = lexer.to_eol('//', true)
local block_comment = lexer.range('/*', '*/')
lex:add_rule('comment', lex:tag(lexer.COMMENT, line_comment + block_comment))

-- Numbers.
local integer = lexer.integer * lexer.word_match('u l ul lu', true)^-1
lex:add_rule('number', lex:tag(lexer.NUMBER, lexer.float + integer))

-- Preprocessor.
local ws = S(' \t')^0
local include = lex:tag(lexer.PREPROCESSOR, '#' * ws * 'include') *
	(lex:get_rule('whitespace') * lex:tag(lexer.STRING, lexer.range('<', '>', true)))^-1
local preproc = lex:tag(lexer.PREPROCESSOR, '#' * ws * lex:word_match(lexer.PREPROCESSOR))
lex:add_rule('preprocessor', include + preproc)

-- Operators: channel send/receive (<-, ?), tuple/scope (::, :=) plus the C set.
lex:add_rule('operator', lex:tag(lexer.OPERATOR, P('<-') + '::' + ':=' + S('+-/*%<>~!=^&|?:;,.()[]{}$')))

-- Fold points.
lex:add_fold_point(lexer.PREPROCESSOR, '#if', '#endif')
lex:add_fold_point(lexer.PREPROCESSOR, '#ifdef', '#endif')
lex:add_fold_point(lexer.PREPROCESSOR, '#ifndef', '#endif')
lex:add_fold_point(lexer.OPERATOR, '{', '}')
lex:add_fold_point(lexer.COMMENT, '/*', '*/')

-- Word lists.
lex:set_word_list(lexer.KEYWORD, {
	'alloc', 'alt', 'become', 'break', 'case', 'check', 'continue', 'default', 'do', 'else',
	'extern', 'for', 'goto', 'if', 'intern', 'par', 'private', 'proc', 'raise', 'rescue', 'return',
	'sizeof', 'static', 'switch', 'task', 'typedef', 'unalloc', 'while', 'zerox', --
	'register', 'const', 'volatile', 'inline' -- accepted C-isms
})

lex:set_word_list(lexer.TYPE, {
	'adt', 'aggr', 'byte', 'chan', 'char', 'double', 'enum', 'float', 'int', 'lint', 'long', 'sint',
	'short', 'signed', 'tuple', 'uint', 'ulint', 'union', 'unsigned', 'usint', 'void', 'poly', 'Lock',
	'QLock', 'Rendez', 'Rune', 'Biobuf', 'Fmt'
})

lex:set_word_list(lexer.FUNCTION_BUILTIN, {
	'print', 'fprint', 'sprint', 'exits', 'terminate', 'malloc', 'free', 'sleep', 'abort', 'exit',
	'alloc', 'unalloc'
})

lex:set_word_list(lexer.CONSTANT_BUILTIN, {'nil', 'NULL', 'stdin', 'stdout', 'stderr'})

lex:set_word_list(lexer.PREPROCESSOR, {
	'define', 'defined', 'elif', 'else', 'endif', 'error', 'if', 'ifdef', 'ifndef', 'line', 'pragma',
	'undef', 'include', 'lib'
})

lexer.property['scintillua.comment'] = '//'

return lex
