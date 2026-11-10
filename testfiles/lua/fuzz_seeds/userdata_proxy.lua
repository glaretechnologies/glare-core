local p = newproxy(true)
local mt = getmetatable(p)
mt.__index = {value = 5}
mt.__tostring = function() return 'proxy' end
assert(p.value == 5 and tostring(p) == 'proxy')
assert(type(p) == 'userdata')
