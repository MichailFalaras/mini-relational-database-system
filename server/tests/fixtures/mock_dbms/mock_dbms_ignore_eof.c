#include <stdio.h>
#include <windows.h>

int main(void) {
    fprintf(stderr, "Mock MiniDB process started.\n");

    // Send READY
    printf("{\"version\":1,\"type\":\"ready\",\"capabilities\":[\"query\",\"schema\"]}\n");
    fflush(stdout);
    
    while (1) {
        Sleep(10000);
    }

    return 0;
}