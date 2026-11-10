local function read() return value end
setfenv(read, {value = 17})
assert(read() == 17 and getfenv(read).value == 17)
assert(not pcall(rawset, math, 'pi', 0))
