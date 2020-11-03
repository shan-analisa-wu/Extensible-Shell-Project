Student Information
-------------------
Shan Wu (anali99)
Ziyao Zhu (ziyao99)

How to execute the shell
------------------------
1. Type "make" in the regular command window in src folder to compile and get an
executable called "esh".
2. Type in ./esh in the regular command window to run the executable.
3. The shell is being executed and user can type in commands.


Important Notes
---------------
<Any important notes about your system>


Description of Base Functionality
---------------------------------
<describe your IMPLEMENTATION of the following commands:
jobs, fg, bg, kill, stop, \ˆC, \ˆZ >

jobs: The jobs command prints out jobs(esh_pipeline) started in the shell and teir status.
The implementation of this command is iterating over the job list and print out each job using the
following format:
[job id]	Status	Job typed in the command line

fg: The fg command brings a background job to foreground. First the command
blockes the SIGCHLD signal. Then the command retrieves the job id from the
user typed command. Then it iterates over the job list to find the corresponding job using the job id.
Then it checks the status of the job. If the job is stopped, it sends a SIGCONT to the job. Then it gives terminal
to the corresponding process group(the job). The job's status is then being changed to FOREFROUND and bg_job
is set to false. Then it calles wait_for_job function so the shell wait for all processes in the job to complete.
Finally the terminal is given to the shell and the SIGCHLD is unblocked.

bg: The gb command checks whether a specific background job is stopped. If so, it sends the job signal to let the job
continue to execute. First, it blocks the SIGCHLD signal. Then the command retrieves the job id of the job. And it iterates
over the job list to find the corresponding job. Then the command checks the status of the job. If the job is stopped, it sends
SIGCONT signal to the corresponding process group telling the job to continue to execute. Then the job's status is changed to
BACKGROUND and the current terminal state is being saved. Finally it unblocks the SIGCHLD.

kill: The command kills a user specified job. It retrieves the job id from the user entered command and then iterate over 
the job list to find the corresponding job. If the job is found in the job list, it sends a SIGKILL signal to the corresponding
process group. The job is then be killed and removed from job list. At the beginning of the implementation of the command the SIGCHLD
is being blocked and at the end the SIGCHLD is unblocked.

stop: The command stopes a user specified job. It retrieves the job id from the user entered command and then iterates over the job list to
find the corresponding job. If the job is found, the command sends a SIGSTOP signal the the process group to stop the job. The status
of the job is then being changed to STOPPED. At the beginning of the implementation of the command the SIGCHLD is being blocked and at 
the end the SIGCHLD is unblocked.

ctrl-C: When ctrl-C is pressed, a signal is sent to the shell. The SIGCHLD handler then responds to the signal. If there is a foreground job running
in the shell. The job would be killed. If there is no job currently running in the foreground, the shell itself would be killed. This is achieved by the
sigchld_handler and the child_status_change function. In the child_status_change function, it removes the corresponding job from the job list.

ctrl-Z：When ctrl-Z is pressed, a signal is sent to the shell. The SIGCHLD then responds to the signal. If there is a foreground job running in the shell,
the job would be stopped and the status of the job would be changed to STOPPED. If there is no foreground job currently running, the shell itself would be
stopped. This is achieved by the sigchld_handler and the child_status_change function. In the child_status_change function, it changes the status of the
job to STOPPED and save the current terminal state of the job.


Description of Extended Functionality
-------------------------------------
<describe your IMPLEMENTATION of the following functionality:
I/O, Pipes, Exclusive Access >

I/O: The shell can send input from a file to a command("<"), write the output of a command to a new file or overwrite a file(">"), append the output to
the end of a file(">>"). To achieve this, the command first check the iored_input and iored_output field. If iored_input is not null, the command needs to
read input from a file. Then it opens the specific file with read only mode and set the command's STDIN to be reading from the file using dup2(it sets the
STDIN to be the file descriptor previous open function returned). If the iored_output is not null, the command needs to write its output to a file. There
are two possibilities in this case. First, if append_to_output field is true, then the command append its output to the end of a specific file. The command
then use open to open the file with write only mode, O_APPEND(Positions the file offset at the end of the file before each write operation.) and use dup2 to set
the command's STDOUT to be the file descriptor returned by previous open function. Second, if the append_to_output is false, the command opens the file with
write only mode and O_CREAT to create a new file if the file is not existed. Then it uses dup2 to set the command's STDOUT to be the file descriptor the previous open
function returned. The file descriptors get closed after the dup2 function in each of the above cases.

Pipes: Our implementation of the pipe uses an array of pipe. Before looping over commands in esh_pipeline, an array of pipe with size (number of commands - 1) is
created. While looping over commands, the position of ech commands in being checked. If its the first command, only the write end of the first pipe would be used.
If its the last command, only the read end of the last pipe would be used. If its command in the middle, the write end of the pipe with the same position number would be used
and the read end of pipe with (position number - 1) would be used. In the child process, we use dup2 to set the corresponding read/write end of pipes to STDIN and STDOUT and clse all
the pipes. In the parent process, we close the already used read/write end of the pipes. The logic of closing pipe in parent is the same as what we did in the child processes.

Exclusive Access: This function allows the shell to grant exclusive access to specific jobs. It is achieved by assigning process group. All commands in the same esh_pipeline(they are in the same
job) is assigned the same process group id. We first set the default process group id to be -1. When loop over commands, we check if the process group id has been set. If so, we set the process group id
of the command to be the process group id set previously. If not, we set the process group id to be pid of the first command entered the loop. We also saved the terminal status of each jobs if they are
background or they are switched back to background. When SIGTTOU or SIGTTIN is sent to the shell, the shell give terminal to the corresponding process group
with give_terminal_to function.


List of Plugins Implemented
---------------------------
(Written by Your Team)
1. history
   The plugin retrieves the previously typed command and can clear history if needed.
2. binaryDecimalConverter
   The plugin can convert user typed binary number to decimal and vice versa.
3. var
   The plugin can assign value to a specific variable and change the value of an
   already assigned variable. It can also clear out all the variables.
(Written by Others)
1. campg3+ybryan10_genquote.so
   campg3+ybryan10
2. campg3+ybryan10_randNum.so
   campg3+ybryan10
3. campg3+ybryan10_revstring.so
   campg3+ybryan10
4. colinpeppler+gweihao_factorial.so
   colinpeppler+gweihao
5. jfdenton+sfranklin_runners.so
   jfdenton+sfranklin
6. jfdenton+sfranklin_time.so
   jfdenton+sfranklin
7. emmam99+hdavid9_clock.so
   emmam99+hdavid9
8. emmam99+hdavid9_d20.so
   emmam99+hdavid9
9. emmam99+hdavid9_fib.so
   emmam99+hdavid9
10. lzishuai+yl2017_calc.so
    lzishuai+yl2017

