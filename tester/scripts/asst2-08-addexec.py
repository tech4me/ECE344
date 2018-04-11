#!/usr/bin/python
#
# tests execv() alone -- nothing else
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, sys

def main():
    test = core.TestUnit("addexec")
    test.runprogram("/testbin/addexec")
    test.look_for_and_print_result('Answer: 1234567', 10)

if __name__ == "__main__":
    main()

