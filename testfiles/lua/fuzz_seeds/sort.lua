lines = {
      luaH_set = 10,
      luaH_get = 24,
      luaH_present = 48,
    }
	
	a = {}
    for n in pairs(lines) do table.insert(a, n) end
    table.sort(a)
    for i,n in ipairs(a) do print(n) end
	