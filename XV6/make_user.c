#include "types.h"
#include "user.h"

#define WRONG_INPUT "Usage: make_user user_id password"
#define INVALID_ID "Invalid user ID! It must be a positive integer."

int is_valid_number(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {
    if(argc != 3) {
        printf(1, "%s\n", WRONG_INPUT);
        exit();
    }

    if (!is_valid_number(argv[1])) {
        printf(1, "%s\n", INVALID_ID);
        exit();
    }

    int uid = atoi(argv[1]);
    char* pass = argv[2];
    int result = make_user(uid, pass);

    printf(1,"result: %d\n",result);

    exit();
}
