-- std = "min"
globals = { "vix" }
include_files = { "lua/*.lua", "lua/**/*.lua", "test/lua/*.lua" }
exclude_files = { "lua/lexer.lua", "lua/lexers/**", "test/lua/vixrc.lua" }
files["test/lua"] = { std = "+busted" }
