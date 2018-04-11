#!/usr/bin/python
#
# tests fork() without anything else -- expects OS to not crash
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys

mark = 9

def main():
    test = core.TestUnit("forkbomb")
    check = 'OS\/161 kernel \[\? for menu\]\: '
    test.runprogram("/testbin/forkbomb")
    # run in silent mode so TIMEOUT does not show up
    result = test.look_for(check, 1)

    if result != -1:
        test.print_result(0, mark)
    else:
        test.print_result(mark, mark)

if __name__ == "__main__":
	main()
