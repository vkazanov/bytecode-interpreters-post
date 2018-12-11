import sys
from raddsl.rewrite import *
from regexp_parse import SCREEN_UNSPECIFIED
from tools import apply_rule, flatten
from terms import *

def term_error(text):
    def walk(t):
        print("%s: %s" % (text, t.out), file=sys.stderr)
        sys.exit(1)
    return walk

def label(t):
    t.out = "L%d" % t.label_cnt
    t.label_cnt += 1
    return True

expr = delay(lambda: expr)

event = alt(
    rule(Event(let(N=id), SCREEN_UNSPECIFIED), to(lambda v: [
        Next(),
        Name(v.N)
    ])),
    rule(Event(let(N=id), let(S=id)), to(lambda v: [
        Next(),
        Name(v.N),
        Screen(v.S)
    ]))
)

plus = rule(Plus(let(E=expr)),
    let(REPEAT=label), let(DONE=label),
    to(lambda v: [
        Label(v.REPEAT),
        v.E,
        Split(v.REPEAT, v.DONE),
        Label(v.DONE)
    ])
)

star = rule(Star(let(E=expr)),
    let(REPEAT=label), let(MATCH=label), let(DONE=label),
    to(lambda v: [
        Label(v.REPEAT),
        Split(v.MATCH, v.DONE),
        Label(v.MATCH),
        v.E,
        Jump(v.REPEAT),
        Label(v.DONE)
    ])
)

maybe = rule(Maybe(let(E=expr)),
    let(YES=label), let(DONE=label),
    to(lambda v: [
        Split(v.YES, v.DONE),
        Label(v.YES),
        v.E,
        Label(v.DONE)
    ])
)

choice = rule(Choice(let(L=expr), let(R=expr)),
    let(LEFT=label), let(RIGHT=label), let(DONE=label),
    to(lambda v: [
        Split(v.LEFT, v.RIGHT),
        Label(v.LEFT),
        v.L,
        Jump(v.DONE),
        Label(v.RIGHT),
        v.R,
        Label(v.DONE)
    ])
)

expr = alt(
    plus,
    star,
    maybe,
    choice,
    event,
    rule(Any(), to(lambda v: [Next()])),
    rule(Concat(let(E=each(expr))), to(lambda v: v.E)),
    term_error("Unknown node type")
)

trans = each(expr)

def translate(ast):
    m, r = apply_rule(trans, ast, label_cnt=0)
    return flatten(r)
