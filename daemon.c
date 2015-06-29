/*
 * This is mostly code shamelessly taken from 
 * http://www.4pmp.com/2009/12/a-simple-daemon-in-c/
 * with a few adjustments to better fit our needs.
 */
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "daemon.h"

int pidFilehandle;
char pidPath[64];

void signal_handler(int sig) {
	switch (sig) {
	case SIGHUP:
		/* XXX: handle kill -HUP somehow */
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
	case SIGINT:
	case SIGTERM:
		syslog(LOG_INFO, "Daemon exiting");
		daemonShutdown();
		exit(EXIT_SUCCESS);
		break;
	default:
		syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
		break;
	}
}

void daemonShutdown() {
	char syslogLine[256];

	close(pidFilehandle);
	if(unlink(pidPath))
	{
		snprintf(syslogLine, sizeof syslogLine, "unlink() failed for %s - [%s]", pidPath, strerror(errno));
		syslog(LOG_WARNING, syslogLine);
	}
	else
	{
		;;
		/* Do nothing on unlink() success
		snprintf(syslogLine, sizeof syslogLine, "Pidfile %s deleted", pidPath);
		syslog(LOG_DEBUG, syslogLine);
		 */
	}
}

void daemonize(char *rundir, char *pidfile) {
	int pid, sid, i;
	char str[10]; // will hold process_id (pid)
	struct sigaction newSigAction;
	sigset_t newSigSet;

	/* Check if parent process id is set */
	if (getppid() == 1) {
		/* PPID exists, therefore we are already a daemon */
		return;
	}

	/* Set signal mask - signals we want to block */
	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD); /* ignore child - i.e. we don't need to wait for it */
	sigaddset(&newSigSet, SIGTSTP); /* ignore Tty stop signals */
	sigaddset(&newSigSet, SIGTTOU); /* ignore Tty background writes */
	sigaddset(&newSigSet, SIGTTIN); /* ignore Tty background reads */
	sigprocmask(SIG_BLOCK, &newSigSet, NULL); /* Block the above specified signals */

	/* Set up a signal handler */
	newSigAction.sa_handler = signal_handler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	/* Signals to handle */
	sigaction(SIGHUP, &newSigAction, NULL); /* catch hangup signal */
	sigaction(SIGTERM, &newSigAction, NULL); /* catch term signal */
	sigaction(SIGINT, &newSigAction, NULL); /* catch interrupt signal */

	/* Fork*/
	pid = fork();

	if (pid < 0) {
		/* Could not fork */
		printf("Could not fork() child process");
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		/* Child created ok, so exit parent process */
		printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}

	/* Child continues */

	umask(027); /* Set file permissions 750 */

	/* Get a new process group */
	sid = setsid();

	if (sid < 0) {
		printf("Could not create session for new process group.");
		exit(EXIT_FAILURE);
	}

	/* close all descriptors */
	for (i = getdtablesize(); i >= 0; --i) {
		close(i);
	}

	/* Route I/O connections to /dev/null
	 * we will be using syslog for messaging
	 */
	i = open("/dev/null", O_RDWR); /* Open STDIN */
	dup(i); /* STDOUT */
	dup(i); /* STDERR */
	chdir(rundir); /* change running directory */

	/* Ensure only one copy */
	pidFilehandle = open(pidfile, O_RDWR | O_CREAT, 0600);

	if (pidFilehandle == -1) {
		/* Couldn't open pid file */
		syslog(LOG_ERR, "Could not open PID file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	/* Try to lock file */
	if (lockf(pidFilehandle, F_TLOCK, 0) == -1) {
		/* Couldn't get lock on PID file */
		syslog(LOG_ERR, "Could not lock PID file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	/* Get and format PID */
	snprintf(str,sizeof str, "%d\n", getpid());

	/* write pid to lockfile */
	if(write(pidFilehandle, str, strlen(str))<strlen(str))
	{
		syslog(LOG_ERR,"Failed to write process id into PID file.");
		exit(EXIT_FAILURE);
	};
}
