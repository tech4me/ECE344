#!/usr/bin/python
#
# tests crashes induced by bad memory accesses, including infinite recursion.
# also implicitly tests page reclamation, since we run multiple tests under
# the same instance of the emulator.
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys

# runs palin 4 times to see if page reclamation is completed
def warm_up(test, nruns):
    print "WARM UP"
    for i in xrange(nruns):
        print "RUN %d/%d"%(i+1, nruns)
        test.runprogram("/testbin/palin")
        check = 'IS a palindrome'
        out = test.look_for([test.menu, check])
        if out <= 0:
            return False
    return True

outputs = {'a' : 2, 
           'b' : 2, 
           'c' : 4, 
           'd' : 3, 
           'e' : 3,
           'g' : 5,
           'h' : 2,
           'i' : 2,
           'j' : 4,
           'k' : 4,
           'l' : 10, 
           'm' : 9, 
           'n' : 9
}

crash_o_mark = 2
total = len(outputs) + crash_o_mark 

def runtest(test, i, val, mark):
    check = 'Fatal user mode trap '
    test.runprogram("/testbin/crash")
    
    out = test.look_for([test.menu, "Choose:"])
    # could not start the program. try next test case
    if out <= 0:
        test.print_result(0, mark)
        return
    
    # send command immediately without waiting for menu prompt
    test.send_command(i, 0)
    test.look_for_and_print_result(check + str(val), mark, menu=1)    

def main():
    test = core.TestUnit("crash")

    out = warm_up(test, 4)
    if not out:
        test.print_result(0, total)
        return

    print "START TEST"
    keylist = outputs.keys()
    keylist.sort()
    for i in keylist:
        runtest(test, i, outputs[i], 1)
    
    # the 'o' option with virtual memory will take substantially longer
    test.set_timeout(540)
    runtest(test, "o", 3, crash_o_mark)
    

if __name__ == "__main__":
	main()
