#!/usr/bin/python
#
# asst3-matmult
#
# this test *requires* swap to work because it uses more than 512KB memory
#

import core
import sys

mark = 10

def main():
    test = core.TestUnit("matmult")
    test.runprogram("/testbin/matmult")
    test.set_timeout(180)
    test.look_for_and_print_result('answer is: 8772192', mark, menu=1)

if __name__ == "__main__":
	main()
