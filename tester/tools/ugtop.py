#!/bin/python
#
# ugtop.py
#
# find a low utilization machine within the ug cluster
#
# Author: Kuei (Jack) Sun <kuei.sun@mail.utoronto.ca>
#
# University of Toronto
# 2016
#

import subprocess, re

ugnums = range(132, 180+1) + range(201, 250+1)

def check():
    result = list()
    prog = re.compile(r"\d+\.\d+")
    for num in ugnums:
        output = subprocess.check_output("ssh ug%d top -b -n 1 | head -n 1"%num, 
            shell=True)
        match = prog.findall(output)
        if len(match) > 0:
            entry = (max([float(v) for v in match]), num)
            result.append(entry)
    # result is sorted from lowest utilization to highest
    result.sort()
    return result
    
if __name__ == "__main__":
    res = check()
    for i in xrange(5):
        util, num = res[i]
        print "ug%d load average: %.2f"%(num, util)

