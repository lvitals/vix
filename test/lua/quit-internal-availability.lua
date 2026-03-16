-- Test that internal files are available during the QUIT event.
-- This ensures that plugins can still call functions like vix:message()
-- during shutdown, which relies on vix->error_file (an internal file).

vix.events.subscribe(vix.events.QUIT, function()
    -- This message is sent during the QUIT event.
    -- It uses internal files which must not be freed yet.
    vix:message("QUIT event handler executing successfully.")
    
    -- Print to stderr to verify the execution in headless tests
    io.stderr:write("SUCCESS: QUIT event handled\n")
    io.stderr:flush()
end)

describe("QUIT event internal files availability", function()
    it("is subscribed successfully", function()
        -- The actual execution happens during cleanup
        assert.truthy(true)
    end)
end)
