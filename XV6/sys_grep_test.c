#include "types.h"
#include "user.h"

int delay(void) {
    int i;
    for (i = 0; i < 10000000; i++) {
        for (i = 0; i < 100000; i++) {
        }
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    char user_buf[256];
    int result;

    printf(1, "=== Grep Syscall Test Program ===\n\n");

    // try to find 'borrows' in 'README'
    printf(1, "Test 1: Find 'borrows' in 'README'\n");
    memset(user_buf, 0, sizeof(user_buf));
    result = grep_syscall("borrows", "README", user_buf, sizeof(user_buf));
    // sleep(2);
    printf(1, "Result: %d\n", result);
    printf(1, "Output: %s\n", user_buf);


    // try to find 'nonexistent_word_123' in 'README'
    printf(1, "\nTest 2: Find 'nonexistent_word_123' in 'README'\n");
    memset(user_buf, 0, sizeof(user_buf));
    result = grep_syscall("nonexistent_word_123", "README", user_buf, sizeof(user_buf));
    // sleep(2);
    printf(1, "Result: %d\n", result);
    printf(1, "Output: %s\n", user_buf);


    // try to find 'borrows' in 'no_such_file.txt'
    printf(1, "\nTest 3: Find 'borrows' in 'no_such_file.txt'\n");
    memset(user_buf, 0, sizeof(user_buf));
    result = grep_syscall("borrows", "no_such_file.txt", user_buf, sizeof(user_buf));
    // sleep(2);
    printf(1, "Result: %d\n", result);
    printf(1, "Output: %s\n", user_buf);


    // try to find 'borrows' in 'README' (buffer too small)
    printf(1, "\nTest 5: Find 'borrows' in 'README' (buffer too small)\n");
    memset(user_buf, 0, sizeof(user_buf));
    result = grep_syscall("borrows", "README", user_buf, 10);
    // // delay();
    // sleep(15);
    printf(1, "Result: %d\n", result);
    printf(1, "Output: %s\n", user_buf);

    printf(1, "\n=== Grep Syscall Test Finished ===\n");
    exit();
}