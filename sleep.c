#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main() {
    printf("sleep...\n");
    sleep(10);
    printf("wakeup\n");
}