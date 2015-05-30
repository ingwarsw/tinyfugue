-- fh = io.open("/home/goblin/LUA", "w")
-- fh:write("otworzone\n")

    ---------------------------------------------
    -- Return indentation string for passed level
    ---------------------------------------------
    local function tabs(i)
        return string.rep(".",i).." "   -- Dots followed by a space
    end

    -----------------------------------------------------------
    -- Return string representation of parameter's value & type
    -----------------------------------------------------------
    local function toStrType(t)
        local function fttu2hex(t) -- Grab hex value from tostring() output
            local str = tostring(t);
            if str == nil then
                return "tostring() failure! \n"
            else
                local str2 = string.match(str,"[ :][ (](%x+)")
                if str2 == nil then
                    return "string.match() failure: "..str.."\n"
                else
                    return "0x"..str2
                end
            end
        end
        -- Stringify a value of a given type using a table of functions keyed
        -- by the name of the type (Lua's version of C's switch() statement).
        local stringify = {
            -- Keys are all possible strings that type() may return,
            -- per http://www.lua.org/manual/5.1/manual.html#pdf-type.
            ["nil"]			= function(v) return "nil (nil)"			    end,
            ["string"]		= function(v) return '"'..v..'" (string)'	    end,
            ["number"]		= function(v) return v.." (number)"			    end,
            ["boolean"]		= function(v) return tostring(v).." (boolean)"  end,
            ["function"]	= function(v) return fttu2hex(v).." (function)" end,
            ["table"]		= function(v) return fttu2hex(v).." (table)"	end,
            ["thread"]		= function(v) return fttu2hex(v).." (thread)"	end,
            ["userdata"]	= function(v) return fttu2hex(v).." (userdata)" end
        }
        return stringify[type(t)](t)
    end

    -------------------------------------
    -- Count elements in the passed table
    -------------------------------------
    local function lenTable(t)		-- What Lua builtin does this simple thing?
        local n=0                   -- '#' doesn't work with mixed key types
        if ("table" == type(t)) then
            for key in pairs(t) do  -- Just count 'em
                n = n + 1
            end
            return n
        else
            return nil
        end
    end

    --------------------------------
    -- Pretty-print the passed table
    --------------------------------
    local function do_dumptable(t, indent, seen)
        -- "seen" is an initially empty table used to track all tables
        -- that have been dumped so far.  No table is dumped twice.
        -- This also keeps the code from following self-referential loops,
        -- the need for which was found when first dumping "_G".
		local ret = ""
        if ("table" == type(t)) then	-- Dump passed table
            seen[t] = 1
            if (indent == 0) then
                ret = ret .. "The passed table has "..lenTable(t).." entries:"
                indent = 1
            end
            for f,v in pairs(t) do
                if ("table" == type(v)) and (seen[v] == nil) then    -- Recurse
                    ret = ret ..  tabs(indent)..toStrType(f).." has "..lenTable(v).." entries: {"
                    do_dumptable(v, indent+1, seen)
                    ret = ret .. tabs(indent).."}" 
                else
                    ret = ret .. tabs(indent)..toStrType(f).." = "..toStrType(v)
                end
            end
        else
            ret = ret .. tabs(indent).."Not a table!"
        end

		return ret
    end

    --------------------------------
    -- Wrapper to handle persistence
    --------------------------------
    function dumptable(t)   -- Only global declaration in the package
        -- This wrapper exists only to set the environment for the first run:
        -- The second param is the indentation level.
        -- The third param is the list of tables dumped during this call.
        -- Getting this list allocated and freed was a pain, and this
        -- wrapper was the best solution I came up with...
        return do_dumptable(t, 0, {})
    end



function luadupa(str, attrs, line_attr)
	-- fh:write("dodaje: str: " .. str .. "j"\n")
	tf_eval("/echo " .. str .. "] " .. dumptable(attrs) .. "> " .. line_attr)
end
