#!/usr/bin/python
#
# asst3-hasher
#
# Verifies working copy-on-write system. This test is intended to exclude
# working swap implementation and only give mark to students who finish
# copy-on-write.
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys

testcases = [
    # nprocs, nsecs, mark
    (8,  1, 7),
    (10, 2, 6),
    (12, 3, 5),
]

total = sum([a[2] for a in testcases])

def run(test, nprocs, nsecs):
    print "%d PROCS"%nprocs
    test.runprogram("/testbin/hasher", "%d %d"%(nprocs, int(nsecs)))
    test.set_timeout(20)
    check = r"hasher: failed .+?\n"
    
    out = test.look_for([test.menu, check, "hasher: created \d+ child"])
    if out <= 1:
        return 0
           
    out = test.look_for([test.menu, check, "hasher: test completed"])
    if out <= 1:
        return 0
          
    return 1

def main():
    test = core.TestUnit("hasher")
    score = 0
    
    for (i, nsecs, mark) in testcases:
        if run(test, i, nsecs):
            score += mark
        else:
            break
        
    test.print_result(score, total)

if __name__ == "__main__":
	main()
	
