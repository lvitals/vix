describe("Window Resizing and Layout", function()
    it("should have default weight 100", function()
        assert.are.equal(vix.win.weight, 100)
    end)

    it("should inherit weight during split", function()
        vix.win.weight = 150
        vix:feedkeys("<C-w>s")
        -- The new window (vix.win) should inherit weight 150
        assert.are.equal(vix.win.weight, 150)
        vix:feedkeys(":q<Enter>") -- Close the split
        assert.are.equal(vix.win.weight, 150)
    end)

    it("should increase weight with <C-w>+ and <C-w>>", function()
        vix.win.weight = 100
        -- We need at least two windows to resize
        vix:feedkeys("<C-w>s")
        local initial_weight = vix.win.weight
        vix:feedkeys("<C-w>+")
        -- STEP is 20, but in my implementation it's 20 * 3 = 60
        -- push = window_resize_step * 3 = 20 * 3 = 60
        assert.are.equal(vix.win.weight, initial_weight + 60)
        
        vix:feedkeys("<C-w>>")
        assert.are.equal(vix.win.weight, initial_weight + 120)
        vix:feedkeys(":q<Enter>")
    end)

    it("should decrease weight with <C-w>- and <C-w><", function()
        vix.win.weight = 200
        vix:feedkeys("<C-w>s")
        local initial_weight = vix.win.weight
        vix:feedkeys("<C-w>-")
        assert.are.equal(vix.win.weight, initial_weight - 60)
        
        vix:feedkeys("<C-w><")
        assert.are.equal(vix.win.weight, initial_weight - 120)
        vix:feedkeys(":q<Enter>")
    end)

    it("should reset weights with <C-w>=", function()
        vix:feedkeys("<C-w>s")
        vix.win.weight = 500
        vix:feedkeys("<C-w>=")
        assert.are.equal(vix.win.weight, 100)
        
        for win in vix:windows() do
            assert.are.equal(win.weight, 100)
        end
        vix:feedkeys(":q<Enter>")
    end)

    it("should switch layout with <C-w>H, V, S, J, K, L", function()
        vix:feedkeys("<C-w>V")
        assert.are.equal(vix.layout, vix.layouts.VERTICAL)
        
        vix:feedkeys("<C-w>H")
        assert.are.equal(vix.layout, vix.layouts.HORIZONTAL)
        
        vix:feedkeys("<C-w>V")
        assert.are.equal(vix.layout, vix.layouts.VERTICAL)
        
        vix:feedkeys("<C-w>S")
        assert.are.equal(vix.layout, vix.layouts.HORIZONTAL)
        
        vix:feedkeys("<C-w>K")
        assert.are.equal(vix.layout, vix.layouts.HORIZONTAL)
        
        vix:feedkeys("<C-w>L")
        assert.are.equal(vix.layout, vix.layouts.VERTICAL)
    end)
end)
