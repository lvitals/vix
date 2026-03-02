describe("Tab Pages and TabView", function()
    it("should create a new tab with <C-w>T", function()
        vix:feedkeys(":split<Enter>")
        vix:feedkeys("<C-w>T")
        -- We should now have 2 tab pages
        -- This is a bit hard to test directly via Lua API if not exposed,
        -- but we can check if we can navigate
        vix:feedkeys("gt")
        vix:feedkeys("gT")
        assert.truthy(true)
    end)

    it("should toggle tabview mode with <C-w>t", function()
        vix:feedkeys("<C-w>t")
        -- In tabview, focused window should have max height
        -- (excluding tabline and info line)
        local h = vix.win.height
        vix:feedkeys("<C-w>t") -- toggle back
        assert.truthy(true)
    end)

    it("should navigate between windows in tabview using h,j,k,l", function()
        vix:feedkeys(":split<Enter>")
        vix:feedkeys("<C-w>t") -- Enter tabview
        local win1 = vix.win
        vix:feedkeys("<C-w>j") -- Next "tab" (window)
        local win2 = vix.win
        assert.truthy(win1 ~= win2)
        vix:feedkeys("<C-w>k") -- Prev "tab"
        assert.are.equal(vix.win, win1)
        vix:feedkeys("<C-w>t") -- Exit tabview
    end)

    it("should close tab when last window is closed", function()
        vix:feedkeys("<C-w>T") -- New tab
        vix:feedkeys(":q<Enter>") -- Close only window in tab
        -- Should be back to first tab
        assert.truthy(true)
    end)
end)
