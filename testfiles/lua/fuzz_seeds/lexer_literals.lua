--[==[ a long comment with ]=] inside ]==]
local s = [=[a]]b]=]
local n = 0b1010_0101 + 0x1_f
assert(`{s}:{n}` == "a]]b:196")
