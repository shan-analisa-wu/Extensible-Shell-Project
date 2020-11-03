#!/usr/bin/python
#
# Tests the functionality of calc plug-in
#
import sys

# the script will be invoked with two arguments:
# argv[1]: the name of the hosting shell's eshoutput.py file
# argv[2]: a string " -p dirname" where dirname is passed to stdriver.py
#          this will be passed on to the shell verbatim.
esh_output_filename = sys.argv[1]
shell_arguments = sys.argv[2]

import imp, atexit
sys.path.append("/home/courses/cs3214/software/pexpect-dpty/");
import pexpect, shellio, signal, time, os, re, proc_check

#Ensure the shell process is terminated
def force_shell_termination(shell_process):
	c.close(force=True)

#pulling in the regular expression and other definitions
def_module = imp.load_source('', esh_output_filename)
logfile = None
if hasattr(def_module, 'logfile'):
    logfile = def_module.logfile

#spawn an instance of the shell
c = pexpect.spawn(def_module.shell + shell_arguments,  drainpty=True, logfile=logfile)

atexit.register(force_shell_termination, shell_process=c)

##############################Test 1##########################################
# run var command
c.sendline("binary int 10")


# should get expected result
assert c.expect("00000000000000000000000000001010") == 0, "Unexpected output"

##############################Test 2##########################################
# run var command
c.sendline("binary bi 1111")

# should get expected result
assert c.expect('15') == 0, "Unexpected output"





shellio.success()
