#!/usr/bin/python
#
# tests waitpid(), execv(), _exit(), and fork() together
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, re

def main():
    test = core.TestUnit("facts")
    test.runprogram("/testbin/facts")
    test.set_timeout(15)
    output = "12! +11! +10! +9! +8! +7! +6! +5! +4! +3! +2! +1! = 522956313"
    test.look_for_and_print_result(re.escape(output), 24)

if __name__ == "__main__":
	main()
