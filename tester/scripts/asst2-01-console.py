#!/usr/bin/python
#
# tests read() and write() for console only
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys

def send_key(test, key):
    test.send_command(key, 0)
    test.look_for_and_print_result(r"passed\.", 1)

def main():
    test = core.TestUnit("console")
    test.runprogram("/testbin/console")
    test.look_for_and_print_result("hello world", 1)
    test.look_for_and_print_result("false warning", 1)
    send_key(test, "\n")
    send_key(test, "6")
    for i in [1, 2, 3, 4, 5]:
        test.look_for_and_print_result(r"%d\.\.\. passed\."%i, 1)
    for i in [6, 7, 8]:
        res = test.look_for_and_print_result(r"%d\.\.\. passed\."%i, 1)
        if res == -1:
            # send a random key to trigger the next command in sequence
            test.send_command("\n", 0)
    for i in [9, 10, 11]:
        test.look_for(r"test case %d\.\.\."%i)
        send_key(test, "I")   
    
if __name__ == "__main__":
    main()
