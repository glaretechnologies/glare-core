local values = {3, 4, 5}
local wrapper = setmetatable({}, {__iter = function() return ipairs(values) end})
local total = 0
for _, v in wrapper do total += v end
for _, v in values do total -= v end
assert(total == 0)
