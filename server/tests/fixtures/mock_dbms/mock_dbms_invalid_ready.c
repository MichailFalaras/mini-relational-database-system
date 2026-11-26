#include <stdio.h>

int main(int argc, char *argv[]) {
    // Invalid protocol version
    printf("{\"version\":2,\"type\":\"ready\",\"capabilities\":[\"query\",\"schema\"]}\n");
    fflush(stdout);

    // Keep stdin open until the manager terminates us
    while (getchar() != EOF) {}

    return 0;
}