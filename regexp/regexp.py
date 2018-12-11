import argparse
import sys
from pprint import pprint
from tools import attr
from regexp_parse import scan, parse, SCREEN_UNSPECIFIED
from regexp_trans import translate


def dump_asm(linear_ast):
    for node in linear_ast:
        if attr(node, "tag") == "Label":
            print("%s:" % node[1])
        else:
            print(" ".join(map(str, node)).upper())
    print("MATCH")


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

    linear_ast = translate(parsed_ast)
    if args.dump_linear:
        print("Linear code:", file=sys.stderr)
        pprint(linear_ast, indent=2, stream=sys.stderr)

    if args.linear_only:
        return

    dump_asm(linear_ast)


if __name__ == '__main__':
    main()
