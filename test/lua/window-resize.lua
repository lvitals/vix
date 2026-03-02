describe("Window Mode and Resizing", function()
    it("should enter WINDOW mode with <C-w> and exit with Esc", function()
        assert.are.equal(vix.mode, vix.modes.NORMAL)
        vix:feedkeys("<C-w>")
        assert.are.equal(vix.mode, vix.modes.WINDOW)
        vix:feedkeys("<Escape>")
        assert.are.equal(vix.mode, vix.modes.NORMAL)
    end)

    it("should enter WINDOW mode and exit with q", function()
        vix:feedkeys("<C-w>")
        assert.are.equal(vix.mode, vix.modes.WINDOW)
        vix:feedkeys("q")
        assert.are.equal(vix.mode, vix.modes.NORMAL)
    end)

    it("should inherit weight during split in WINDOW mode", function()
        vix.win.weight = 150
        vix:feedkeys("<C-w>")
        assert.are.equal(vix.mode, vix.modes.WINDOW)
        
        -- S is split horizontal in WINDOW mode
        vix:feedkeys("S")
        -- Should stay in WINDOW mode after split
        assert.are.equal(vix.mode, vix.modes.WINDOW)
        assert.are.equal(vix.win.weight, 150)
        
        vix:feedkeys(":q<Enter>") -- Back to normal and close
        -- vix:feedkeys switches to NORMAL for : command unless mapped
        -- but here it's fine for cleanup
    end)

    it("should resize windows using + and - in WINDOW mode", function()
        -- Create a split first
        vix:feedkeys("<C-w>S")
        vix:feedkeys("<C-w>") -- Re-enter WINDOW mode
        
        local initial_weight = vix.win.weight
        vix:feedkeys("+") -- This should increase weight
        assert.truthy(vix.win.weight > initial_weight)
        
        local current_weight = vix.win.weight
        vix:feedkeys("-") -- This should decrease weight
        assert.truthy(vix.win.weight < current_weight)
        
        vix:feedkeys("=") -- Reset
        assert.are.equal(vix.win.weight, 100)
        
        vix:feedkeys("q") -- Exit
        vix:feedkeys(":q<Enter>")
    end)

    it("should switch global layout in WINDOW mode using s and v", function()
        vix:feedkeys("<C-w>")
        
        vix:feedkeys("v") -- Vertical
        assert.are.equal(vix.layout, vix.layouts.VERTICAL)
        
        vix:feedkeys("s") -- Horizontal
        assert.are.equal(vix.layout, vix.layouts.HORIZONTAL)
        
        vix:feedkeys("q")
    end)

    it("should navigate windows in WINDOW mode using h,j,k,l", function()
        vix:feedkeys("<C-w>S") -- Two windows
        local win1 = vix.win
        
        vix:feedkeys("<C-w>") -- Mode WINDOW
        vix:feedkeys("j") -- Next window
        local win2 = vix.win
        assert.truthy(win1 ~= win2)
        
        vix:feedkeys("k") -- Previous window
        assert.are.equal(vix.win, win1)
        
        vix:feedkeys("q")
        vix:feedkeys(":qall!<Enter>")
    end)

    it("should minimize other windows to 1 line in horizontal layout when pushed", function()
        vix:feedkeys("<C-w>S")
        vix:feedkeys("<C-w>S") -- 3 windows total
        
        -- Select the first window
        vix:feedkeys("<C-w>k")
        vix:feedkeys("<C-w>k")
        
        -- Enter WINDOW mode and push '+' many times to expand J1
        vix:feedkeys("<C-w>")
        for i=1, 20 do
            vix:feedkeys("+")
        end
        
        -- Now check the other windows. At least one should have height 1
        local heights = {}
        for win in vix:windows() do
            table.insert(heights, win.height)
        end
        
        -- One should be maximized, others should be 1
        local ones = 0
        for _, h in ipairs(heights) do
            if h == 1 then ones = ones + 1 end
        end
        
        assert.truthy(ones >= 1)
        vix:feedkeys("q")
        vix:feedkeys(":qall!<Enter>")
    end)
end)
