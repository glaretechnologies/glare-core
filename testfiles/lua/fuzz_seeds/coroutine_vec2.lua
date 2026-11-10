local make_vec = coroutine.wrap(function() return Vec2(3, 4) end)
local v = make_vec()
assert(v.x == 3 and v.y == 4)
