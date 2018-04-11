#!/usr/bin/python
#
# tests waitpid(), and fork(). waitpid() does not have to pass the correct
# exit status of the child (as long as it does not pass a negative value)
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import core
import sys

mark = 15

def main():
    test = core.TestUnit("forktest")
    test.runprogram("/testbin/forktest")
    test.set_timeout(15)

    out = test.look_for("([0123]{30,30})$")
    if out < 0:
        test.print_result(0, mark)
        return

    output = test.kernel.match.group(0)
    output_array = [0] * 4
    for i in range(len(output)):
        index = int(output[i])
        output_array[index] = output_array[index] + 1
    if (output_array[0] == 2) and (output_array[1] == 4) and \
            (output_array[2] == 8) and (output_array[3] == 16):
        test.print_result(mark, mark)
    else:
        test.print_result(0, mark)

if __name__ == "__main__":
    main()
