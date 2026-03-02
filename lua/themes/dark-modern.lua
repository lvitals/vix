-- VS Code Dark Modern theme
local lexers = vix.lexers

local colors = {
	bg = '#1f1f1f',
	fg = '#cccccc',
	selection = '#264f78',
	line_highlight = '#2b2d2e',
	comment = '#6a9955',
	keyword = '#569cd6',
	control = '#c586c0',
	string = '#ce9178',
	number = '#b5cea8',
	func = '#dcdcaa',
	variable = '#9cdcfe',
	constant = '#4fc1ff',
	type = '#4ec9b0',
	class = '#4ec9b0',
	parameter = '#9cdcfe',
	property = '#9cdcfe',
	namespace = '#4ec9b0',
	operator = '#d4d4d4',
	regex = '#d16969',
	tag = '#569cd6',
	attribute = '#9cdcfe',
	
	status_bg = '#007acc',
	status_fg = '#ffffff',
	status_inactive_bg = '#181818',
	status_inactive_fg = '#cccccc',
	
	line_number = '#858585',
	line_number_active = '#c6c6c6',
	
	error = '#f14c4c',
	warning = '#cca700',
	info = '#3794ff',
}

lexers.colors = colors

lexers.STYLE_DEFAULT = 'fore:'..colors.fg..',back:'..colors.bg
lexers.STYLE_NOTHING = ''
lexers.STYLE_ATTRIBUTE = 'fore:'..colors.attribute
lexers.STYLE_CLASS = 'fore:'..colors.class
lexers.STYLE_COMMENT = 'fore:'..colors.comment
lexers.STYLE_CONSTANT = 'fore:'..colors.constant
lexers.STYLE_DEFINITION = 'fore:'..colors.func
lexers.STYLE_ERROR = 'fore:'..colors.error
lexers.STYLE_FUNCTION = 'fore:'..colors.func
lexers.STYLE_HEADING = 'fore:'..colors.keyword..',bold'
lexers.STYLE_KEYWORD = 'fore:'..colors.keyword
lexers.STYLE_LABEL = 'fore:'..colors.control
lexers.STYLE_NUMBER = 'fore:'..colors.number
lexers.STYLE_OPERATOR = 'fore:'..colors.operator
lexers.STYLE_REGEX = 'fore:'..colors.regex
lexers.STYLE_STRING = 'fore:'..colors.string
lexers.STYLE_PREPROCESSOR = 'fore:'..colors.control
lexers.STYLE_TAG = 'fore:'..colors.tag
lexers.STYLE_TYPE = 'fore:'..colors.type
lexers.STYLE_VARIABLE = 'fore:'..colors.variable
lexers.STYLE_WHITESPACE = ''
lexers.STYLE_EMBEDDED = 'fore:'..colors.control
lexers.STYLE_IDENTIFIER = 'fore:'..colors.fg

lexers.STYLE_LINENUMBER = 'fore:'..colors.line_number..',back:'..colors.bg
lexers.STYLE_LINENUMBER_CURSOR = 'fore:'..colors.line_number_active..',back:'..colors.bg
lexers.STYLE_CURSOR = 'reverse'
lexers.STYLE_CURSOR_PRIMARY = 'reverse'
lexers.STYLE_CURSOR_LINE = 'back:'..colors.line_highlight
lexers.STYLE_COLOR_COLUMN = 'back:'..colors.line_highlight
lexers.STYLE_SELECTION = 'back:'..colors.selection
lexers.STYLE_STATUS = 'fore:'..colors.status_inactive_fg..',back:'..colors.status_inactive_bg
lexers.STYLE_STATUS_FOCUSED = 'fore:'..colors.status_fg..',back:'..colors.status_bg
lexers.STYLE_TAB = 'fore:'..colors.status_inactive_fg..',back:'..colors.status_inactive_bg
lexers.STYLE_TAB_FOCUSED = 'fore:'..colors.status_fg..',back:'..colors.status_bg
lexers.STYLE_SEPARATOR = 'fore:'..colors.line_number..',back:'..colors.bg
lexers.STYLE_INFO = 'fore:'..colors.info
lexers.STYLE_EOF = 'fore:'..colors.line_number

-- Language specific overrides matching VS Code logic

-- Markdown
lexers.STYLE_BOLD = 'bold'
lexers.STYLE_ITALIC = 'italics'
lexers.STYLE_CODE = 'fore:'..colors.string
lexers.STYLE_LINK = 'fore:'..colors.constant..',underlined'
lexers.STYLE_REFERENCE = 'fore:'..colors.constant
lexers.STYLE_LIST = 'fore:'..colors.keyword

-- Diff
lexers.STYLE_ADDITION = 'fore:'..colors.string
lexers.STYLE_DELETION = 'fore:'..colors.error
lexers.STYLE_CHANGE = 'fore:'..colors.warning

-- XML/HTML
lexers.STYLE_TAG_UNKNOWN = 'fore:'..colors.tag
lexers.STYLE_TAG_SINGLE = 'fore:'..colors.tag
lexers.STYLE_TAG_DOCTYPE = 'fore:'..colors.tag..',bold'
lexers.STYLE_ATTRIBUTE_UNKNOWN = 'fore:'..colors.attribute
