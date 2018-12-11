from raddsl.rewrite import Tree, perform


class Head(dict):
    def __eq__(self, right):
        return self["tag"] == right

    def __ne__(self, right):
        return not self.__eq__(right)

    def __repr__(self):
        return self["tag"]


def make_term(tag):
    return lambda *a, **k: (Head(tag=tag, **k),) + a


def attr(term, attribute_name):
    return term[0][attribute_name]


def add_pos(state):
    state.out.append(state.pos)
    return True


def apply_rule(rule, node, **kwargs):
    t = Tree(node)
    for k in kwargs:
        setattr(t, k, kwargs[k])
    return perform(t, rule), t.out


def flatten(node):
    if isinstance(node, list):
        lst = []
        for x in node:
            y = flatten(x)
            lst.extend(y if isinstance(x, list) else [y])
        node[:] = lst
    elif isinstance(node, tuple):
        map(flatten, node)
    return node
