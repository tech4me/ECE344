#!/usr/bin/python
#
# btree
#
# tests demand paging without fork(), swap, or large data region working. 
# partial mark is given if sbrk() is not implemented. 
#
# Authors: 
# 
# Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#
# Edit 1: stack test can be triggered without menu argument. We'll do this
# instead in the interest of students who didn't finish menu arguments
#

import core
import sys
import re

tests = [
    ("",  20), # stack
    ("-h", 5), # heap
]

def main():
    out = 0
    test = core.TestUnit("btree")
    check = "Passed btree test"
    for (arg, mark) in tests:    
        test.runprogram("/testbin/btree", arg)
        out = test.look_for_and_print_result(check, mark, menu=1)

if __name__ == "__main__":
    main()
