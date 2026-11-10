local sum = 0
for i = 8, 1, -1 do
    if i % 2 == 0 then continue end
    sum += i
    if sum > 10 then break end
end
repeat sum -= 1 until sum <= 10
while sum < 12 do sum += 1 end
assert((if sum == 12 then sum else 0) == 12)
