local function inspect()
    local f = debug.info(1, 'f')
    local argc, variadic = debug.info(f, 'a')
    assert(argc == 0 and not variadic)
    return debug.traceback('trace')
end
assert(string.find(inspect(), 'trace', 1, true))
