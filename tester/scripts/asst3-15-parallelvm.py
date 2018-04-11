#!/usr/bin/python
#
# asst3-parallelvm
#
# This test can only pass if you have a working swap and copy-on-write
# implementation. The timeout is such that if you only implemented swapping
# then there is no way you can finish in time.
#
# Note: you will need to compile your code in release mode (i.e., not debug)
# in order to pass this test.
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys

nprocs = 18
timeout = 60
retries = 3
mark = 7

def main():
    test = core.TestUnit("parallelvm")  
    
    for r in xrange(retries):
        test.set_timeout(timeout)
        print "ATTEMPT %d/%d"%(r+1, retries)  
        test.runprogram("/testbin/parallelvm", str(nprocs))
        out = test.look_for([test.menu, "subprocesses failed", "Test complete"])
        if out == 2:
            test.print_result(mark, mark)
            return
        else:
            test.restart()
            
    test.print_result(0, mark)   
  
        
if __name__ == "__main__":
    main()

