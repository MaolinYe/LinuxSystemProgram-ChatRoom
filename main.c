#include <stdio.h>
#include <time.h>
#include <fcntl.h>

int main() {
    time_t currentTime;
    time(&currentTime);
    int fd=open("test.txt",O_WRONLY);
    fprintf(fd,"��ǰʱ��Ϊ: %s", ctime(&currentTime));

    return 0;
}
