#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <setjmp.h> // sigsetjmp, siglongjmp를 사용하기 위해 선언

#define EOL		1
#define ARG		2
#define AMPERSAND	3
#define SEMICOLON	4

#define MAXARG		512
#define MAXBUF		512

#define FOREGROUND	0
#define BACKGROUND	1

int userin(char* p);
void procline();
int gettok(char** outptr);
int inarg(char c);
int runcommand(char **cline, int where);
