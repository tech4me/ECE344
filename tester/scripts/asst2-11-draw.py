#!/usr/bin/python
#
# tests execv() and program arguments without fork()
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys, re, random

# build the regular expression for triangle and pyramid
def triangle(s):
    regexp = ""
    for i in xrange(1,s):
        regexp += r"\*{%d}\r*\n"%i
    return regexp + r"\*{%d}"%s

def pyramid(s):
    regexp = ""
    for i in xrange(1,s):
        regexp += r" {%d}\*{%d}\r*\n"%(s-i, i*2-1)
    return regexp + r"\*{%d}"%(s*2-1)

def box(s):
    return r"(\*{%d}\r*\n){%d}\*{%d}"%(s, s-1, s)

shapes = {
    "box" : box, 
    "triangle" : triangle, 
    "pyramid" : pyramid,
}

def main():
    test = core.TestUnit("draw")
    for shape in shapes:
        size = random.randint(3, 9)
        test.runprogram("/testbin/draw", "%s %d"%(shape, size))
        test.set_timeout(15)
        regexp = shapes[shape](size)
        test.look_for_and_print_result(regexp, 4)

if __name__ == "__main__":
    main()

