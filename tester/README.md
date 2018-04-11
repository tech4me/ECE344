# os161-tester
Tester scripts for labs offered by ECE344, University of Toronto

The purpose of this tester is to automate marking of student's solution.

Overview
--------

* bin: contains the os161-tester shell script
* src: this is initially empty, but you should place os161 source tarball 
  in here (e.g., os161-1.11a.tar.gz). 
* tools: where you should place auxiliary scripts, such as scripts to 
  update a csv file
* scripts: this is where individual test scripts are kept. Look here if
  you want to add or modify a test case.
* sysconfig: in marking mode, we use a specific sys161 conf file for each
  assignment, and this is where they are kept.
  
Setup
-----

Check out the script into the class directory with the root folder specified
as 'tester', e.g.:
```
cd /cad2/ece344s
git clone https://github.com/jacksun007/os161-tester.git tester
```

Place the os161 source tarball into the tester/src folder. The os161 version
number must match what is in os161-tester script (currently 1.11a). The tarball
itself must a folder named os161-XXX where XXX is the version number and the 
folder contains the full os161 source. The script depends on this.

Make sure the students have read and execute permission on everything except for the
tools folder, and you are good to go!

Development
-----------

In case where you want to change parts of the tester. You can check out this repository
into a separate folder (e.g., $HOME/ece344). The tester script has a "hidden" argument
which allows you to change the default installation path for the run so that you can use
your local version to run tests. I suggest adding the following line to your ~/.cshrc
file:

```
alias tester $HOME/ece344/tester/bin/os161-tester -p $HOME/ece344/tester
```

The script assumes you are doing development work on the ug machines, where the cs161
toolchains are available at `/cad2/ece344s/cs161/bin`. Otherwise, you will have to
update the os161-tester script to deal with running it on your home machine.

Mark Breakdown
--------------

## Assignment 2

|   | Feature        | Subtotal | Detailed Breakdown 
|---|----------------|----------|:------------------
|1. | _exit          | 5  | exittest 5/5
|2. | fork           | 9  | forkbomb 9/9
|3. | execv          | 19 | badcall 9/21, addexec 10/10
|4. | menu argument  | 11 | add 5/5, argtest 6/6
|5. | waitpid        | 12 | badcall 12/21
|6. | read           | 13 | crash 13/13 
|7. | write          | 0  | implicitly required for all test cases
|   | 6+7            | 15 | console 15/15
|   | 3+4            | 12 | draw 12/12
|   | 2+5            | 30 | forktest 15/15, wait 15/15
|   | 2+3+5+1        | 24 | facts 24/24

Total: 150

Note:
* write is not required for exittest and forkbomb
* crash also requires correct error handling
* we do not check for the correct exit value in most test cases

## Assignment 3

|   | Feature        | Subtotal | Detailed Breakdown 
|---|----------------|----------|:------------------
|1. |__time          | 12       | timeit 8/8, badcall 4/17
|2. |sbrk            | 25       | badcall 5/17, malloctest 20/20
|3. |page reclamation| 28       | crash 15/15, badcall 8/17, stacktest 5/5
|4. |demand paging   | 45       | bigprog 15/15, btree 20/25, sty 10/10
|5. |swap            | 25       | matmult 10/10, huge 10/10, triplemat 5/5
|6. |copy-on-write   | 18       | hasher 18/18
|   |2+4             | 5        | btree 5/25
|   |5/6             | 20       | forktest 15/15, facts 5/5
|   |5+6             | 7        | parallelvm 7/7

Total: 185

Future Work
-----------

Move console, exittest, and timeit to asst0.


