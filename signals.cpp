#include "signals.h"


using namespace std;

void ctrlZHandler(int sig_num) {
    SmallShell& small_shell = SmallShell::getInstance();
    cout << "smash: got ctrl-Z" << endl;
    if(small_shell.getActiveCMD() == nullptr){
        return;
    }
    int res = kill(small_shell.getActiveCMD()->getCmdPID(), SIGSTOP);
    if (res != SUCCESS){
        smashError::SyscallFailed("kill");
        return;
    }
    cout << "smash: process " << small_shell.getActiveCMD()->getCmdPID()  << " was stopped" << endl;
    small_shell.addJobShell(small_shell.getActiveCMD(),true);
    small_shell.setActiveCMD(nullptr);
}

void ctrlCHandler(int sig_num) {
    SmallShell& small_shell = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    if(small_shell.getActiveCMD() == nullptr || waitpid(small_shell.getActiveCMD()->getCmdPID(), nullptr, WNOHANG) != 0){
        small_shell.setActiveCMD(nullptr);
        return;
    }
    int res = kill(small_shell.getActiveCMD()->getCmdPID(), SIGKILL);
    if (res != SUCCESS){
        smashError::SyscallFailed("kill");
        return;
    }
    cout << "smash: process " << small_shell.getActiveCMD()->getCmdPID() << " was killed" << endl;
    small_shell.setActiveCMD(nullptr);
}

void alarmHandler(int sig_num) {
    cout << "smash: got an alarm" << endl;
    SmallShell& small_shell = SmallShell::getInstance();
    if(small_shell.getActiveCMD() != nullptr && waitpid(small_shell.getTimeoutPid(), NULL, WNOHANG) != 0){
        small_shell.removeJobByPID(small_shell.getTimeoutPid());
        small_shell.timeoutRemoveByPID(small_shell.getTimeoutPid());
        return;
    }
    int res = kill(small_shell.getTimeoutPid(), SIGKILL);
    if(res != 0)
    {
        smashError::SyscallFailed("kill");
        return;
    }
    cout << "smash: " << small_shell.getTimeoutCmdLine()  << " timed out!" << endl;
    small_shell.timeoutRemoveByPID(small_shell.getTimeoutPid());
}

