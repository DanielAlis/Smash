#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = alarmHandler;
    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    if(sigaction(SIGALRM ,&sa ,NULL) != 0) {
        perror("smash error: failed to set sig-alarm handler");
    }

    //TODO: setup sig alarm handler
    SmallShell& smash = SmallShell::getInstance();
    while(smash.getActiveStatus()) {
        std::cout << smash.getChprompt();
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        if(cmd_line.size()>0)
        {
            smash.executeCommand(cmd_line.c_str());
        }
    }
    return 0 ;
}