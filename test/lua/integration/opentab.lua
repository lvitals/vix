describe("opentab and space handling", function()
    it("should enable tabview when opentab is set to on", function()
        vix:command("set opentab on")
        assert.is_true(vix.ui.tabview)
        vix:command("set opentab off")
    end)

    it("should open file in new tab with :o when opentab is on", function()
        vix:command("set opentab on")
        local initial_win_count = 0
        for _ in vix.windows do initial_win_count = initial_win_count + 1 end
        
        vix:command("open test-file-1.txt")
        
        local new_win_count = 0
        for _ in vix.windows do new_win_count = new_win_count + 1 end
        assert.are.equal(initial_win_count + 1, new_win_count)
        vix:command("set opentab off")
    end)

    it("should replace current file with :e even when opentab is on", function()
        vix:command("set opentab on")
        vix:command("open test-file-2.txt")
        local win_before = vix.win
        
        vix:command("edit test-file-3.txt")
        
        -- In our implementation, :e closes old win and swaps with new one
        -- The focused window object should change, but the total count 
        -- should stay the same as after the last :o
        assert.are_not.equal(win_before, vix.win)
        vix:command("set opentab off")
    end)

    it("should open files with spaces in name using :e and :o", function()
        -- Create a dummy file with space
        local filename = "file with space.txt"
        local f = io.open(filename, "w")
        if f then
            f:write("test")
            f:close()
        end

        vix:command("edit " .. filename)
        assert.are.equal(vix.win.file.name, filename or "")
        
        vix:command("open " .. filename)
        assert.are.equal(vix.win.file.name, filename or "")
        
        os.remove(filename)
    end)
end)
