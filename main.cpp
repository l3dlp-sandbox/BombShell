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

using std::cout;
using std::endl;

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

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(0);
  
  switch(cmd->type){
    default:
      fprintf(stderr, "unknown runcmd\n");
      exit(-1);

    case ' ':
      ecmd = (struct execcmd*)cmd;
      if(ecmd->argv[0] == 0)
        exit(0);
      execvp(ecmd->argv[0], ecmd->argv);
      break;

    case '>':
    case '<': {
      rcmd = (struct redircmd*)cmd;
      int fd = open(rcmd->file, rcmd->mode);
      dup2(fd, rcmd->fd);
      runcmd(rcmd->cmd);
      break;
    }
      

    case '|': {
      pcmd = (struct pipecmd*)cmd;
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
  cout << KNRM"              __________              ___.     "KYEL"_________.__           .__  .__  " << endl;
  cout << KYEL"       ,--.!, "KNRM"\\______   \\ ____   _____\\_ |__ "KYEL" /   _____/|  |__   ____ |  | |  |  " << endl;
  cout << KYEL"    __/   -*-  "KNRM"|    |  _//  _ \\ /     \\| __ \\ "KYEL"\\_____  \\ |  |  \\_/ __ \\|  | |  |  " << endl;
  cout << KYEL"  ,d08b.  '|`  "KNRM"|    |   (  <_> )  Y Y  \\ \\_\\ \\"KYEL"/        \\|   Y  \\  ___/|  |_|  |__" << endl;
  cout << KYEL"  0088MM       "KNRM"|______  /\\____/|__|_|  /___  /"KYEL"_______  /|___|  /\\___  >____/____/" << endl;
  cout << KYEL"  `9MMP'       "KNRM"       \\/             \\/    \\/   "KYEL"     \\/      \\/     \\/           "KNRM << endl;
  cout << KYEL"              v1.0 by "KNRM"B. de Magnienville, "KYEL"N. Creton & "KNRM"B. Karolewski "KNRM << endl;
  // Read and run input commands.
  char buf[100];
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
    } else {
        int pid = fork1();
        if(pid == 0)
            runcmd(parsecmd(buf));
        wait(&pid);
    }
  }

  return 0;
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

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

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
    s++;
    break;
  case '>':
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
  peek(&s, es, "");
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
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
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

  while(peek(ps, es, "<>")){
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
  while(!peek(ps, es, "|")){
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
