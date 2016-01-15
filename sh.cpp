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
using std::string;
using std::vector;
using std::remove;

#define MAXARGS 10
#define KNRM  "\x1B[0m"
#define KYEL  "\x1B[33m"

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
	int permissions;   // the permission set to the file
};

struct pipecmd {
	int type;          // |
	struct cmd *left;  // left side of pipe
	struct cmd *right; // right side of pipe
};

struct bgcmd {
	int type;          // &
	struct cmd *cmd;   // the command to be run in background (e.g., an execcmd)
};

int fork1(void);  // Fork but exits on failure.
void tcsetpgrp1();  // tcsetpgrp but exits on failure.
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
			int fd;
			if ((fd = open(rcmd->file, rcmd->mode, rcmd->permissions)) < 0) // Modified to open the file with the right permissions
				fprintf(stderr, "cannot open %s\n", rcmd->file);
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
	cout << KYEL "bsh" << KNRM "> ";
	memset(buf, 0, nbuf);
	fgets(buf, nbuf, stdin);
	if(buf[0] == 0) // EOF
		return -1;
	return 0;
}

const char* getprocessnamebypid(const int pid) { // Return the process name of the given process id
	char* name = (char*)calloc(1024,sizeof(char));
	if (name) {
		sprintf(name, "/proc/%d/cmdline",pid);
		FILE* f = fopen(name,"r");
		if (f) {
			size_t size;
			size = fread(name, sizeof(char), 1024, f);
			if (size > 0) {
				if ('\n' == name[size-1])
					name[size-1] = '\0';
			}
			fclose(f);
		}
	}
	return name;
}

int checktstp(int pid) { // Check if the given process id is in stopped state
	char path2[1035];
	sprintf(path2, "/proc/%d", pid);
	struct stat sts;
    if (stat(path2, &sts) == -1) { // check if pid still running
		return 0;
	}
    FILE *fp;
    char path[1035];
    sprintf(path, "cat /proc/%d/status | grep State", pid);
    fp = popen(path, "r"); // Open the command for reading
    if (fp == NULL) {
    	printf("Failed to run command\n");
    	exit(-1);
    }

    fgets(path, sizeof(path) - 1, fp);
    pclose(fp);

    string output(path, 100);
    if (output.find("T (stopped)") != std::string::npos) {
		return 1;
    }
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
	cout << KYEL "              v1.0 by " KNRM "Annihil " KYEL "& " KNRM "Dziter " KNRM << endl;

	signal(SIGQUIT, (__sighandler_t) sigquithandler);
	signal(SIGINT, (__sighandler_t) siginthandler);
	signal(SIGCHLD, (__sighandler_t) sigchldhandler);
	signal(SIGHUP, (__sighandler_t) sighuphandler);
	signal(SIGTTOU, SIG_IGN);

	// Read and run input commands.
	char buf[100];
	while (getcmd(buf, sizeof(buf)) >= 0) {
		string commands(buf, 100);
		if (commands.find("exit") == 0) {
			cout << KYEL "  ,-* " << KNRM "All background processes have been killed" <<  endl;
			cout << KYEL " (_)  " << KYEL "The BSH team hope you had a good experience" KNRM << endl;
			sighuphandler(); // Sends SIGHUP signal to all stored processes
		}
		else if (commands.find("help") == 0) { // Handle help command
			cout << KYEL "&       - " << KNRM "If added after your command, runs the process in background." <<  endl;
			cout << KYEL "bg      - " << KNRM "Put last executed process in background." <<  endl;
			cout << KYEL "fg      - " << KNRM "Put last executed process in foreground." <<  endl;
			cout << KYEL "killjob - " << KNRM "Stop a background process using its id in jobs." <<  endl;
			cout << KYEL "jobs    - " << KNRM "Display the processes currently in background with pid, pgid and program's name." <<  endl;
		}
		else if (commands.find("killjob") == 0) { // Handle killjob command
			string idstr = commands.substr(7, commands.length()); // Retrieve the index
			int id = atoi(idstr.c_str());
			if (id < 0 || id >= pids.size()) {
				cout << "index out of bound" << endl;
				break;
			}
			pid_t pid = pids[id];
			kill(pid, SIGKILL); // Kill the selected procress by sending SIGKILL to it
		}
		else if (commands.find("jobs") == 0) { // Handle jobs command
			for (int i = 0; i < pids.size(); i++) {
				int pid = pids[i];
				int gpid = getpgid(pid);
				if (pid > 0 && gpid > 0) {
					cout << KNRM "[" << i << "] " << KYEL << pid << KNRM " " << gpid << KYEL " " << getprocessnamebypid(pid) << KNRM << endl;
				}
			}
		}
		else if (commands.find("fg") == 0) { // Handle fg command
			pid_t pid = pids.back();
			kill(pid, SIGCONT); // Send SIGCONT to resume the process
			if (tcsetpgrp(STDIN_FILENO, getpgid(pid)) == -1) { // Put the last backgrounded process to foreground
				perror("tcsetpgrp");
				exit(-1);
			}
			pids.erase(remove(pids.begin(), pids.end(), pid), pids.end()); // Removing process pid from background list
			cout << KNRM "Process " KYEL << getprocessnamebypid(pid) << " (" << pid << ")" << KNRM " sent to foreground" << endl; // Display the process name going into foreground
			if (waitpid(pid, NULL, 0) != -1) { // Awaiting end of process
				cout << " Terminate" << endl;
				tcsetpgrp1(); // Put shell to foreground
			}
		} else if (commands.find("bg") == 0) { // Handle bg command
			pid_t pid = pids.back();
			kill(pid, SIGCONT); // Send SIGCONT to resume the stopped process
		} else if (commands.find("cd") == 0) {
			buf[strlen(buf)-1] = 0; // chop \n
			if (chdir(buf+3) < 0)
				fprintf(stderr, "cannot cd %s\n", buf+3);
		} else {
			struct cmd* cmd = parsecmd(buf);
			int pid = fork1();
			if (pid == 0) { // Child
				if (setpgid(0, 0) == -1) { // Instantiate a new process group to run the cmd
					perror("setpgid");
					exit(-1);
				}
				if (cmd->type != '&'){
					tcsetpgrp1(); // Send foreground's cmd to foreground
				}
				runcmd(cmd);
			}
			else { // Parent
				pids.push_back(pid); // Add process pid to vector
				if (cmd->type == '&')
					cout << "[" << pids.size() << "]" << " " << pid << endl;
			}

			if (cmd->type != '&') {
				int status;
				if (waitpid(pid, &status, WUNTRACED) != -1) { // Awaiting end of process
					if (checktstp(pid)) { // Check if procress stopped (with SIGTSTP)
						cout << " Stopped process " << pid << " stored to background list" << endl;
					} else {
						pids.erase(remove(pids.begin(), pids.end(), pid), pids.end()); // Removing process id from the background pid list
					}
					tcsetpgrp1(); // Put shell to foreground
				}
			}
		}
	}

	return 0;
}

void sigquithandler() {
	cout << " Received signal 3" << endl;
	cout << KYEL "bsh" << KNRM "> " << std::flush;
}

void siginthandler() {
	cout << " Received signal 2" << endl;
	cout << KYEL "bsh" << KNRM "> " << std::flush;
}

void sigchldhandler() {
	pid_t pid;
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { // Iterate over all the closed processes
		pids.erase(remove(pids.begin(), pids.end(), pid), pids.end()); // Removing process id from the background pid list
	}
}

void sighuphandler() {
	for (int i = 0; i < pids.size(); i++) {
		int pid = pids[i];
		if (pid > 0 && getpgid(pid) > 0)
			kill(pid, SIGHUP); // Send SIGUP to all stored process
	}
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
	cmd->permissions = S_IREAD|S_IWRITE;
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
