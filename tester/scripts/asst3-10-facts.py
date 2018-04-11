#!/usr/bin/python
#
# asst3-facts
#
# tests copy-on-write implementation when a shared page needs to be loaded
# in from disk 
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, re

mark = 5

def main():
    test = core.TestUnit("facts")
    test.runprogram("/testbin/facts")
    output = "12! +11! +10! +9! +8! +7! +6! +5! +4! +3! +2! +1! = 522956313"
    test.look_for_and_print_result(re.escape(output), mark, menu=1)

if __name__ == "__main__":
	main()
