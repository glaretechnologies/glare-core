local fields = {}
local obj = setmetatable({}, {
    __index = fields,
    __newindex = function(_, k, v) rawset(fields, k, v) end,
    __call = function(self, x) return self.value + x end,
    __concat = function(self, suffix) return tostring(self.value) .. suffix end,
})
obj.value = 4
assert(obj(3) == 7 and obj .. '!' == '4!')
