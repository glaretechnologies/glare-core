local s = '(ab) x=12 y=34'
assert(s:match('%b()') == '(ab)')
local result, count = s:gsub('(%a)=(%d+)', function(k, v) return v .. k end)
assert(count == 2 and result:find('12x', 1, true))
for n in result:gmatch('%d+') do assert(tonumber(n)) end
assert(not pcall(string.match, s, '['))
