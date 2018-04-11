#!/usr/bin/python
#
# a cooked-up version of matmult. can pass with swap implemented
# fully correctly.
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

mark = 5
nprocs = 3

def main():
    test = core.TestUnit("triplemat")
    test.runprogram("/testbin/triplemat")
    test.set_timeout(720)
    
    # TODO: we should check for more intermediate input here
    # However, we will need to make sure that matmult prints
    # strings atomically
    
    out = test.look_for([test.menu, 'Fatal user mode trap', 
                        r'Congratulations\! You passed\.'])
    if out <= 1:
        test.print_result(0, mark)
    else:
        test.print_result(mark, mark)

if __name__ == "__main__":
	main()
