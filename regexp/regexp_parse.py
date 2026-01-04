import sys
from raddsl.parse import *
from tools import add_pos, attr
from terms import *

# AST node generation
#

ast_integer = to(2, lambda p, x: Int(int(x), pos=p))
ast_op = to(2, lambda p, x: Op(x, pos=p))

SCREEN_UNSPECIFIED = 0
ast_screen_unspecified = to(0, lambda: Int(SCREEN_UNSPECIFIED))
ast_event = to(2, lambda x, y: Event(x[1], y[1], pos=attr(x, "pos")))
ast_choice = to(2, lambda x, y: Choice(x, y))
ast_plus = to(1, lambda x: Plus(x))
ast_star = to(1, lambda x: Star(x))
ast_maybe = to(1, lambda x: Maybe(x))
ast_any = to(0, lambda: Any())
ast_concat = to(1, lambda x: Concat(x))

# Scanner
#

comment = seq(a("#"), many(non(a("\n"))))
ws = many(alt(space, comment))
integer = seq(quote(some(digit)), ast_integer)
OPERATORS = ": | + * ? ( ) .".split()
operator = seq(quote(match(OPERATORS)), ast_op)
tokens = seq(many(seq(ws, add_pos, alt(operator, integer))), ws, end)

# Parser
#

# ops
op = lambda n: eat(lambda x: x == Op(n))

nameexp = push(eat(lambda x: attr(x, "tag") == "Int"))
screenexp = push(eat(lambda x: attr(x, "tag") == "Int"))

# non-terminals
eventexp = seq(nameexp,
               alt(seq(op(":"), screenexp),
                   ast_screen_unspecified),
               ast_event)

anyexp = seq(op("."), ast_any)
concatexp = lambda x: concatexp(x)
parenexp = seq(op("("), concatexp, op(")"))
simpleexp = alt(eventexp, anyexp, parenexp)
repeatexp = seq(simpleexp, opt(alt(seq(op("+"), ast_plus),
                                   seq(op("*"), ast_star),
                                   seq(op("?"), ast_maybe))))
unionexp = lambda x: unionexp(x)
unionexp = seq(repeatexp, opt(seq(some(seq(op("|"), unionexp)), ast_choice)))
concatexp = seq(group(many(unionexp)), ast_concat)
regexp = seq(many(unionexp), end)


def scan(src):
    s = Stream(src)
    if tokens(s):
        return s.out
    else:
        print("Failed to scan (error_pos={}, char={})".format(s.error_pos, s.buf[s.error_pos]),
              file=sys.stderr)
        sys.exit(1)


def parse(src):
    s = Stream(src)
    if regexp(s):
        return s.out
    else:
        print("Failed to parse (error_pos={}, tok={})".format(s.error_pos, s.buf[s.error_pos]),
              file=sys.stderr)
        sys.exit(1)
