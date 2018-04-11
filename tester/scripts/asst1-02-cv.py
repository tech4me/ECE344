#!/usr/bin/python

import core
import sys

def main():
    result = 0
    test = core.TestUnit("condition variable")
    test.send_command("sy3")
    for i in range(5):
        for i in range(31, -1, -1):
            out = test.look_for('Thread ' + str(i))
            if out < 0:
                result = -1 # failure
                break
        if result == -1: # get out of outer loop on failure
            break
    if result == 0:
        test.look_for_and_print_result('CV test done', 10)
    else: # no partial mark
        test.print_result(0, 10)


if __name__ == "__main__":
	main()
