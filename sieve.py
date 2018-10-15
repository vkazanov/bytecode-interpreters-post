from __future__ import print_function
import timeit
import sys


def sieve(arr):
    arr_size = len(arr)
    for i in xrange(2, arr_size):
        n = i
        while True:
            n += i
            if n >= arr_size:
                break
            arr[n] = True

    for i in xrange(arr_size):
        if not arr[i]:
            print(i)


def main():
    arr = [False] * 65536
    start_time = timeit.default_timer()
    for x in xrange(100):
        sieve(arr)
    print(timeit.default_timer() - start_time, file=sys.stderr)


if __name__ == '__main__':
    main()
