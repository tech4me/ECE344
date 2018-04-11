#!/usr/bin/python
#
# asst3-forktest
#
# this test can pass with either swap implementation or copy-on-write 
# implementation, without needing both.
#
# we run this test many times to look for timing bugs
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys

# 8 + 4 + 2 + 1 = 15
total = 15
nruns = 4

def main():
    test = core.TestUnit("forktest")
    test.set_timeout(150)
    mark = 0
    next = 8
    
    for i in xrange(nruns):
        print "RUN %d/%d"%(i+1, nruns)
        test.runprogram("/testbin/forktest")
        out = test.look_for([test.menu, "([0123]{30,30})$"])
        
        # if we crash or timeout, end test immediately
        if out < 0:
            break
        elif out == 0:
            # did not find expected output, just go again
            print "FAIL"
            continue
        
        # found possible candidate of expected output, check and see
        output = test.kernel.match.group(0)
        output_array = [0] * 4
        for c in output:
            index = int(c)
            output_array[index] += 1
        if (output_array[0] == 2) and (output_array[1] == 4) and \
                (output_array[2] == 8) and (output_array[3] == 16):
            mark += next
            next = next/2
            print "PASS"
        else:
            # did not find correct output 
            print "FAIL"
    
    # either all 4 runs finished or OS crashed at some point        
    test.print_result(mark, total)  

if __name__ == "__main__":
    main()

