#!/usr/bin/python
#
# bigprog
#
# tests demand paging in the data region without fork() or swap working. 
#
# Authors: 
# 
# Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys
import re

mark = 15

def main():
    out = 0
    test = core.TestUnit("bigprog")
    check = "Passed bigprog test"
    test.runprogram("/testbin/bigprog")
    out = test.look_for_and_print_result(check, mark, menu=1)

if __name__ == "__main__":
    main()
