local function values(...) return select('#', ...), ... end
local head, count, x, hole, y = 8, values(4, nil, 6)
local t = table.pack(head, count, x, hole, y)
local n, first = values(table.unpack(t, 1, t.n))
assert(n == 5 and first == 8 and t[5] == 6)
