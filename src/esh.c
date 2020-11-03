/*
 * esh - the 'pluggable' shell.
 *
 * Developed by Godmar Back for CS 3214 Fall 2009
 * Virginia Tech.
 */
#include <stdio.h>
#include <readline/readline.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>

#include "esh.h"
#include "esh-sys-utils.h"

struct esh_command_line* job_list;
int jid;

int builtIn(struct esh_command* cmd, struct termios* terminal, pid_t shell);
static void give_terminal_to(pid_t, struct termios*);
static void wait_for_job(struct esh_pipeline*);
void child_status_change(pid_t child, int status);
void loop_over_command(struct esh_command_line*, struct termios*, pid_t);
void evaluate(struct esh_command*, struct termios*, pid_t, int**, int, int);
void kill_job(int job_id);
void stop(int);
int** do_pipe(struct esh_pipeline*);
void close_pipe(int**, int);

struct esh_pipeline* find_job(int);
int find_jid(struct esh_pipeline*);
void jobs(void);
static void sigchld_handler(int sig, siginfo_t *info, void *_ctxt);
void fg(int job_id, struct termios* terminal, pid_t shell);
void bg(int job_id);
void printJob(struct esh_pipeline* p);
void io_direct(struct esh_command * cmd);
void free_pipe(int**, int);


static void
usage(char *progname)
{
    printf("Usage: %s -h\n"
           " -h            print this help\n"
           " -p  plugindir directory from which to load plug-ins\n",
           progname);

    exit(EXIT_SUCCESS);
}

/* Build a prompt by assembling fragments from loaded plugins that
 * implement 'make_prompt.'
 *
 * This function demonstrates how to iterate over all loaded plugins.
 */
static char *
build_prompt_from_plugins(void)
{
    char *prompt = NULL;

    for (struct list_elem * e = list_begin(&esh_plugin_list);
         e != list_end(&esh_plugin_list); e = list_next(e)) {
        struct esh_plugin *plugin = list_entry(e, struct esh_plugin, elem);

        if (plugin->make_prompt == NULL)
            continue;

        /* append prompt fragment created by plug-in */
        char * p = plugin->make_prompt();
        if (prompt == NULL) {
            prompt = p;
        } else {
            prompt = realloc(prompt, strlen(prompt) + strlen(p) + 1);
            strcat(prompt, p);
            free(p);
        }
    }

    /* default prompt */
    if (prompt == NULL)
        prompt = strdup("esh> ");

    return prompt;
}

/* The shell object plugins use.
 * Some methods are set to defaults.
 */
struct esh_shell shell =
        {
                .build_prompt = build_prompt_from_plugins,
                .readline = readline,       /* GNU readline(3) */
                .parse_command_line = esh_parse_command_line /* Default parser */
        };

int
main(int ac, char *av[])
{
    int opt;
    list_init(&esh_plugin_list);

    /* Process command-line arguments. See getopt(3) */
    while ((opt = getopt(ac, av, "hp:")) > 0) {
        switch (opt) {
            case 'h':
                usage(av[0]);
                break;

            case 'p':
                esh_plugin_load_from_directory(optarg);
                break;
        }
    }

    esh_plugin_initialize(&shell);
    struct termios* init_terminal;
    init_terminal = esh_sys_tty_init();
    give_terminal_to(getpgrp(), init_terminal);
    job_list = calloc(1, sizeof(struct esh_command_line));
    list_init(&(job_list->pipes));
    jid = 0;
    esh_signal_sethandler(SIGCHLD, sigchld_handler);
    pid_t shell_pgrp = getpgrp();

    /* Read/eval loop. */
    for (;;) {
        /* Do not output a prompt unless shell's stdin is a terminal */
        char * prompt = isatty(0) ? shell.build_prompt() : NULL;
        char * cmdline = shell.readline(prompt);
        free (prompt);

        if (cmdline == NULL)  /* User typed EOF */
            break;

        struct esh_command_line * cline = shell.parse_command_line(cmdline);
        loop_over_command(cline, init_terminal, shell_pgrp);
        free (cmdline);
        if (cline == NULL)                  /* Error in command line */
            continue;

        if (list_empty(&cline->pipes)) {    /* User hit enter */
            esh_command_line_free(cline);
            continue;
        }

        esh_command_line_print(cline);
        esh_command_line_free(cline);
    }
    free(job_list);
    return 0;
}


/**
 * This method loop over each esh_pipeline and evaluate them.
 * @param line the list of esh_pipeline
 * @param terminal the status of the terminal of the shell
 * @param shell the pid of the shell
 */
void loop_over_command(struct esh_command_line* line, struct termios* terminal, pid_t shell){
    while(!list_empty(&(line->pipes))){
        struct list_elem* e = list_pop_front(&line->pipes);
        struct esh_pipeline* pipe = list_entry(e, struct esh_pipeline, elem);
        struct list_elem* a = list_begin(&(pipe->commands));
        struct esh_command* cmd = list_entry(a, struct esh_command, elem);

        // check if the command is a built-in command
        if(builtIn(cmd, terminal, shell) != 1) {
            //give default value to process group id and job id
            pipe->pgrp = -1;
            pipe->jid = 0;

            esh_signal_block(SIGCHLD);
            int** array_pipe = do_pipe(pipe);
            int num_command = list_size(&(pipe->commands));
            int count = 0;

            //loop over commands in each esh_pipeline
            for (struct list_elem *a = list_begin(&(pipe->commands));
                 a != list_end(&(pipe->commands)); a = list_next(a)) {
                struct esh_command *cmd = list_entry(a, struct esh_command, elem);
                evaluate(cmd, terminal, shell,array_pipe,count, num_command);
                count++;
            }

            //check if the job is foreground or background
            if (!pipe->bg_job) {
                pipe->status = FOREGROUND;
                give_terminal_to(pipe->pgrp, terminal);
                wait_for_job(pipe);
                give_terminal_to(getpid(), terminal);
            } else {
                printf("[%d] %d\n", pipe->jid, pipe->pgrp);
                esh_sys_tty_save(&(pipe->saved_tty_state));
                pipe->status = BACKGROUND;
            }
            free_pipe(array_pipe, num_command);
            esh_signal_unblock(SIGCHLD);
        }
    }
}

/**
 * This method add jobs to job list, fork for non-builtin command, assign
 * process group id for jobs and handle the pipeline and io-redirection.
 *
 * @param cmd the commandto be forked
 * @param terminal the terminal status of the shell
 * @param shell the pid of the shell
 * @param array the array of pipe
 * @param index the index of the command
 * @param num_command the total number of commands in the esh_pipeline
 */
void evaluate(struct esh_command* cmd, struct termios* terminal, pid_t shell, int** array, int index, int num_command){
    pid_t pid;
    struct esh_pipeline* cor_esh_pipeline;
    cor_esh_pipeline = cmd->pipeline;

    //assign job id and add jobs to job list
    if(list_empty(&(job_list->pipes))){
        jid = 1;
        cor_esh_pipeline->jid = jid;
        list_push_back(&(job_list->pipes), &(cor_esh_pipeline->elem));
    }
    else{
        if(cor_esh_pipeline->jid == 0) {
            jid++;
            cor_esh_pipeline->jid = jid;
            list_push_back(&(job_list->pipes), &(cor_esh_pipeline->elem));
        }
    }

    //fork
    pid = fork();
    if(pid == 0){
        //assign process group id
        //when you forked and your process group id is -1 (meaning it's the first command in the pipeline, we will need to set the process group id of this forked process to
        //its own process id. if it's not the first command in the pipeline, set group id of the forked process to be pipeline's group id (cor_esh_pipeline->pgrp)
        if(cor_esh_pipeline->pgrp == -1){
            //this is the first command in the pipeline, set its process group id to be its pid
            cor_esh_pipeline->pgrp = getpid();
            setpgid(getpid(), cor_esh_pipeline->pgrp);
        }
        else {
            //this is not the first command in th pipeline, set its process group id to cor_esh_pipeline->pgrp
            setpgid(getpid(), cor_esh_pipeline->pgrp);
        }

        //check if the command is foreground or background
        if(!cor_esh_pipeline->bg_job){
            cor_esh_pipeline->status = FOREGROUND;

        }
        else{
            cor_esh_pipeline->status = BACKGROUND;
            esh_sys_tty_save(terminal);
        }

        //check if the command contains pipe
        //complete the pipe functionality
        if(num_command > 1) {
            if (index == 0) {
                dup2(array[0][1], STDOUT_FILENO);
                close_pipe(array, num_command);
            } else if (index == num_command - 1) {
                dup2(array[index - 1][0], STDIN_FILENO);
                close_pipe(array, num_command);

            } else {
                dup2(array[index - 1][0], STDIN_FILENO);
                dup2(array[index][1], STDOUT_FILENO);
                close_pipe(array, num_command);
            }
        }

        //i/o redirection
        io_direct(cmd);

        if(execvp(cmd->argv[0], cmd->argv) < 0){
            esh_sys_fatal_error("Invalid command.");
        }
    }
    else{
        //assign process group id
        if (cor_esh_pipeline->pgrp == -1) {
            cor_esh_pipeline->pgrp = pid;
            setpgid(pid, cor_esh_pipeline->pgrp);
        }
        else {
            setpgid(pid, cor_esh_pipeline->pgrp);
        }
        cmd->pid = pid;

        //check if is a background or foreground command
        if(!cor_esh_pipeline->bg_job){
            cor_esh_pipeline->status = FOREGROUND;
            give_terminal_to(cor_esh_pipeline->pgrp, terminal);
            give_terminal_to(getpid(), terminal);
        }
        else{
            esh_sys_tty_save(&(cor_esh_pipeline->saved_tty_state));
            cor_esh_pipeline->status = BACKGROUND;
        }

        //close corresponding already used pipeline in the parent process
        if(num_command > 1) {
            if (index == 0) {
                close(array[0][1]);
            } else if (index == num_command - 1) {
                close(array[index - 1][0]);
            } else {
                close(array[index - 1][0]);
                close(array[index][1]);
            }
        }

    }
}

/**
 * Check if the command is builtin command
 * @param cmd the command
 * @param terminal the terminal status of the shell
 * @param shell the pid of the shell
 * @return return 0 if it is not builtin command, otherwise, return 1
 */
int builtIn(struct esh_command* cmd, struct termios* terminal, pid_t shell)
{
    //run the plugins
    for (struct list_elem * e = list_begin(&esh_plugin_list);
         e != list_end(&esh_plugin_list); e = list_next(e)) {
        struct esh_plugin *plugin = list_entry(e, struct esh_plugin, elem);

        if (plugin->process_builtin == NULL) {
            continue;
        }
        bool x = plugin->process_builtin(cmd);
        if(x){
            return 1;
        }
    }

    struct esh_pipeline* ori_pipe = cmd->pipeline;
    if(strcmp(cmd->argv[0], "jobs") == 0){
        jobs();
        return 1;
    }
    else if(strcmp(cmd->argv[0],"bg") == 0){
        bg(find_jid(ori_pipe));
        return 1;
    }
    else if(strcmp(cmd->argv[0], "fg") == 0){
        fg(find_jid(ori_pipe), terminal, shell);
        return 1;
    }
    else if(strcmp(cmd->argv[0],"kill") == 0 ){
        kill_job(find_jid(ori_pipe));
        return 1;
    }
    else if(strcmp(cmd->argv[0], "stop") == 0){
        stop(find_jid(ori_pipe));
        return 1;
    }
    return 0;
}



/* Wait for all processes in this pipeline to complete, or for
 * the pipeline's process group to no longer be the foreground
 * process group.
 * You should call this function from a) where you wait for
 * jobs started without the &; and b) where you implement the
 * 'fg' command.
 *
 * Implement child_status_change such that it records the
 * information obtained from waitpid() for pid 'child.'
 * If a child has exited or terminated (but not stopped!)
 * it should be removed from the list of commands of its
 * pipeline data structure so that an empty list is obtained
 * if all processes that are part of a pipeline have
 * terminated.  If you use a different approach to keep
 * track of commands, adjust the code accordingly.
 */
static void
wait_for_job(struct esh_pipeline *pipeline)
{
    assert(esh_signal_is_blocked(SIGCHLD));

    while (pipeline->status == FOREGROUND && !list_empty(&pipeline->commands)) {
        int status;
        pid_t child = waitpid(-1, &status, WUNTRACED);
        if (child != -1) {
            child_status_change(child, status);
        }
    }
}

/*
 * SIGCHLD handler.
 * Call waitpid() to learn about any child processes that
 * have exited or changed status (been stopped, needed the
 * terminal, etc.)
 * Just record the information by updating the job list
 * data structures.  Since the call may be spurious (e.g.
 * an already pending SIGCHLD is delivered even though
 * a foreground process was already reaped), ignore when
 * waitpid returns -1.
 * Use a loop with WNOHANG since only a single SIGCHLD
 * signal may be delivered for multiple children that have
 * exited.
 */
static void
sigchld_handler(int sig, siginfo_t *info, void *_ctxt)
{
    pid_t child;
    int status;

    assert(sig == SIGCHLD);

    while ((child = waitpid(-1, &status, WUNTRACED|WNOHANG)) > 0) {
        child_status_change(child, status);
    }
}

/**
 * Assign ownership of ther terminal to process group
 * pgrp, restoring its terminal state if provided.
 *
 * Before printing a new prompt, the shell should
 * invoke this function with its own process group
 * id (obtained on startup via getpgrp()) and a
 * sane terminal state (obtained on startup via
 * esh_sys_tty_init()).
 */
static void
give_terminal_to(pid_t pgrp, struct termios *pg_tty_state)
{
    esh_signal_block(SIGTTOU);
    int rc = tcsetpgrp(esh_sys_tty_getfd(), pgrp);
    if (rc == -1)
        esh_sys_fatal_error("tcsetpgrp: ");

    if (pg_tty_state)
        esh_sys_tty_restore(pg_tty_state);
    esh_signal_unblock(SIGTTOU);
}

/**
 * Handling the status change of the child
 *
 * @param child the pid of the child to be handled
 * @param status the status of the child
 */
void child_status_change(pid_t child, int status){
    for(struct list_elem * e = list_begin(&(job_list->pipes));
        e != list_end(&(job_list->pipes)); e = list_next(e)) {
        struct esh_pipeline *pipe = list_entry(e, struct esh_pipeline, elem);
        for(struct list_elem * e = list_begin(&(pipe->commands));
            e != list_end(&(pipe->commands)); e = list_next(e)) {
            struct esh_command *cmd = list_entry(e, struct esh_command, elem);
            struct esh_pipeline* opipe = cmd->pipeline;
            if(cmd->pid == child) {
                // if the command is stopped
                if (WIFSTOPPED(status)) {
                    printf("Child %d stopped\n", child);
                    opipe->status = STOPPED;
                    esh_sys_tty_save(&(opipe->saved_tty_state));
                    printJob(opipe);
                }
                //if the command exited normally
                if (WIFEXITED(status)) {
                    list_remove(&(cmd->elem));
                    if(list_empty(&opipe->commands)) {
                        list_remove(&(opipe->elem));
                    }
                }
                //if the command receives signal
                if (WIFSIGNALED(status)) {
                    list_remove(&(cmd->elem));
                    if(list_empty(&opipe->commands)) {
                        list_remove(&(opipe->elem));
                    }
                }
                //if the command is continued
                if(WIFCONTINUED(status)) {
                    esh_sys_tty_restore(&(opipe->saved_tty_state));
                }
            }
        }
    }
}

/**
 * Handling the bg command
 *
 * @param job_id the job id of the job being called bg
 */
void bg(int job_id){
    esh_signal_block(SIGCHLD);
    struct esh_pipeline* job;
    job = find_job(job_id);
    if(job->status == STOPPED){
        kill(-job->pgrp, SIGCONT);
    }
    job->status = BACKGROUND;
    esh_sys_tty_save(&(job->saved_tty_state));
    esh_signal_unblock(SIGCHLD);
}

/**
 * Handling the fg command
 *
 * @param job_id the job id of the job being called fg
 * @param shell the pid of the shell
 * @param terminal the terminal status of the shell
 */
void fg(int job_id, struct termios* terminal, pid_t shell){
    struct esh_pipeline* job;
    job = find_job(job_id);

    //print out the command
    for (struct list_elem *e = list_begin(&(job->commands));
         e != list_end(&(job->commands)); e = list_next(e)) {
        struct esh_command *cmd = list_entry(e, struct esh_command, elem);
        char **argv = cmd->argv;
        while (*argv) {
            printf("%s", *argv++);
            if(*argv) {
                printf(" ");
            }
        }
        if (list_size(&job->commands) > 1 && cmd->pipeline != list_entry(list_back(&(job->commands)), struct esh_pipeline, elem)) {
            printf("|");
        }
    }
    printf("\n");
    fflush(stdout);
    esh_signal_block(SIGCHLD);
    job->bg_job = !job->bg_job;
    //give terminal to the process group
    give_terminal_to(job->pgrp, &(job->saved_tty_state));
    if(job->status == STOPPED){
        kill(-job->pgrp, SIGCONT);
    }
    job->status = FOREGROUND;
    wait_for_job(job);
    //give terminal back to the shell
    give_terminal_to(shell, terminal);
    esh_signal_unblock(SIGCHLD);
}

/**
 * Handling the kill command
 *
 * @param job_id the job id of the job being killed
 */
void kill_job(int job_id){
    esh_signal_block(SIGCHLD);
    struct esh_pipeline* job;
    job = find_job(job_id);
    if(job != NULL) {
        kill(-job->pgrp, SIGKILL);
    }
    esh_signal_unblock(SIGCHLD);
}

/**
 * Handling the stop command
 *
 * @param job_id the job id of the job being stopped
 */
void stop(int job_id){
    esh_signal_block(SIGCHLD);
    struct esh_pipeline* job;
    job = find_job(job_id);
    if(job != NULL) {
        kill(-job->pgrp, SIGSTOP);
    }
    esh_signal_unblock(SIGCHLD);
}

/**
 * Find corresponding jobs according to the job id
 *
 * @param job_id the job id user entered
 */
struct esh_pipeline* find_job(int job_id){
    struct esh_pipeline* job;
    job = NULL;
    //loop over job list to find the corresponding job
    for(struct list_elem * e = list_begin(&(job_list->pipes));
        e != list_end(&(job_list->pipes)); e = list_next(e)) {
        struct esh_pipeline* pipe = list_entry(e, struct esh_pipeline, elem);
        if(pipe->jid == job_id){
            job = pipe;
        }
    }
    return job;
}

/**
 * Find the passed-in job id according to the user input
 *
 * @param pipe the esh_pipeline to find the job id from
 */
int find_jid(struct esh_pipeline* pipe){
    int job_id = 0;
    for(struct list_elem * a = list_begin(&(pipe->commands));
        a != list_end(&(pipe->commands)); a = list_next(a)){
        struct esh_command* cmd = list_entry(a, struct esh_command, elem);
        if(cmd->argv[1] != NULL){
            if(strstr("-", cmd->argv[1])){
                job_id = atoi(cmd->argv[2]);
            }
            else{
                job_id = atoi(cmd->argv[1]);
            }
        }
    }
    return job_id;
}

/**
 * This method prints out all the jobs started and currently exist on the shell
 */
void jobs(void){
    char* status[] = {[FOREGROUND] = "FOREGROUND", [BACKGROUND] = "Running", [STOPPED] = "Stopped", [NEEDSTERMINAL] = "Needterminal"};
    if(!list_empty(&(job_list->pipes))) {
        for (struct list_elem *a = list_begin(&(job_list->pipes));
             a != list_end(&(job_list->pipes)); a = list_next(a)) {
            struct esh_pipeline *p = list_entry(a, struct esh_pipeline, elem);
            printf("[%d]\t%s\t\t", p->jid, status[p->status]);
            printf("(");
            int count = 0;
            for (struct list_elem *e = list_begin(&(p->commands));
                 e != list_end(&(p->commands)); e = list_next(e)) {
                struct esh_command *cmd = list_entry(e, struct esh_command, elem);
                char **argv = cmd->argv;

                while (*argv) {
                    printf("%s", *argv++);
                    if(*argv) {
                        printf(" ");
                    }
                }
                if (list_size(&p->commands) > 1 && count < (list_size(&p->commands) -1)) {
                    printf(" | ");

                }
                count++;
            }
            if(p->bg_job){
                printf(" &");
            }
            printf(")");
            printf("\n");
        }
    }
}

/**
 * This command prints out a single job
 *
 * @param p the job(esh_pipeline) to be printed
 */
void printJob(struct esh_pipeline* p){
    char* status[] = {[FOREGROUND] = "FOREGROUND", [BACKGROUND] = "Running", [STOPPED] = "Stopped", [NEEDSTERMINAL] = "Needterminal"};
    printf("[%d] %s ", p->jid, status[p->status]);
    printf("(");
    int count = 0;
    for (struct list_elem *e = list_begin(&(p->commands));
         e != list_end(&(p->commands)); e = list_next(e)) {
        struct esh_command *cmd = list_entry(e, struct esh_command, elem);
        char **argv = cmd->argv;
        while (*argv) {
            printf("%s", *argv++);
            if(*argv) {
                printf(" ");
            }
        }
        if (list_size(&p->commands) > 1 && count < (list_size(&p->commands) -1)) {
            printf(" | ");
        }
        count++;
    }
    if(p->bg_job){
        printf(" &");
    }
    printf(")");
    printf("\n");
}


/**
 * Create an array of pipe
 * @param pipeline the pipeline that needs to create pipes with
 * @return the array of pipes
 */
int** do_pipe(struct esh_pipeline* pipeline) {
    int size =list_size(&(pipeline->commands));
    int **array =malloc(sizeof(int*)*(size -1));
    for(int y = 0; y <size - 1; y++) {
        array[y] = malloc(sizeof(int)*2);
        pipe(array[y]);
    }
    return array;
}

/**
 * This method free the dynamically allocated array of pipes
 * @param array the array of pipes to be freed
 * @param num_command the number of commands in the esh_pipeline
 */
void free_pipe(int** array, int num_command){
    for(int y = 0; y <num_command - 1; y++) {
        free(array[y]);
    }
    free(array);
}

/**
 * This method closes all the pipes
 * @param array the array of pipes to be closed
 * @param num_command the total number of commands in the esh_pipeline
 */
void close_pipe(int** array, int num_command){
    for(int i = 0; i < num_command - 1; i++){
        close(array[i][0]);
        close(array[i][1]);

    }
}


/**
 * This method handles the i/o redirection
 * @param cmd The i/o redirection command to be handled
 */
void io_direct(struct esh_command * cmd){
    //if the command reads input from a file
    if(cmd->iored_input != NULL){
        int ffd = open(cmd->iored_input, O_RDONLY, 0);
        dup2(ffd, STDIN_FILENO);
        close(ffd);
    }
    //if the command writes output to a file
    if(cmd->iored_output != NULL){
        //check if the command append to a file
        if(cmd->append_to_output){
            int sfd = open(cmd->iored_output ,  O_WRONLY | O_APPEND | O_CREAT,S_IRWXU) ;
            dup2(sfd, STDOUT_FILENO);
            close(sfd);
        }
        else{
            int tfd = open(cmd->iored_output , O_CREAT | O_WRONLY,S_IRWXU) ;
            dup2(tfd, STDOUT_FILENO);
            close(tfd);

        }
    }
}

