-- Test terminal cursor position tracking

describe("Terminal cursor position", function()
    it("should be (0,0) initially for a new window", function()
        vix:draw()
        -- Note: In headless mode, sidebar_width is 0 by default.
        -- cur_row should be 1 (tab bar is at row 0)
        assert.are.equal(1, vix.ui.cur_row)
        assert.are.equal(0, vix.ui.cur_col)
    end)

    it("should update when the cursor moves", function()
        vix:feedkeys("j") -- move to line 2
        vix:draw()
        assert.are.equal(2, vix.ui.cur_row)
        assert.are.equal(0, vix.ui.cur_col)
        
        vix:feedkeys("ll") -- move 2 chars right
        vix:draw()
        assert.are.equal(2, vix.ui.cur_row)
        assert.are.equal(2, vix.ui.cur_col)
    end)

    it("should move to the bottom line when the prompt is shown", function()
        vix:feedkeys(":") -- show prompt
        vix:draw()
        -- The prompt is at the bottom line (vix.ui.height - 1)
        assert.are.equal(vix.ui.height - 1, vix.ui.cur_row)
        -- The cursor in prompt is after the ':' character
        assert.are.equal(1, vix.ui.cur_col)
        vix:feedkeys("<Escape>")
    end)
end)
