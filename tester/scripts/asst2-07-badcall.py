#!/usr/bin/python
#
# tests invalid arguments for waitpid() and execv()
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys
import pexpect

syscalls = {
    'b' : [
        r"passed: exec NULL",
        r"passed: exec invalid pointer",
        r"passed: exec kernel pointer",
        r"passed: exec the empty string",
        r"passed: exec .*? with NULL arglist",
        r"passed: exec .*? with invalid pointer arglist",
        r"passed: exec .*? with kernel pointer arglist",
        r"passed: exec .*? with invalid pointer arg",
        r"passed: exec .*? with kernel pointer arg",
    ],
    'c' : [
        r"passed: wait for pid -8",
        r"passed: wait for pid -1",
        r"passed: pid zero",
        r"passed: nonexistent pid",
        r"passed: wait with NULL status",
        r"passed: wait with invalid pointer status",
        r"passed: wait with kernel pointer status",
        r"passed: wait with unaligned status",
        r"passed: wait with bad flags",
        r"passed: wait for self",
        r"passed: wait for parent \(from child\)",
        r"passed: wait for parent test \(from parent\): Operation succeeded", 
    ],
}

def main():
    test = core.TestUnit("badcall")
    test.runprogram("/testbin/badcall")
    for ch in syscalls:
        test.look_for("Choose:")
        # the second argument is 0, so don't wait for menu prompt
        test.send_command(ch, 0)
        for output in syscalls[ch]:
            test.look_for_and_print_result(output, 1) 

if __name__ == "__main__":
	main()

