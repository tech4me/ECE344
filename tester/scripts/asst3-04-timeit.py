#!/usr/bin/python
#
# asst3-timeit
#
# simply tests the timeit program to run sieve of eratosthenes twice. 
# the timeit program can actually run a program multiple times to 
# help with detecting race conditions -- e.g.,
#
# p testbin/timeit 100 testbin/forktest
#
# but in the interest of students who may have not completed execv() we run
# the simple basic test case.
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys

mark = 8

def main():
    test = core.TestUnit("timeit")
    test.runprogram("/testbin/timeit")
    test.set_timeout(30)
    check = 'sieve: 2 runs took about \d+ seconds'
    test.look_for_and_print_result(check, mark, menu=1)

if __name__ == "__main__":
    main()

