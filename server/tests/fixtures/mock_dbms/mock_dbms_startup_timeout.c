#include <stdio.h>

int main(void) {
    // Stay alive without sending a READY message
    while (getchar() != EOF) {}

    return 0;
}