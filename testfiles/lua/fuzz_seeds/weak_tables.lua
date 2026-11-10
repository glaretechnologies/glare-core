local cache = setmetatable({}, {__mode = 'kv'})
local keep = {}
cache[keep] = keep
for i = 1, 64 do
    local key = {}
    cache[key] = {key}
end
assert(cache[keep] == keep and gcinfo() > 0)
