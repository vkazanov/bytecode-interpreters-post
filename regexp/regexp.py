#!/usr/bin/env python
import argparse

from raddsl.parse import (
    seq, a, many, non, space, alt, quote, some, digit, match, end, eat, push, group, Stream, to
)
from tools import add_pos, attr, make_term

# Grammar to use
# event_list -> event_choice [" " event_choice]*
# event_choice -> "(" event_descriptor [ "|" event_descriptor]* ")" | event_descriptor
# event_with_modifiers -> event_descriptor [modifier]
# modifier -> "?" | "*" | "+"
# event_descriptor -> event_name [":" event_screen] | event_any
# event_any -> "."
# event_name -> int
# event_screen -> int

# Terms
#
Int = make_term("Int")
Op = make_term("Op")
Event = make_term("Event")

# AST
#
ast_integer = to(2, lambda p, x: Int(int(x), pos=p))
ast_op = to(2, lambda p, x: Op(x, pos=p))

SCREEN_UNSPECIFIED = 0
ast_event_with_screen = to(2, lambda x, y: Event(x[1], y[1], pos=attr(x, "pos")))
ast_event_no_screen = to(1, lambda x: Event(x[1], SCREEN_UNSPECIFIED, pos=attr(x, "pos")))

# Scanner
#
comment = seq(a("#"), many(non(a("\n"))))
ws = many(alt(space, comment))
integer = seq(quote(some(digit)), ast_integer)
OPERATORS = ": | + * ( ) .".split()
operator = seq(quote(match(OPERATORS)), ast_op)
tokens = seq(many(seq(ws, add_pos, alt(operator, integer))), ws, end)

# Parser
#
colon = eat(lambda x: x == ("Op", ":"))
event_name = push(eat(lambda x: x[0] == "Int"))
event_screen = push(eat(lambda x: x[0] == "Int"))
event = alt(seq(event_name, colon, event_screen, ast_event_with_screen),
            seq(event_name, ast_event_no_screen))
event_list = seq(many(event), end)


def scan(src):
    s = Stream(src)
    return s.out if tokens(s) else []


def parse(src):
    s = Stream(scan(src))
    return s.out if event_list(s) else []


def main():
    argparser = argparse.ArgumentParser(description="Piglet Matcher Regexp Compiler")
    argparser.add_argument("regexp", help="Regular expression to parse")
    argparser.add_argument("--scan-only", action="store_true", help="Only perform scanning")
    argparser.add_argument("--tokens", action="store_true", help="Dump scanned tokens")
    args = argparser.parse_args()

    scanned_tokens = scan(args.regexp)
    if args.tokens:
        for t in scanned_tokens:
            print(t)

    if args.scan_only:
        return

    parsed_ast = parse(args.regexp)
    print(parsed_ast)


if __name__ == '__main__':
    main()
