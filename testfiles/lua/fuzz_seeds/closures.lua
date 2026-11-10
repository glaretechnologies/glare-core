local counters = {}
for i = 1, 3 do
    local value = i
    counters[i] = function(delta) value += delta return value end
end
assert(counters[1](2) == 3 and counters[2](-1) == 1)
assert(counters[1](1) == 4)
