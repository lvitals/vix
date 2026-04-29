describe("View Refactor", function()
    it("should handle line wrapping without crashing", function()
        vix:command("set wrapcolumn=5")
        vix:feedkeys("i123456789<Escape>")
        -- If we are here, it didn't crash during wrap_line
    end)

    it("should handle redrawing without crashing", function()
        vix:feedkeys(":redraw<Enter>")
        -- Triggers view_draw -> view_blank_cell
    end)

    it("should handle window resizing without crashing", function()
        vix:feedkeys("<C-w>v") -- split vertical
        vix:feedkeys("<C-w>h") -- move left
        vix:feedkeys("10<C-w>>") -- resize
        -- Triggers view_resize -> view_draw -> view_blank_cell
    end)
end)
