local function scale(x) return Vec3f(x, 2, 3) * 2 end
local v = scale(4) - Vec3f(1, 1, 1)
assert(v.x == 7 and v.y == 3 and v.z == 5)
