#include "types.h"
#include "user.h"

#define WRONG_INPUT "Usage: logs"

int main(int argc, char* argv[]) {
    if(argc != 1) {
        printf(1, "%s\n", WRONG_INPUT);
        exit();
    }

    logs();

    exit();
}