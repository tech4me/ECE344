#!/usr/bin/python
#
# tests multiple runs of stacktest to ensure page reclamation is working
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
import os

mark = 5
numrun = 2

def main():
    out = 0
    test = core.TestUnit("stacktest")
    test.set_timeout(30)
    
    for i in xrange(numrun):
        print "RUN %d/%d"%(i+1, numrun)
        test.runprogram("/testbin/stacktest")
        
        for i in range(4000):
            check = "\ncalling foo: n-i = " + str(i)
            out = test.look_for([test.menu, check])
            if out <= 0:
                break
                
        if out <= 0:
            test.print_result(0, mark)
            return
        
        
    test.print_result(mark, mark)  

if __name__ == "__main__":
	main()
