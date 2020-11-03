#include <stdbool.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include "../esh.h"
#include <signal.h>
#include "../esh-sys-utils.h"
#include <string.h>
char * hist[1000];
int count;
static bool
init_plugin(struct esh_shell *shell)
{
    printf("Plugin 'history' initialized...\n");
    count = 0;
    return true;
}

static bool history_builtin(struct esh_command *cmd)
{
    if(strcmp(cmd->argv[0], "history") != 0) {
        int number = 0;
        char* x = calloc(sizeof(char),100);
        while (cmd->argv[number] != NULL){
            x = strcat(x," ");
            x = strcat(x, cmd->argv[number]);

            number++;
        }
        hist[count] = x;
        count++;
        return false;
    }
    else{

        if(cmd->argv[1] == NULL){
            printf("There are total %d commands in history\n", count);
            for(int i =0; i <count; i++){
                printf("%s\n",hist[i]);
            }
        }
        else{
            if(strcmp(cmd->argv[1],"clear") == 0 ){
                for(int i = 0; i < count; i++) {
                    memset(hist[i], 0, 255);
                }
                count = 0;
                printf("History is clear\n");
            }

        }
    }

    return true;
}

struct esh_plugin esh_module = {
        .rank = 1,
        .init = init_plugin,
        .process_builtin = history_builtin
};
