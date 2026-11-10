local outer = coroutine.wrap(function()
    local inner = coroutine.wrap(function() return Vec3d(1, 2, 3) end)
    return inner() * 2
end)
assert(outer().z == 6)
