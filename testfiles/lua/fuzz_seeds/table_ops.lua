local a = table.create(4, 0)
a[1], a[2], a[3] = 3, 1, 2
table.move(a, 1, 3, 2, a)
table.sort(a, function(x, y) return x > y end)
local b = table.freeze(table.clone(a))
table.clear(a)
assert(#a == 0 and table.isfrozen(b) and b[1] == 3)
assert(not pcall(rawset, b, 1, 0))
