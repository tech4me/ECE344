#!/usr/bin/python

import core
import sys
import pexpect


# this crash test doesn't test the 'o' option
def main():
    test = core.TestUnit("crash")
    # Please check this and the correct value
    check = 'Fatal user mode trap '
    outputs = {'a' : 2, 'b' : 2, 'c' : 4, 'd' : 3, 'e' : 3, \
                   'g' : 5, 'h' : 2, 'i' : 2, 'j' : 4, 'k' : 4, \
                   'l' : 10, 'm' : 9, 'n' : 9}

    keylist = outputs.keys()
    keylist.sort()
    for i in keylist:
        res = test.runprogram("/testbin/crash")
        test.look_for("Choose:")
        # the second argument is 0, so don't wait for menu prompt
        res = test.send_command(i, 0)
        test.look_for_and_print_result(check + str(outputs[i]), 1)

if __name__ == "__main__":
	main()
