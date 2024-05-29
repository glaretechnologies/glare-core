a = 1   -- create a global variable
    -- change current environment
    setfenv(1, {_G = _G})
    _G.print(a)      --> nil
    _G.print(_G.a)   --> 1
	