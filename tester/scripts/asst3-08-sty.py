#!/usr/bin/python
#
# tests multiple runs of sty to ensure page reclamation is working along with
# demand paging and fork()
#
# Authors: 
# 
# Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
# Ashvin Goel <ashvin@eecg.toronto.edu>
#
# University of Toronto
# 2016
#

import core
import sys
import re

numsty = 6
numrun = 3
mark = 10

def main():
    out = 0
    test = core.TestUnit("sty")
    check = '%d hog... are back in the pen'%numsty
    
    for i in xrange(numrun):
        print "RUN %d/%d"%(i+1, numrun)
        test.runprogram("/testbin/sty", str(numsty))
        out = test.look_for([test.menu, check])
        if out <= 0:
            break
            
    if out > 0:
        test.print_result(mark, mark)
    else:
        test.print_result(0, mark)

if __name__ == "__main__":
    main()
