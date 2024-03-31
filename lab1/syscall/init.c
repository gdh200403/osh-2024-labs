#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_HELLO 548

int main() {
    char buf[50];
    long result;
    size_t buf_len = sizeof(buf);

    // Test with a buffer that is large enough
    result = syscall(SYS_HELLO, buf, buf_len);
    if (result == 0) {
        printf("System call returned 0, buffer contains: %s\n", buf);
    } else {
        printf("System call returned -1, buffer is too small\n");
    }

    // Test with a buffer that is too small
    result = syscall(SYS_HELLO, buf, 5);
    if (result == 0) {
        printf("System call returned 0, buffer contains: %s\n", buf);
    } else {
        printf("System call returned -1, buffer is too small\n");
    }

    while(1){

    }
}