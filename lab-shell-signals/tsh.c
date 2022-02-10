/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */ // A command that does not use the & operator is started as foreground job, and the one that uses is started as background
#define UNDEF 0  /* undefined */
#define FG 1     /* running in foreground */
#define BG 2     /* running in background */
#define ST 3     /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    pid_t pgid;            /* job pgid */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, pid_t pgid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler); /* ctrl-c */ // Remember to uncomment this when it's time
    Signal(SIGTSTP, sigtstp_handler);            /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);            /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {

        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline)
{
    // printf("You entered: %s\n", cmdline);

    FILE *fd[MAXARGS];   // file desriptor
    char *argv[MAXARGS]; // argv array
    int pid[MAXARGS];
    int currpid = 0;
    int cmds[MAXARGS];
    int stdin_redir[MAXARGS];
    int stdout_redir[MAXARGS];
    int jid; // Job id
    int state;
    sigset_t set, oset, pset;

    int bg = parseline(cmdline, argv);

    sigfillset(&set);
    sigemptyset(&oset);
    sigaddset(&oset, SIGCHLD);
    // sigaddset(&mask, SIGINT);
    // sigaddset(&mask, SIGTSTP);

    if (!builtin_cmd(argv))
    {
        int cmdindex = parseargs(argv, cmds, stdin_redir, stdout_redir);
        int pipes[cmdindex - 1][2];

        for (int i = 0; i < cmdindex; ++i)
        {

            if (i != cmdindex - 1)
            {
                if (pipe(pipes[i]) < 0)
                {
                    exit(0);
                }
            }

            sigprocmask(SIG_BLOCK, &oset, &pset); // SIG_BLOCK: The set of blocked signals is the union of the current set and the set argument.

            pid[i] = fork(); // fork child process
            currpid = pid[i];

            if (pid[i] == 0)
            { // in child process?
                sigprocmask(SIG_SETMASK, &oset, NULL);
                setpgid(pid[i], pid[0]); // Put the child process in its own process group

                if (stdin_redir[i] != -1)
                { // Check for input and output redir
                    fd[2 * i] = fopen(argv[stdin_redir[i]], "r");
                    int filenum = fileno(fd[2 * i]);
                    dup2(filenum, 0);
                }

                if (stdout_redir[i] != -1)
                {
                    int indx = ((2 * i) + 1);
                    fd[indx] = fopen(argv[stdout_redir[i]], "w");
                    int out = fileno(fd[indx]);
                    dup2(out, 1);
                }

                if (i > 0)
                {
                    dup2(pipes[i - 1][0], STDIN_FILENO); // default standard input file descriptor number which is 0
                    close(pipes[i][0]);
                }

                if (i != cmdindex - 1)
                {
                    dup2(pipes[i][1], STDOUT_FILENO); // default standard input file descriptor number which should be 1

                    close(pipes[i][0]);
                    close(pipes[i][1]);
                }

                execv(argv[cmds[i]], &argv[cmds[i]]);
                printf("%s: Command not found\n", argv[cmds[i]]);
                exit(1);
            }
            else
            {
                if (i != cmdindex - 1)
                {
                    close(pipes[i][1]);
                }

                if (i > 0)
                {
                    close(pipes[i - 1][0]);
                }
            }
        }

        // Setting states
        if (!bg)
        {
            state = FG;
        }
        else
        {
            state = BG;
        }

        sigprocmask(SIG_BLOCK, &set, NULL);
        addjob(jobs, currpid, pid[0], state, cmdline);
        sigprocmask(SIG_SETMASK, &pset, NULL);

        jid = pid2jid(currpid); // Map process ID to job ID

        if (!bg)
        {
            waitfg(currpid); // an integer corresponding to the process ID of the process we are waiting on.
        }
        else
        {
            printf("[%d] (%d) %s\n", jid, pid[0], cmdline);
        }

        return;
    }
}

/* 
 * parseargs - Parse the arguments to identify pipelined commands
 * 
 * Walk through each of the arguments to find each pipelined command.  If the
 * argument was | (pipe), then the next argument starts the new command on the
 * pipeline.  If the argument was < or >, then the next argument is the file
 * from/to which stdin or stdout should be redirected, respectively.  After it
 * runs, the arrays for cmds, stdin_redir, and stdout_redir all have the same
 * number of items---which is the number of commands in the pipeline.  The cmds
 * array is populated with the indexes of argv corresponding to the start of
 * each command sequence in the pipeline.  For each slot in cmds, there is a
 * corresponding slot in stdin_redir and stdout_redir.  If the slot has a -1,
 * then there is no redirection; if it is >= 0, then the value corresponds to
 * the index in argv that holds the filename associated with the redirection.
 * 
 */
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir)
{
    int argindex = 0; /* the index of the current argument in the current cmd */
    int cmdindex = 0; /* the index of the current cmd */

    if (!argv[argindex])
    {
        return 0;
    }

    cmds[cmdindex] = argindex;
    stdin_redir[cmdindex] = -1;
    stdout_redir[cmdindex] = -1;
    argindex++;
    while (argv[argindex])
    {
        if (strcmp(argv[argindex], "<") == 0)
        {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex])
            { /* if we have reached the end, then break */
                break;
            }
            stdin_redir[cmdindex] = argindex;
        }
        else if (strcmp(argv[argindex], ">") == 0)
        {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex])
            { /* if we have reached the end, then break */
                break;
            }
            stdout_redir[cmdindex] = argindex;
        }
        else if (strcmp(argv[argindex], "|") == 0)
        {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex])
            { /* if we have reached the end, then break */
                break;
            }
            cmdindex++;
            cmds[cmdindex] = argindex;
            stdin_redir[cmdindex] = -1;
            stdout_redir[cmdindex] = -1;
        }
        argindex++;
    }

    return cmdindex + 1;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv)
{
    if (strcmp(argv[0], "quit") == 0)
    {
        exit(0);
        // return 1;
    }
    else if (strcmp(argv[0], "fg") == 0)
    {
        do_bgfg(argv);
        return 1;
    }
    else if (strcmp(argv[0], "bg") == 0)
    {
        do_bgfg(argv);
        return 1;
    }
    else if (strcmp(argv[0], "jobs") == 0)
    {
        listjobs(jobs);
        return 1;
    }
    return 0; /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv)
{

    // Ensure that a command-li argument was passed to bg or fg, and print and error otherwise
    char *cmd = argv[0];
    // printf("%s", cmd);

    if (!argv[1])
    {
        printf("%s command requires PID or %%jobid argument\n", cmd);
        return;
    }

    // printf("%d", argv[1][0]);

    if (isdigit(argv[1][0]) || argv[1][0] == '%') // Is it a PID or %jobid?
    {
        int i = 1;
        while (argv[1][i] != 0)
        {

            // printf("%d", argv[1][0]);
            if (!isdigit(argv[1][i]))
            {
                // printf("I'm here");
                printf("%s: argument must be a PID or %%jobid \n", cmd);
                return;
            }
            i++;
        }
    }
    else
    {
        // printf("I'm here");
        printf("%s: argument must be a PID or %%jobid\n", cmd);
        return;
    }

    // Determine whether the job ID or process ID corresponds to a valid job, and print an error otherwise.
    // How to check whether it's fg or bg?
    // How to check if it's a job id or pid? Jobid needs "%"

    int isFgorBg;

    if (strcmp(argv[0], "fg") == 0)
    {
        isFgorBg = FG; // FG State
    }
    else
    {
        isFgorBg = BG; // BG State
    }

    // printf("FG here: %d; BG here: %d", isFg, isBg);

    // int isPid;
    int isJobid;

    if (strstr(argv[1], "%") == NULL)
    {
        // isPid = 1;
        isJobid = 0;
    }
    else
    {
        // isPid = 0;
        isJobid = 1;
    }

    // printf("PID: %d", isPid);
    // printf("JobId: %d", isJobid);

    char *count = argv[1];

    if (isJobid)
    {
        count = (argv[1] + 1);
        // printf("%d", count);
    }

    struct job_t *currJob;
    pid_t pid;
    int val = strtol(count, NULL, 10); // Convert from string to long int
    // printf("%d", val); // Check if number is right

    if (isJobid)
    {
        currJob = getjobjid(jobs, val);

        if (!currJob)
        {
            printf("%c%d: ", '%', val);
            printf("No such job\n"); // TODO: Why is this not printing the number correctly?
            return;
        }

        pid = currJob->pid;

        if (currJob->state == ST)
        {
            kill((-1 * pid), SIGCONT); // Send a SIGCONT signal to the process group of the job
        }

        // printf("%d", isFgorBg)

        currJob->state = isFgorBg;
    }
    else
    {

        pid = val;

        currJob = getjobpid(jobs, pid);

        if (!currJob)
        {
            // printf("I'm here!");
            printf("(%d): No such process\n", pid);
            return;
        }

        if ((currJob->state = ST))
        {
            kill((-1 * pid), SIGCONT);
        }

        // printf("%d", isFgorBg)

        currJob->state = isFgorBg;
    }

    int jobId;
    char *cmdline;

    if (isFgorBg == BG)
    {
        jobId = currJob->jid;
        cmdline = currJob->cmdline;
        printf("[%d] (%d) %s\n", jobId, pid, cmdline);
    }

    // if fg was specified, then wait on the job
    waitfg(pid);
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while (fgpid(jobs) != 0)
    {
        // sleep(1); // Call sleep(1) repeatedly as long as the job with process ID pid is in the foreground (i.e., has state FG).
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig)
{
    // if (verbose)
    // {
    //     printf("sigchld_handler: entering\n");
    // }

    sigset_t mask;
    sigset_t prev_mask;
    pid_t pid;
    int status;
    int jid; // job id

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) // WNOHANG: return immediately if no child exited | WUNTRACED: also return is child hs stopped
    {
        jid = pid2jid(pid);

        // Stopped, child process terminated because of an uncaught signal (e.g., SIGINT), process terminater normally
        // WIFSTOPPED, WIFSIGNALED, WIFEXITED

        if (WIFSTOPPED(status)) // stopped by delivery of a signal?
        {

            if (jobs[jid - 1].state == FG)
            {
                jobs[jid - 1].state = ST; // ctrl-z FG -> ST, changing state of current job
            }

            printf("Job [%d] (%d) stopped by signal %d\n", jid, pid, WSTOPSIG(status)); // Number of signal that caused child process to stop
        }
        else if (WIFSIGNALED(status)) // terminated by an uncaught signal?
        {
            sigprocmask(SIG_BLOCK, &mask, &prev_mask);
            deletejob(jobs, pid);
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, WTERMSIG(status)); // Number of signal that caused child process to terminate
        }
        else if (WIFEXITED(status)) // did it terminate normally?
        {

            sigprocmask(SIG_BLOCK, &mask, &prev_mask); //
            deletejob(jobs, pid);                      // Delete a job whose PID = pid from the job list
            sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        }
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig)

{
    // if (verbose)
    // {
    //     printf("sigint_handler: entering\n");
    // }

    pid_t pid;
    pid = fgpid(jobs);
    // printf(pid)
    kill((-1 * pid), SIGINT); // Why is it not positive?

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig)
{
    // if (verbose)
    // {
    //     printf("sigtsp_handler: entering\n");
    // }

    pid_t pid;
    pid = fgpid(jobs);
    // printf(pid)
    kill((-1 * pid), SIGTSTP);
    sleep(1);

    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, pid_t pgid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].pgid = pgid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
