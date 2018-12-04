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


def translate(ast_node):
    result = []
    label_generator = label_name_generator()
    translate_recur(ast_node, label_generator, result)
    emit_op(result, "MATCH")
    return result


def translate_recur(ast_node, label_generator, result):

    # Just flatten lists
    if isinstance(ast_node, list):
        for child_node in ast_node:
            translate_recur(child_node, label_generator, result)
        return

    # Handle proper nodes
    node_head = ast_node[0]
    if node_head == "Event":
        # Event is matched by it's name and also an optional screen
        _, name, screen = ast_node

        emit_op(result, "NEXT")
        emit_op(result, "NAME", name)
        if screen != SCREEN_UNSPECIFIED:
            emit_op(result, "SCREEN", screen)
    elif node_head == "Any":
        # Just skip no matter what the event is
        emit_op(result, "NEXT")
    elif node_head == "Maybe":
        # Optional event sequence
        _, optional_sequence = ast_node

        yes_label = next(label_generator)
        done_label = next(label_generator)

        emit_op(result, "SPLIT", yes_label, done_label)
        emit_label(result, yes_label)
        translate_recur(optional_sequence, label_generator, result)
        emit_label(result, done_label)
    elif node_head == "Choice":
        # Choice of event sequences, i.e. one of the two options can be present
        _, left_sequence, right_sequence = ast_node
        left_label = next(label_generator)
        right_label = next(label_generator)
        done_label = next(label_generator)

        emit_op(result, "SPLIT", left_label, right_label)

        emit_label(result, left_label)
        translate_recur(left_sequence, label_generator, result)
        emit_op(result, "JUMP", done_label)

        emit_label(result, right_label)
        translate_recur(right_sequence, label_generator, result)
        emit_label(result, done_label)
    elif node_head == "Plus":
        # Plus means that we should match at least one event of the type specified
        _, repeated_sequence = ast_node
        repeat_label = next(label_generator)
        done_label = next(label_generator)

        # One sequence should always be matched
        emit_label(result, repeat_label)
        translate_recur(repeated_sequence, label_generator, result)

        # Now, the repeating part
        emit_op(result, "SPLIT", repeat_label, done_label)
        emit_label(result, done_label)
    elif node_head == "Star":
        # Any number of sequences
        _, repeated_sequence = ast_node
        repeat_label = next(label_generator)
        match_label = next(label_generator)
        done_label = next(label_generator)

        emit_label(result, repeat_label)
        emit_op(result, "SPLIT", match_label, done_label)
        emit_label(result, match_label)
        translate_recur(repeated_sequence, label_generator, result)
        emit_op(result, "JUMP", repeat_label)
        emit_label(result, done_label)
    else:
        print("Unknown node type: {}", ast_node, file=sys.stderr)
        sys.exit(1)


def emit_op(result, opname, *args):
    result.append((opname, *args))


def emit_label(result, label_name):
    result.append((label_name + ":", ))


def label_name_generator():
    label_i = 0
    while True:
        yield "L" + str(label_i)
        label_i += 1


def dump_asm(linear_ast):
    for node in linear_ast:
        print(" ".join(map(str, node)))


def main():
    argparser = argparse.ArgumentParser(description="Piglet Matcher Regexp Compiler")
    argparser.add_argument("regexp", help="Regular expression to parse")

    argparser.add_argument("--dump-tokens", action="store_true", help="Dump scanned tokens")
    argparser.add_argument("--dump-ast", action="store_true", help="Dump AST")
    argparser.add_argument("--dump-linear", action="store_true", help="Dump linear AST")

    argparser.add_argument("--scan-only", action="store_true", help="Only do scanning")
    argparser.add_argument("--ast-only", action="store_true", help="Only do scanning/parsing")
    argparser.add_argument("--linear-only", action="store_true", help="Only do scanning/parsing/flattening")

    args = argparser.parse_args()

    scanned_tokens = scan(args.regexp)
    if args.dump_tokens:
        print("Tokens:", file=sys.stderr)
        pprint(scanned_tokens, indent=2, stream=sys.stderr)

    if args.scan_only:
        return

    parsed_ast = parse(scanned_tokens)
    if args.dump_ast:
        print("AST:", file=sys.stderr)
        pprint(parsed_ast, indent=2, stream=sys.stderr)

    if args.ast_only:
        return

    # TODO: adhoc shit below, to be replaced with proper code
    linear_ast = translate(parsed_ast)
    if args.dump_linear:
        print("Linear code:", file=sys.stderr)
        pprint(linear_ast, indent=2, stream=sys.stderr)

    if args.linear_only:
        return

    dump_asm(linear_ast)


if __name__ == '__main__':
    main()
