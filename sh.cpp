#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>
#include <algorithm>

using std::cout;
using std::endl;
using std::vector;
using std::remove;

#define MAXARGS 10
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
	int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
	int type;              // ' '
	char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
	int type;          // < or > 
	struct cmd *cmd;   // the command to be run (e.g., an execcmd)
	char *file;        // the input/output file
	int mode;          // the mode to open the file with
	int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
	int type;          // |
	struct cmd *left;  // left side of pipe
	struct cmd *right; // right side of pipe
};

struct bgcmd {
	int type;
	struct cmd *cmd;
};

int fork1(void);  // Fork but exits on failure.
void tcsetpgrp1();  // tcsetprgp but exits on failure.
void sigquithandler();
void siginthandler();
void sigchldhandler();
void sighuphandler();
struct cmd *parsecmd(char*);

vector<int> pids;

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
	if(cmd == 0)
		exit(0);
	
	switch(cmd->type){
		default:
			fprintf(stderr, "unknown runcmd\n");
			exit(-1);

		case ' ': {
			struct execcmd *ecmd = (struct execcmd*) cmd;
			if(ecmd->argv[0] == 0)
				exit(0);
			execvp(ecmd->argv[0], ecmd->argv);
			break;
		}

		case '>':
		case '<': {
			struct redircmd *rcmd = (struct redircmd*) cmd;
			int fd = open(rcmd->file, rcmd->mode);
			dup2(fd, rcmd->fd);
			runcmd(rcmd->cmd);
			break;
		}

		case '|': {
			struct pipecmd *pcmd = (struct pipecmd*) cmd;
			int fd[2];
			if (pipe(fd) < 0) {
				fprintf(stderr, "pipe failed\n");
				break;
			}
			int pid[2];
			if ((pid[0] = fork1()) == 0) {
				dup2(fd[1], 1);
				close(fd[0]);
				runcmd(pcmd->left);
			}
			if ((pid[1] = fork1()) == 0) {
				dup2(fd[0], 0);
				close(fd[1]);
				runcmd(pcmd->right);
			}
			close(fd[0]);
			close(fd[1]);
			wait(&pid[0]);
			wait(&pid[1]);
			break;
		}

		case '&': {
			struct bgcmd *bcmd = (struct bgcmd*) cmd;
			runcmd(bcmd->cmd);
			break;
		}
	}
	exit(0);
}

int
getcmd(char *buf, int nbuf)
{
	printf("$ ");
	memset(buf, 0, nbuf);
	fgets(buf, nbuf, stdin);
	if(buf[0] == 0) // EOF
		return -1;
	return 0;
}

int
main(void)
{
	cout << KNRM "              __________              ___.     " KYEL "_________.__           .__  .__  " << endl;
	cout << KYEL "       ,--.!, " KNRM "\\______   \\ ____   _____\\_ |__ " KYEL " /   _____/|  |__   ____ |  | |  |  " << endl;
	cout << KYEL "    __/   -*-  " KNRM "|    |  _//  _ \\ /     \\| __ \\ " KYEL "\\_____  \\ |  |  \\_/ __ \\|  | |  |  " << endl;
	cout << KYEL "  ,d08b.  '|`  " KNRM "|    |   (  <_> )  Y Y  \\ \\_\\ \\" KYEL "/        \\|   Y  \\  ___/|  |_|  |__" << endl;
	cout << KYEL "  0088MM       " KNRM "|______  /\\____/|__|_|  /___  /" KYEL "_______  /|___|  /\\___  >____/____/" << endl;
	cout << KYEL "  `9MMP'       " KNRM "       \\/             \\/    \\/   " KYEL "     \\/      \\/     \\/           " KNRM << endl;
	cout << KYEL "              v1.0 by " KNRM "B. de Magnienville, " KYEL "N. Creton & " KNRM "B. Karolewski " KNRM << endl;

	signal(SIGQUIT, (__sighandler_t) sigquithandler);
	signal(SIGINT, (__sighandler_t) siginthandler);
	signal(SIGCHLD, (__sighandler_t) sigchldhandler);
	signal(SIGHUP, (__sighandler_t) sighuphandler);
	signal(SIGTTOU, SIG_IGN);

	// Read and run input commands.
	char buf[100];
	while(getcmd(buf, sizeof(buf)) >= 0){
		if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
			buf[strlen(buf)-1] = 0;  // chop \n
			if(chdir(buf+3) < 0)
				fprintf(stderr, "cannot cd %s\n", buf+3);
		} else {
			// int pid = fork1();
			// struct cmd* cmd = parsecmd(buf);
			// if (pid == 0)
			// 	runcmd(cmd);
			// wait(&pid);

			struct cmd* cmd = parsecmd(buf);
			int pid = fork1();
			if (pid == 0) { // Child
				if (setpgid(0, 0) == -1) {
					perror("setpgid");
					exit(-1);
				}
				if (cmd->type != '&'){
					tcsetpgrp1(); //Commands go to foreground (for non-background commands)
				}
				runcmd(cmd);
			}
			else { // Parent
				pids.push_back(pid); //Adds the process to the set of running processes
				if (cmd->type == '&')
					cout << "[" << pids.size() << "]" << " " << pid << endl;
			}

			if (cmd->type != '&') { //For non-background commands, wait for the command to end and set the shell to foreground process group again.
				int status;
				if (waitpid(pid, &status, WUNTRACED) != -1){ //Waits for the process to end
					pids.erase(remove(pids.begin(), pids.end(), pid), pids.end());
					tcsetpgrp1(); //Shell goes back in foreground
				}
			}
		}
	}

	return 0;
}

void sigquithandler() {
	cout << " Terminate (core dump)" << endl;
	cout << endl << "$ " << std::flush;
}

void siginthandler() {
	cout << " Terminate" << endl;
	cout << endl << "$ " << std::flush;
}

void sigchldhandler() {
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0){ //Retrieves the exitted child pid
		pids.erase(remove(pids.begin(), pids.end(), pid), pids.end());
	};
}

void sighuphandler() {
	for (int i = 0; i < pids.size(); i++) {
		int pid = pids[i];
		if (pid > 0 && getpgid(pid) > 0)
			kill(pid, SIGHUP);
	}
	// for(vector<int>::iterator it = pids.begin(); it != pids.end(); ++it) {
	// 	if (*it > 0 && getpgid(*it) > 0)
	// 		kill(*it,SIGHUP); //Sends the SIGHUP to all running process groups
	// }
	pids.clear(); //Resets the pid set (array of running process groups ids) : All processes were normally killed by the SIGHUP signal
	exit(0);
}

int
fork1(void)
{
	int pid;
	pid = fork();

	if(pid == -1)
		perror("fork");
	return pid;
}

void 
tcsetpgrp1()
{
	int success = tcsetpgrp(STDIN_FILENO, getpgrp());

	if (success == -1) {
		perror("tcsetpgrp");
		exit(-1);
	}
}

struct cmd*
execcmd(void)
{
	struct execcmd *cmd;

	cmd = (struct execcmd*) malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = ' ';
	return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
	struct redircmd *cmd;

	cmd = (struct redircmd*) malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = type;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
	cmd->fd = (type == '<') ? 0 : 1;
	return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
	struct pipecmd *cmd;

	cmd = (struct pipecmd*) malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = '|';
	cmd->left = left;
	cmd->right = right;
	return (struct cmd*)cmd;
}

struct cmd*
bgcmd(struct cmd *subcmd)
{
	struct bgcmd *cmd;

	cmd = (struct bgcmd*)malloc(sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = '&';
	cmd->cmd = subcmd;
	return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|&>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
	char *s;
	int ret;
	
	s = *ps;
	while(s < es && strchr(whitespace, *s))
		s++;
	if(q)
		*q = s;
	ret = *s;
	switch(*s){
	case 0:
		break;
	case '|':
	case '<':
	case '>':
	case '&': // Needed to handle background process
		s++;
		break;
	default:
		ret = 'a';
		while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
			s++;
		break;
	}
	if(eq)
		*eq = s;
	
	while(s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

// ps : pointer to a string
// es : pointer to the end of the string
// moves *ps to the next character, returns true if this character is in the
// string toks
int
peek(char **ps, char *es, char *toks)
{
	char *s;

	s = *ps;
	while(s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
	int n = es - s;
	char *c = (char*) malloc(n+1);
	assert(c);
	strncpy(c, s, n);
	c[n] = 0;
	return c;
}

struct cmd*
parsecmd(char *s)
{
	char *es;
	struct cmd *cmd;

	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, (char*)"");
	if(s != es){
		fprintf(stderr, "leftovers: %s\n", s);
		exit(-1);
	}
	return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
	struct cmd *cmd;

	cmd = parsepipe(ps, es);
	while(peek(ps, es, (char*)"&")){
		gettoken(ps, es, 0, 0);
		cmd = bgcmd(cmd);
	}
	return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
	struct cmd *cmd;

	cmd = parseexec(ps, es);
	if(peek(ps, es, (char*)"|")){
		gettoken(ps, es, 0, 0);
		cmd = pipecmd(cmd, parsepipe(ps, es));
	}
	return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
	int tok;
	char *q, *eq;

	while(peek(ps, es, (char*)"<>")){
		tok = gettoken(ps, es, 0, 0);
		if(gettoken(ps, es, &q, &eq) != 'a') {
			fprintf(stderr, "missing file for redirection\n");
			exit(-1);
		}
		switch(tok){
		case '<':
			cmd = redircmd(cmd, mkcopy(q, eq), '<');
			break;
		case '>':
			cmd = redircmd(cmd, mkcopy(q, eq), '>');
			break;
		}
	}
	return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
	char *q, *eq;
	int tok, argc;
	struct execcmd *cmd;
	struct cmd *ret;
	
	ret = execcmd();
	cmd = (struct execcmd*)ret;

	argc = 0;
	ret = parseredirs(ret, ps, es);
	while(!peek(ps, es, (char*)"|&")){
		if((tok=gettoken(ps, es, &q, &eq)) == 0)
			break;
		if(tok != 'a') {
			fprintf(stderr, "syntax error\n");
			exit(-1);
		}
		cmd->argv[argc] = mkcopy(q, eq);
		argc++;
		if(argc >= MAXARGS) {
			fprintf(stderr, "too many args\n");
			exit(-1);
		}
		ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	return ret;
}
