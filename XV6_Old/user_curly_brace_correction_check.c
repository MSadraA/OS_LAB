#include "types.h"
#include "user.h"
#include "fcntl.h"
int is_valid_sequence(char *user_arr)
{
    int is_valid=0,i=0;
    char stack[100];
    int top=-1;
    while(user_arr[i]!='\0')
    {
        if(user_arr[i]=='{')
        {
            ++top;
            stack[top]='{';
        }
        else if(top>-1 && (stack[top]=='{' && user_arr[i]=='}'))
            --top;
        else
            return 0;
        i++;
    }
    if(top==-1)
        is_valid=1;
    else
        is_valid=0;
    return is_valid;
}
int main(int argc,char *argv[])
{
    int result_file_descriptor;
    if(argc!=2)
    {
        printf(1,"invalid command!\n");
        exit();
    }
    int result=is_valid_sequence(argv[1]);
    result_file_descriptor=open("Result.txt",O_CREATE|O_WRONLY);
    if(result_file_descriptor<0)
        printf(2,"problem in making result.txt");
    if(result==1)
    {
        write(result_file_descriptor,"RIGHT\n",6);
    }
    else
    {
        write(result_file_descriptor,"WRONG\n",6);
    }
    exit();
}