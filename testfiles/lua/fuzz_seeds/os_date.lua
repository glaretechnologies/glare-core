local t = os.date('!*t', 86400)
assert(t.year == 1970 and t.month == 1 and t.day == 2)
assert(os.difftime(86400, 60) == 86340)
