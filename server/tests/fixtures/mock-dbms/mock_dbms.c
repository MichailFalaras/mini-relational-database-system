#include <stdio.h>

int main(int argc, char *argv[]) {
    // The Node.js - C DBMS communication protocol uses:
    // - stdout for machine-readable application-specific messages
    // - stderr for human-readable debug logs, diagnostics, warnings and errors     
    fprintf(stderr, "Mock MiniDB process started.\n");

    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        fprintf(stderr, "Received input: %s", buffer);
    }

    fprintf(stderr, "Mock MiniDB process exiting.\n");

    return 0;
}