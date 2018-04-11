#!/usr/bin/python

import core
import sys

def main():
    test = core.TestUnit("Hello World")
    test.look_for_and_print_result("Hello World", 5)

if __name__ == "__main__":
    main()
