#!/usr/bin/python
#
# tests just _exit()
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, re

def main():
    test = core.TestUnit("exittest")
    test.runprogram("/testbin/exittest")
    idx = test.look_for(["Unknown syscall", "exittest failed", test.menu])
    
    # pass if you either show the menu again, or if you have not 
    # synchronize the menu yet, we'll just wait for the timeout as 
    # long as the error messages does not show up
    if idx in [2, -1]:
        test.print_result(5, 5)
    else:
        test.print_result(0, 5)

if __name__ == "__main__":
	main()
