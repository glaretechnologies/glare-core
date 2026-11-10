local tag = {}
local function fail() error(tag) end
local ok, err = pcall(fail)
assert(not ok and err == tag)
ok, err = xpcall(fail, function(e) return {cause = e} end)
assert(not ok and err.cause == tag)
