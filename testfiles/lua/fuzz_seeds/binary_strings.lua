local fmt = '>i2I4'
local bytes = string.pack(fmt, -5, 0x01020304)
local a, b, pos = string.unpack(fmt, bytes)
assert(a == -5 and b == 0x01020304 and pos == string.packsize(fmt) + 1)
assert(not pcall(string.unpack, fmt, bytes:sub(2)))
