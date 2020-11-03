#include <stdbool.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include "../esh.h"
#include <signal.h>
#include "../esh-sys-utils.h"
#include <string.h>
int ipow(int, int);
static bool
init_plugin(struct esh_shell *shell)
{
    printf("Plugin 'binary' initialized...\n");

    return true;
}

static bool binary_builtin(struct esh_command *cmd)
{

    if(strcmp(cmd->argv[0], "binary") != 0) {
        return false;
    }
    else{
        int number = 0;
        while(cmd->argv[number] != NULL){
            number++;
        }
        if(number < 3){
            printf("Wrong format\n");
            return true;
        }
        else if(number == 3){
            if(strcmp(cmd->argv[1], "int") == 0){
                int number = atoi(cmd->argv[2]);
                for (int i = 31; i >= 0; i--)
                {
                    int k = number >> i;

                    if (k & 1) {
                        printf("1");
                    }
                    else {
                        printf("0");
                    }
                }

                printf("\n");
            }
            else if (strcmp(cmd->argv[1], "bi") == 0){
                int answer = 0;
                int x = 0;
                for(int i = 0; i <strlen(cmd->argv[2]);i++){

                    if(cmd->argv[2][i] == '1'){
                         x  = strlen(cmd->argv[2]) -i - 1;

                        answer += ipow(2, x);

                    }
                }
                printf("%d\n", answer);
            }
        }
    }
    return true;

}

struct esh_plugin esh_module = {
        .rank = 2,
        .init = init_plugin,
        .process_builtin =binary_builtin
};

int ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}

