#include "types.h"
#include "user.h"

#define WRONG_INPUT "Usage: logout"

int main(int argc, char* argv[]) {
    if(argc != 1) {
        printf(1, "%s\n", WRONG_INPUT);
        exit();
    }

    int result = logout();

    printf(1,"result: %d\n",result);

    exit();
}
