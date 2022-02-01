#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>

void sigint_handler(int signum) {
	// send SIGKILL to all processes in group, so this process and children
	// will terminate.  Use SIGKILL because SIGTERM and SIGINT (among
	// others) are overridden in the child.
	kill(-getpgid(0), SIGKILL);
}

int main(int argc, char *argv[]) {
	char *scenario = argv[1];
	int pid = atoi(argv[2]);

	struct sigaction sigact;

	// Explicitly set flags
	sigact.sa_flags = SA_RESTART;
	sigact.sa_handler = sigint_handler;
	// Override SIGINT, so that interrupting this process sends SIGKILL to
	// this one and, more importantly, to the child.
	sigaction(SIGINT, &sigact, NULL);

	switch (scenario[0]) {

	case '0':
		kill(pid, SIGHUP);
		sleep(1);
		break;
	case '1':
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '2':
		kill(pid, SIGINT);
		sleep(3);
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '3':
		kill(pid, SIGINT);
		sleep(3);
		kill(pid, SIGINT);
		sleep(3);
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '4':
		kill(pid, SIGINT);
		sleep(1);
		kill(pid, SIGHUP);
		sleep(5);
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '5':
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '6':
		kill(pid, SIGINT);
		sleep(1);
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGUSR1);
		sleep(1);
		kill(pid, SIGSTKFLT); // This is the corresponding number to 16
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '7':
		kill(pid, SIGINT);
		sleep(1);
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGUSR1);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '8':
		kill(pid, SIGHUP);
		sleep(1);
		kill(pid, SIGSYS); // Off 31
		sleep(1);
		kill(pid, SIGUSR1); // 10
		sleep(1);
		// Get foo to greater than 0
		kill(pid, SIGPWR); // SIGPWR 30
		sleep(5);
		kill(pid, SIGCHLD);
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		kill(pid, SIGUSR2); // 10
		sleep(1);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	case '9':
		kill(pid, SIGUSR2);
		sleep(1);
		kill(pid, SIGSYS); // This is ok to use cause it matches number 31?
		sleep(1);
		kill(pid, SIGQUIT); // 8, 9 
		sleep(1);
		kill(pid, SIGPWR);
		sleep(1);
		kill(pid, SIGSYS); // 1, 2
		sleep(5);
		kill(pid, SIGTERM);
		sleep(1);
		break;
	}
	waitpid(pid, NULL, 0);
}
