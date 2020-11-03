#include <stdbool.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include "../esh.h"
#include <signal.h>
#include "../esh-sys-utils.h"
#include <string.h>
char * variable[100][2];
int total = 0;
static bool
init_plugin(struct esh_shell *shell)
{
    printf("Plugin 'simple Variable' initialized...\n");
    return true;
}

static bool variable_builtin(struct esh_command *cmd)
{

    if(strcmp(cmd->argv[0], "var") != 0) {
        return false;
    }
    else {
        bool format = false;
        int count = 0;
        while (cmd->argv[count + 1] != NULL) {
            count++;
        }

        if (count == 2 && strcmp(cmd->argv[2], "=") == 0) {
            bool found = false;
            format = true;
            for (int i = 0; i < total; i++) {
                if (strcmp(variable[i][0], cmd->argv[1]) == 0) {
                    printf("The value is %s\n", variable[i][1]);
                    found = true;
                }
            }
            if (found == false) {
                printf("The variable is not found\n");
            }

            return true;
        } else if (count == 3 && strcmp(cmd->argv[2], "=") == 0) {
            format = true;
            for (int i = 0; i < total; i++) {
                if (strcmp(variable[i][0], cmd->argv[1]) == 0) {
                    variable[i][1] = cmd->argv[3];
                    printf("The value of %s is changed to %s\n",variable[i][0], variable[i][1]);
                    return true;
                }
            }

            variable[total][0] = cmd->argv[1];
            variable[total][1] = cmd->argv[3];

            printf("The variable %s is created\n", variable[total][0]);
            total++;

        }
        else if(count == 1 && strcmp(cmd->argv[1], "clear") == 0){
            format = true;
            for(int i = 0; i <total; i++){
                memset(variable[i][1], 0, 255);
                memset(variable[i][0], 0, 255);
            }
            total = 0;
        }
        if(format == false){
            printf("Wrong format\n");

        }
        return true;
    }


}

struct esh_plugin esh_module = {
        .rank = 4,
        .init = init_plugin,
        .process_builtin =variable_builtin
};

