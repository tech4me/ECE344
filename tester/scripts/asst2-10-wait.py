#!/usr/bin/python
#
# tests waitpid(), _exit() and fork() together without needing execv()
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core, re

def main():
    test = core.TestUnit("wait")
    test.runprogram("/testbin/wait")
    test.look_for_and_print_result("1 wekp", 3)
    test.look_for_and_print_result("2 ewp", 3)
    test.look_for_and_print_result("3 wer", 3)
    test.look_for_and_print_result("4 asrcp", 3)  
    test.look_for_and_print_result("5 acp", 3)

if __name__ == "__main__":
	main()
