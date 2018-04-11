#!/usr/bin/python

import core
import sys

def main():
    test = core.TestUnit("argtest")
    test.runprogram("/testbin/argtest", "test")
    check1 = 'argc: 2'
    # runprogram changes the name of the program
    check2 = 'argv\[0\]: '+ test.prog
    check3 = 'argv\[1\]: test'
    test.look_for_and_print_result(check1, 2)
    test.look_for_and_print_result(check2, 2)
    test.look_for_and_print_result(check3, 2)

if __name__ == "__main__":
	main()
