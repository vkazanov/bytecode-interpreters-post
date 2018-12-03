class Head(dict):
    def __eq__(self, right):
        return self["tag"] == right

    def __ne__(self, right):
        return not self.__eq__(right)

    def __repr__(self):
        return self["tag"]


def make_term(tag):
    return lambda *a, **k: (Head(tag=tag, **k),) + a


is_term = lambda x: type(x) == tuple
attr = lambda t, a: t[0][a]


def add_pos(state):
    state.out.append(state.pos)
    return True
