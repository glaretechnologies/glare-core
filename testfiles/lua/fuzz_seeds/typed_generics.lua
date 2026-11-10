type Node<T> = { value: T, next: Node<T>? }
local function head<T>(node: Node<T>): T
    return node.value
end
local node: Node<number | string> = {value = 7}
assert((head(node) :: number) == 7)
