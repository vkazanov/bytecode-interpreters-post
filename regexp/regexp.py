#!/usr/bin/env python
import argparse
import sys
from pprint import pprint

from raddsl.parse import (
    seq, a, many, non, space, alt, quote, some, digit, match, end, eat, push, group, Stream, to, opt
)
from tools import add_pos, attr, make_term

# Terms
#

# scan result terms
Int = make_term("Int")
Op = make_term("Op")

# AST terms
Event = make_term("Event")
Choice = make_term("Choice")
Plus = make_term("Plus")
Star = make_term("Star")
Maybe = make_term("Maybe")
Any = make_term("Any")

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
bar = eat(lambda x: x == ("Op", "|"))
dot = eat(lambda x: x == ("Op", "."))
colon = eat(lambda x: x == ("Op", ":"))
lparen = eat(lambda x: x == ("Op", "("))
rparen = eat(lambda x: x == ("Op", ")"))
quant_plus = eat(lambda x: x == ("Op", "+"))
quant_star = eat(lambda x: x == ("Op", "*"))
quant_maybe = eat(lambda x: x == ("Op", "?"))

nameexp = push(eat(lambda x: x[0] == "Int"))
screenexp = push(eat(lambda x: x[0] == "Int"))

# non-terminals
eventexp = seq(nameexp,
               alt(seq(colon, screenexp),
                   ast_screen_unspecified),
               ast_event)

anyexp = seq(dot, ast_any)
concatexp = lambda x: concatexp(x)
parenexp = seq(lparen, concatexp, rparen)
simpleexp = alt(eventexp, anyexp, parenexp)
repeatexp = alt(seq(simpleexp, alt(seq(quant_plus, ast_plus),
                                   seq(quant_star, ast_star),
                                   seq(quant_maybe, ast_maybe))),
                simpleexp)
unionexp = lambda x: unionexp(x)
unionexp = seq(repeatexp, opt(seq(some(seq(bar, unionexp)), ast_choice)))
concatexp = group(many(unionexp))
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


def main():
    argparser = argparse.ArgumentParser(description="Piglet Matcher Regexp Compiler")
    argparser.add_argument("regexp", help="Regular expression to parse")
    argparser.add_argument("--dump-tokens", action="store_true", help="Dump scanned tokens")
    argparser.add_argument("--dump-ast", action="store_true", help="Dump AST")
    argparser.add_argument("--scan-only", action="store_true", help="Only perform scanning")
    argparser.add_argument("--ast-only", action="store_true", help="Only perform scanning/parsing")
    args = argparser.parse_args()

    scanned_tokens = scan(args.regexp)
    if args.dump_tokens:
        print("Tokens:")
        pprint(scanned_tokens, indent=2)

    if args.scan_only:
        return

    parsed_ast = parse(scanned_tokens)
    if args.dump_ast:
        print("AST:")
        pprint(parsed_ast, indent=2)

    if args.ast_only:
        return


if __name__ == '__main__':
    main()
