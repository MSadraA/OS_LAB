#include "types.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        printf(1, "Usage: %s <string>\n", argv[0]);
        exit();
    }

    int sum = 0;
    for(int i = 1 ; i <= argc ; i++){
        char *str = argv[i];
        int current = 0;
        int in_number = 0;
    
        for (int i = 0; str[i] != '\0'; i++) {
            if (str[i] >= '0' && str[i] <= '9') {
                current = current * 10 + (str[i] - '0');
                in_number = 1;
            } else if (in_number) {
                sum += current;
                current = 0;
                in_number = 0;
            }
        }
        if (in_number)
            sum += current;
    }

    int fd = open("result.txt", O_CREATE | O_WRONLY);
    if (fd < 0) {
        printf(1, "Cannot open result.txt\n");
        exit();
    }

    printf(fd, "%d\n", sum);
    close(fd);

    exit();
}
