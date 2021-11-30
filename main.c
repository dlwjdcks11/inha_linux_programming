#include "smallsh.h"

char prompt[4096]; // getcwd의 결과를 받을 buffer 아주 크게 설정
sigjmp_buf position; // jump를 위한 분기점 설정

void handler(int signo) { // sigaction의 handler로 설정
    printf("\n");
    siglongjmp(position, 1); // sigsetjmp 지점으로 점프
}

int main() {
    struct sigaction act;
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL); // SIGINT 시그널에 대하여 커스텀 핸들러 적용
	
    int tmp = sigsetjmp(position, 1); // SIGINT가 발생될 시 siglongjmp를 이용해 넘어올 분기 설정

    while(getcwd(prompt, 4096) != NULL && userin(strcat(prompt, "$ ")) != EOF)
        procline();
     // getcwd로 현재 경로를 가져오고, strcat을 이용하여 실제 shell처럼 뒤에 $를 붙여준다.
        
	return 0;
}
