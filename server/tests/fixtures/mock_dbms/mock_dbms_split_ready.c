#include <stdio.h>

int main (int argc, char *argv[]) {
    printf("{\"version\":1,\"type\":");
    fflush(stdout);

    printf("\"ready\",\"capabilities\":[]}\n");
    fflush(stdout);
    return 0;
}