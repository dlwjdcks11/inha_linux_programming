#include "smallsh.h"

static char inpbuf[MAXBUF];
static char tokbuf[MAXBUF];
static char *ptr = inpbuf;
static char *tok = tokbuf;

static char special[] = {' ', '\t', '&', ';', '\n', '\0'};

int argCnt = 0; // 전체 parameter 수를 받을 전역변수 저장

int userin(char* p) {
	int c, count;
	ptr = inpbuf;
	tok = tokbuf;

	printf("%s", p);
	count = 0;

	while(1) {
		if ((c = getchar()) == EOF)
			return EOF;
		if (count < MAXBUF)
			inpbuf[count++] = c;
		if (c == '\n' && count < MAXBUF) {
			inpbuf[count] = '\0';
			return count;
		}
		if (c == '\n' || count >= MAXBUF) {
			printf("smallsh: input line too long\n");
			count = 0;
			printf("%s", p);
		}
	}
}

int gettok(char** outptr) {
	int type;
	*outptr = tok;

	while (*ptr == ' ' || *ptr == '\t')
		ptr++;

	*tok++ = *ptr;
	switch (*ptr++) {
		case '\n':
			type = EOL;
			break;
		case '&':
			type = AMPERSAND;
			break;
		case ';':
			type = SEMICOLON;
			break;
		default:
			type = ARG;
			while (inarg(*ptr))
				*tok++ = *ptr++;
	}
	*tok++ = '\0';
	return type;
}

int inarg(char c) {
	char* wrk;

	for (wrk = special; *wrk; wrk++) {
		if (c == *wrk)
			return 0;
	}

	return 1;
}

void procline() {
	char* arg[MAXARG + 1];
	int toktype, type;
	int narg = 0;

	for (;;) {
		switch (toktype = gettok(&arg[narg])) {
			case ARG:
				if (narg < MAXARG)
					narg++;
				break;
			case EOL:
			case SEMICOLON:
			case AMPERSAND:
				if (toktype == AMPERSAND)
					type = BACKGROUND;
				else
					type = FOREGROUND;
				
				if (narg != 0) {
					arg[narg] = NULL;
					argCnt = narg; // 총 parameter 수 전역변수로 저장
					runcommand(arg, type);
				}
				if (toktype == EOL)
					return;
				narg = 0;
				break;
		}
	}
}

int runcommand(char **cline, int where) {
	struct sigaction act;
	act.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_NOCLDWAIT;
	sigemptyset(&(act.sa_mask));
	sigaction(SIGCHLD, &act, NULL); // SIGCHLD가 실행될 떄의 flag들 설정(자식 종료되는거 확인 안해도 자식이 죽을 수 있게 설정)
	
	char* beforePipe[MAXARG + 1]; // pipe 이전 명령어 저장용 buffer (afterPipe의 입력으로 보낼 출력을 만듬)
	char* afterPipe[MAXARG + 1]; // pipe 이후 명령어 저장용 buffer (beforePipe의 결과를 입력으로 받아 결과 출력)
	pid_t pid;
	pid_t pid2; // 두 번쨰 fork용 변수
	int status;

	int signExist = 0; // 꺾쇠가 존재하는지 여부를 저장할 flag
	int pipeIndex = 0; // pipe의 위치를 저장할 변수
	char** tmp = cline; // command line 임시 복사
    
	for (int i = 0; i < argCnt; i++) {
		if (strcmp(">", *tmp) == 0)
			signExist = 1; // 꺽쇠가 존재하면, flag가 1
		if (strcmp("|", *tmp++) == 0)
			pipeIndex = i; // 파이프가 존재하면, 현재 index 저장
	}

	if (pipeIndex != 0) { // pipe가 존재하면,
		for (int i = 0; i < pipeIndex; i++) {
			beforePipe[i] = cline[i];
		}
		beforePipe[pipeIndex] = NULL; // pipe 이전의 명령어 저장하고, 문자열 완성
		
		int j = 0;
		for (int i = pipeIndex + 1; i < argCnt; i++) {
			afterPipe[j] = cline[i];
			j++;
		}
		afterPipe[argCnt - pipeIndex] = NULL; // pipe 이후의 명령어 저장하고, 문자열 완성
	}

    if (strcmp("exit", *cline) == 0) { // 명령어가 exit라면, 
		exit(0); // 프로세스 종료
	}
	else if (strcmp("cd", *cline) == 0) { // 명령어가 cd라면, 
		if (argCnt != 2) { // parameter의 개수가 2가 아니라면, 
            printf("cd needs two arguments include \"cd\"\n");
            return 0; // 에러 출력 후 종료
        }
        *cline++; // command line의 포인터를 하나 증가시켜서 path name에 접근하게 만든다.
		if (chdir(*cline) < 0) { // chdir system call 호출
			perror(*cline); // 에러처리
		}
	}
	else { // 그 외에 exec 함수로 실행이 가능한 명령어라면,
		switch (pid = fork()) {
			case -1:
				perror("smallsh");
				return -1;
			case 0:
				if (where == BACKGROUND)
					signal(SIGINT, SIG_IGN); // background process일 때, SIGINT 시그널 무시

				if (signExist == 0 && pipeIndex == 0) { // 꺾쇠가 없다면, 그냥 수행
					execvp(*cline, cline);
					perror(*cline);
					exit(1);
				}
				else if (pipeIndex != 0) { // pipe가 존재한다면, 		
					int p[2]; // pipe 열어줄 file descriptor가 두개 들어갈 int 배열 선언
					if (pipe(p) == -1) { // pipe 열어줌
						perror("pipe");
						exit(1);
					}

					switch(pid2 = fork()) { // pipe
						case 0: // 자식 프로세스
							if (dup2(p[1], 1) < 0) // p[1]에 stdout fd 연결
								perror("dup2_child");
							close(p[0]); // 안쓰는 fd 닫아줌
							execvp(*beforePipe, beforePipe); // pipe 전 명령어 실행, output은 pipe에 입력된다.
						default: // 부모 프로세스
							if (dup2(p[0], 0) < 0) // p[0]에 stdin fd 연결
								perror("dup2_parent");
							close(p[1]); // 안쓰는 fd 닫아줌
							execvp(*afterPipe, afterPipe); // pipe 이후 명령어 실행, input은 pipe를 읽어서 실행한다.
					}
				}
				else if (signExist == 1) { // 꺾쇠가 있다면, 
					int fd = open(cline[argCnt - 1], O_RDWR | O_CREAT | O_TRUNC, 0644); // 출력 대신 받을 파일 open
					dup2(fd, 1); // 방금 오픈한 descriptor를 stdandard output에 복사, standard output으로 갈 출력이 fd로 나오게 된다.
					close(fd); // fd file descriptor 닫아준다.

					cline[argCnt - 1] = NULL; // 출력받을 파일 이름 부분 삭제 (맨 마지막 index)
					cline[argCnt - 2] = NULL; // 꺾쇠 부분 삭제 (맨 마지막 바로 전 index)

					execvp(*cline, cline); // 나머지 명령어 실행
					perror(*cline);
					exit(1); // 프로세스 종료
				}
		}
	}
	
	if (where == BACKGROUND) {
		printf("[Process id] %d\n", pid);
		return 0;
	}
	if (waitpid(pid, &status, 0) == -1)
		return -1;
	else
		return status;
}