# raddsl
# Author: Peter Sovietov


class Tree:
    def __init__(self, out=None):
        self.scope = None
        self.out = out


class RuleScope(dict):
    def __getattr__(self, n):
        return self[n]


tuple_or_list = (tuple, list)


def match_seq(tree, f, x):
    i = 0
    for y in f:
        if not match(tree, y, x[i]):
            return False
        i += 1
    return True


def match(tree, f, x):
    if callable(f):
        x, tree.out = tree.out, x
        m, tree.out = f(tree), x
        return m
    if type(f) not in tuple_or_list:
        return f == x
    if type(x) not in tuple_or_list or len(x) != len(f):
        return False
    return match_seq(tree, f, x)


def perform(tree, f):
    return f(tree) if callable(f) else match(tree, f, tree.out)


def non(x):
    def walk(tree):
        return not perform(tree, x)
    return walk


def alt(*args):
    def walk(tree):
        old = tree.scope
        for x in args:
            if perform(tree, x):
                return True
            tree.scope = old
        return False
    return walk


def seq(*args):
    def walk(tree):
        old = tree.out
        for x in args:
            if not perform(tree, x):
                tree.out = old
                return False
        return True
    return walk


def let(**kwargs):
    name, value = list(kwargs.items())[0]

    def walk(tree):
        if name in tree.scope:
            return match(tree, tree.scope[name], tree.out)
        if perform(tree, value):
            tree.scope = RuleScope(tree.scope)
            tree.scope[name] = tree.out
            return True
        return False
    return walk


def rule(*args):
    f = seq(*args)

    def walk(tree):
        scope = tree.scope
        tree.scope = RuleScope()
        m = f(tree)
        tree.scope = scope
        return m
    return walk


def to(f):
    def walk(tree):
        tree.out = f(tree.scope)
        return True
    return walk


def where(*args):
    f = seq(*args)

    def walk(tree):
        old = tree.out
        m = f(tree)
        tree.out = old
        return m
    return walk


def build(f):
    def walk(tree):
        tree.out = f(tree)
        return True
    return walk


def cons(h, t):
    def walk(tree):
        if type(tree.out) in tuple_or_list and tree.out:
            m = match(tree, h, tree.out[:len(h)])
            return m and match(tree, t, tree.out[len(h):])
        return False
    return walk


def rewrite_seq(tree, f, x):
    if type(x) is list:
        x = tuple(x)
    i = 0
    for y in f:
        if not rewrite_rec(tree, y, x[i]):
            return False
        x = x[:i] + (tree.out,) + x[i + 1:]
        i += 1
    tree.out = list(x) if type(x) is list else x
    return True


def rewrite_rec(tree, f, x):
    tree.out = x
    if callable(f):
        return f(tree)
    if type(f) not in tuple_or_list:
        return f == x
    if type(x) not in tuple_or_list or len(x) != len(f):
        return False
    return rewrite_seq(tree, f, x)


def rewrite(f):
    def walk(tree):
        old = tree.out
        if not rewrite_rec(tree, f, tree.out):
            tree.out = old
            return False
        return True
    return walk


def each_rec(tree, f, out):
    old = tree.out
    i = 0
    for tree.out in old:
        if type(tree.out) in tuple_or_list:
            if not f(tree):
                tree.out = old
                return False
            out = out[:i] + (tree.out,) + out[i + 1:]
        i += 1
    tree.out = out
    return True


def each(f):
    def walk(tree):
        if type(tree.out) is tuple:
            return each_rec(tree, f, tree.out)
        if type(tree.out) is list:
            m = each_rec(tree, f, tuple(tree.out))
            if m:
                tree.out = list(tree.out)
            return m
        return False
    return walk


def repeat(f):
    def walk(tree):
        while True:
            old = tree.out
            if not perform(tree, f):
                tree.out = old
                return True
    return walk


def id(t):
    return True


def scope(f):
    return lambda t: f(t.scope)(t)


def opt(x):
    return alt(x, id)


def delay(f):
    return lambda t: f()(t)


def act(f):
    return lambda t: f(t, **t.scope)


def guard(f):
    return lambda t: f(t.scope)


def topdown(x):
    f = seq(x, each(delay(lambda: f)))
    return f


def bottomup(x):
    f = seq(each(delay(lambda: f)), x)
    return f


def innermost(x):
    f = bottomup(opt(seq(x, delay(lambda: f))))
    return f
