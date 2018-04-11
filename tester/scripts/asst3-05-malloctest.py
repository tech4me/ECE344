#!/usr/bin/python
#
# Merged all previous separate malloc tests together.
#
# test 1, basic malloc implementation required -- no swap/cow needed
# test 2: swap (probably) required, and a limit needs to be put on how large
#         the heap can grow otherwise the test fails catastrophically.
# test 5: some form of demand paging is necessary to pass this (even though
#         same goes for test 1)
#
# Authors: 
# 
# Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
# Ashvin Goel <ashvin@eecg.toronto.edu>
#
# University of Toronto
# 2016
#

import core, sys

tests = [
    # give basic malloc() implementation more mark in case students
    # implemented malloc() without demand paging working
    (1, 10, 10, 'Passed malloc test 1'),     
    (2, 5, 900, 'Passed malloc test 2'),
    (5, 5, 120, 'Passed malloc test 5'),
]

def main():
    test = core.TestUnit("malloctest")
    for (testnum, mark, timeout, check) in tests:
        test.runprogram("/testbin/malloctest", str(testnum))
        test.set_timeout(timeout)
        test.look_for_and_print_result(check, mark, menu=1)

if __name__ == "__main__":
	main()
