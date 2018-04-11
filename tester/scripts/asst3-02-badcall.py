#!/usr/bin/python
#
# tests invalid arguments for waitpid(), execv(), sbrk(), and __time()
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
    'd' : [
        r"passed: sbrk\(too-large negative",
        r"passed: sbrk\(huge positive",
        r"passed: sbrk\(huge negative",
        r"passed: sbrk\(unaligned positive",
        r"passed: sbrk\(unaligned negative",
    ],
    'e' : [
        "passed: __time with invalid seconds pointer",
        "passed: __time with kernel seconds pointer",
        "passed: __time with invalid nsecs pointer",
        "passed: __time with kernel nsecs pointer",
    ]
}

# lose 2 marks per failure
loss = 2

def main():
    test = core.TestUnit("badcall")
    test.runprogram("/testbin/badcall")
    # start with some marks in each category
    res = [4, 4, 5, 4]
    top = sum(res)
    j = 0
    for ch in syscalls:
        out = test.look_for([test.menu, "Choose:"])
        if out <= 0:
            print "FAIL"
            res[j] = 0
            continue
        fail = 0
        # the second argument is 0, so don't wait for menu prompt
        test.send_command(ch, 0)
        for output in syscalls[ch]:
            # (jsun): we check for failure messages to speed up the tester
            i = test.look_for(["UH-OH", "FAILURE", "unimplemented", output])
            if i <= 2:
                res[j] -= loss
                fail = 1
        if fail:
            print "FAIL"
        else:
            print "PASS" 
        j += 1
    # you get zero instead of negative marks for too many failures
    res = [ 0 if m < 0 else m for m in res ]
    test.print_result(sum(res), top)

if __name__ == "__main__":
	main()

