#!/usr/bin/python
#
# asst3-huge
#
# this test requires a well-designed page table to handle 8MB of address 
# range from the data region
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys

mark = 10
stages = [ "1", "2.0", "2.1", "2.2", "2.3", "2.4", "2" ]

def main():
    test = core.TestUnit("huge")
    test.runprogram("/testbin/huge")
    # this is per "look_for" so should be reasonable
    test.set_timeout(120)
    for stage in stages:
        out = test.look_for([test.menu, r"stage \[%s\] done"%stage])
        if out <= 0:
            test.print_result(0, mark)
            return
        else:
            print "STAGE %s DONE"%stage
    test.look_for_and_print_result('You passed', mark, menu=1)

if __name__ == "__main__":
	main()
