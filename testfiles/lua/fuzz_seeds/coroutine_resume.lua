local co = coroutine.create(function()
    local v = coroutine.yield(Vec2(1, 2))
    return v * 2
end)
local ok, v = coroutine.resume(co)
assert(ok and v.x == 1)
local done, result = coroutine.resume(co, v)
assert(done and result.y == 4)
