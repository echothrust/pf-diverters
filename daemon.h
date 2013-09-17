#ifndef DAEMON_H
#define DAEMON_H 1
void daemonShutdown();
void signal_handler(int sig);
void daemonize(char *rundir, char *pidfile);
#endif
