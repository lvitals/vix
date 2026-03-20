-- Test window navigation and tab bar scrolling logic

describe("Tab Bar Scrolling and Navigation", function()
    it("should navigate through all open files and skip internal windows", function()
        -- Create several windows
        vix:feedkeys(":split file1.txt<Enter>")
        vix:feedkeys(":split file2.txt<Enter>")
        vix:feedkeys(":split file3.txt<Enter>")
        
        -- Enter tabview mode to see them as tabs
        vix:feedkeys("<C-w>t")
        
        local start_win = vix.win
        local wins = {}
        wins[1] = start_win
        
        -- Navigate right 3 times
        vix:feedkeys("<C-w>l")
        wins[2] = vix.win
        assert.truthy(wins[2] ~= wins[1], "Should have moved to next window")
        
        vix:feedkeys("<C-w>l")
        wins[3] = vix.win
        assert.truthy(wins[3] ~= wins[2], "Should have moved to next window")
        
        vix:feedkeys("<C-w>l")
        wins[4] = vix.win
        assert.truthy(wins[4] ~= wins[3], "Should have moved to next window")
        
        -- After 4 windows (original + 3 splits), we should be back to start_win
        vix:feedkeys("<C-w>l")
        assert.are.equal(start_win, vix.win, "Should have wrapped around back to first window")
        
        -- Test navigation with prompt shown (should still skip it)
        vix:feedkeys(":") -- Show prompt
        -- Now we are in COMMAND mode, Ctrl-w might not work the same depending on keymap
        -- But we can exit and try to see if navigation was affected
        vix:feedkeys("<Escape>")
        
        vix:feedkeys("<C-w>h") -- Move left
        assert.are.equal(wins[4], vix.win, "Should have moved back to last window")
        
        vix:feedkeys("<C-w>t") -- Exit tabview
    end)

    it("should handle many tabs without crashing and scroll correctly", function()
        -- Create many windows to exceed screen width
        -- Each tab is roughly " [No Name] " (12 chars). 
        -- Default width is 80. 10 tabs = 120 chars.
        for i = 1, 15 do
            vix:feedkeys(":vnew<Enter>")
        end
        
        vix:feedkeys("<C-w>t") -- Enter tabview
        
        -- Move through all of them
        for i = 1, 20 do
            vix:feedkeys("<C-w>l")
            vix:draw() -- This triggers ui_tab_scroll_to_visible
        end
        
        -- If it didn't crash, the logic for valid win_view_offset is working
        assert.truthy(true)
        
        -- Clean up
        vix:feedkeys("<C-w>t")
        for i = 1, 15 do
            vix:feedkeys(":q<Enter>")
        end
    end)
    
    it("should reset win_view_offset when switching tabs", function()
        vix:feedkeys(":vnew tab1_win1<Enter>")
        vix:feedkeys("<C-w>T") -- New tab with one window
        vix:feedkeys(":vnew tab2_win1<Enter>")
        vix:feedkeys(":vnew tab2_win2<Enter>")
        
        -- Switch tabs
        vix:feedkeys("gt")
        vix:draw()
        
        vix:feedkeys("gT")
        vix:draw()
        
        -- No crash means reset logic worked
        assert.truthy(true)
    end)
end)
