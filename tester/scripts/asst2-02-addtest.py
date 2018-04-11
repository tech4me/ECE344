#!/usr/bin/python

import core
import sys
import random

def addtest(mark):
    test = core.TestUnit("add")
    a = random.randint(0, 49999)
    b = random.randint(0, 49999)
    test.runprogram("/testbin/add", "%d %d"%(a, b))
    check = 'Answer: %d'%(a+b)
    test.look_for_and_print_result(check, mark)
    
if __name__ == "__main__":
	addtest(5)
