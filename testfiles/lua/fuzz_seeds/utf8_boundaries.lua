local text = utf8.char(0, 0x7f, 0x800, 0x1f642)
local n = 0
for pos, cp in utf8.codes(text) do
    n += 1
    assert(utf8.offset(text, n) == pos and utf8.codepoint(text, pos) == cp)
end
local length, bad = utf8.len(text .. string.char(255))
assert(n == 4 and length == nil and bad == #text + 1)
