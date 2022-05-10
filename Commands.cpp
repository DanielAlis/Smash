#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <utime.h>
#include <limits.h>
#include <cstring>
#include <fstream>


using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";


#include <string>


int nthOccurrence(const std::string &str, const std::string &findMe, int nth) {
    size_t pos = 0;
    int cnt = 0;

    while (cnt != nth) {
        pos += 1;
        pos = str.find(findMe, pos);
        if (pos == std::string::npos)
            return -1;
        cnt++;
    }
    return pos;
}

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}


int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool _isDigits(const std::string &str) {
    return (str.find_first_not_of("0123456789") == std::string::npos
            || (str.substr(0, 1).find_first_not_of("-123456789") == std::string::npos &&
                str.substr(1).find_first_not_of("0123456789") == std::string::npos));

}

bool _isRedirectionCmd(std::string rd_cmd) {
    return (rd_cmd.substr(1, (rd_cmd.length() - 1)).find(">") != string::npos && rd_cmd.at(1) != '>' &&
            rd_cmd.at(rd_cmd.length() - 1) != '>');
}

bool _isPipeCmd(std::string rd_cmd) {
    return (rd_cmd.substr(1, (rd_cmd.length() - 1)).find("|") != string::npos && rd_cmd.at(1) != '|' &&
            rd_cmd.at(rd_cmd.length() - 1) != '|');
}
// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() {
    this->chprompt = "smash> ";
    this->plastPwd = new char[COMMAND_ARGS_MAX_LENGTH];
    this->job_list = JobsList();
    this->fg_command = nullptr;
    this->shellActive = true;
    this->cmdLine = "";
    this->timeout_list = new std::vector<TimeoutCommand *>();
    this->timeout_pid = -1;
    this->timeout_cmd_line = "";
//    this->activeCommandPID = -1; //TODO: deffine for -1
}

SmallShell::~SmallShell() {
// TODO: add your implementation
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command::Command(const char *cmd_line) {
    char *args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("timeout") == 0) {
        this->is_timeout = true;
        std::string new_cmd = "";
        for (int i = 2; i < num_of_args; i++) {
            new_cmd += args[i];
            new_cmd += " ";
        }
        this->actual_cmd = new char[new_cmd.length()];
        strcpy(this->actual_cmd, new_cmd.c_str());
        _removeBackgroundSign(this->actual_cmd);
    } else {
        this->actual_cmd = nullptr;
        this->is_timeout = false;
    }
    int size = std::string(cmd_line).length();
    this->cmd_line = new char[size];
    bg_cmd = new char[size];
    strcpy(this->cmd_line, cmd_line);
    strcpy(this->bg_cmd, (cmd_line));
    _removeBackgroundSign(this->bg_cmd);
    if (_isBackgroundComamnd(cmd_line)) {
        bg_command = true;
    } else {
        bg_command = false;
    }
    if (_isBackgroundComamnd(cmd_line)) {
        bg_command = true;
    } else {
        bg_command = false;
    }
    this->cmd_pid = getpid();

}

Command *SmallShell::CreateCommand(const char *cmd_line) {

    char *args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    this->job_list.removeFinishedJobs();
    if (_isPipeCmd(cmd_s)) {
        std::string cmd1, cmd2;
        int pos_of_pip_sign = cmd_s.find("|");
        bool err = false;
        if (cmd_s.find("|&") != string::npos) {
            err = true;
            cmd1 = _trim(cmd_s.substr(0, pos_of_pip_sign));
            cmd2 = _trim(cmd_s.substr(pos_of_pip_sign + 2));
        } else {
            cmd1 = _trim(cmd_s.substr(0, pos_of_pip_sign));
            cmd2 = _trim(cmd_s.substr(pos_of_pip_sign + 1));
        }
        return new PipeCommand(cmd_line, cmd1, cmd2, err);
    }
    if (_isRedirectionCmd(cmd_s)) {
        std::string file;
        std::string rd_cmd;

        int pos_of_rd_sign = cmd_s.find(">");
        int flags;
        if (cmd_s.find(">>") != string::npos) {
            flags = (O_CREAT | O_APPEND | O_WRONLY);
            file = _trim(cmd_s.substr(pos_of_rd_sign + 2));
            rd_cmd = _trim(cmd_s.substr(0, pos_of_rd_sign));
        } else {
            flags = (O_CREAT | O_WRONLY | O_TRUNC);
            file = _trim(cmd_s.substr(pos_of_rd_sign + 1));
            rd_cmd = _trim(cmd_s.substr(0, pos_of_rd_sign));
        }
        char *f = new char[file.length()];
        strcpy(f, file.c_str());
        char *c = new char[rd_cmd.length()];
        strcpy(c, rd_cmd.c_str());
        return new RedirectionCommand(cmd_line, c, f, flags);
    } else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    } else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    } else if (firstWord.compare("cd") == 0) {
        if (num_of_args > 2) {
            smashError::TooManyArguments("cd");
        } else {
            if (string(getPlastPwd()).length() > 0) {
                char *plast = getPlastPwd();
                return new ChangeDirCommand(cmd_line, &plast, args[1]);
            } else {
                return new ChangeDirCommand(cmd_line, nullptr, args[1]);
            }
        }

    } else if (firstWord.compare("chprompt") == 0) {
        if (num_of_args == 1) {
            setChprompt();
        } else {
            setChprompt(args[1]);
        }
        return nullptr;
    } else if (firstWord.compare("fg") == 0) {
        if (num_of_args == 1) {
            return new ForegroundCommand(cmd_line, &this->job_list, -1, true);
        } else if (num_of_args > 2 || (!(_isDigits(args[1])))) {
            smashError::InvalidArguments("fg");
        } else {
            return new ForegroundCommand(cmd_line, &this->job_list, atoi(args[1]));
        }
//        return new ForegroundCommand(cmd_line, &(this->job_list));
    } else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, &this->job_list);
    } else if (firstWord.compare("kill") == 0) {
        if (num_of_args != 3 || args[1][0] != '-' || !_isDigits(string(args[1]).substr(1)) || !_isDigits(args[2]) ||
            stoi(string(args[1]).substr(1)) < 1 || stoi(string(args[1]).substr(1)) > 31) {
            smashError::InvalidArguments("kill");
        } else {
            return new KillCommand(cmd_line, &this->job_list, string(args[2]), string(args[1]).substr(1));
        }
    } else if (firstWord.compare("bg") == 0) {
        if (num_of_args == 1) {
            return new BackgroundCommand(cmd_line, &this->job_list, -1, true);
        } else if (num_of_args > 2 || (!(_isDigits(args[1])))) {
            smashError::InvalidArguments("bg");
        } else {
            return new BackgroundCommand(cmd_line, &this->job_list, atoi(args[1]));
        }

    } else if (firstWord.compare("quit") == 0) {
        quitShell();
        if (num_of_args == 2 && strcmp(args[1], "kill") == 0) {
            return new QuitCommand(cmd_line, &this->job_list, true);
        } else {
            return new QuitCommand(cmd_line, &this->job_list);
        }
    } else if (firstWord.compare("tail") == 0) {
        if (num_of_args == 2) {
            return new TailCommand(cmd_line, args[1]);
        } else if (num_of_args == 3 && args[1][0] == '-' && _isDigits(string(args[1]).substr(1))) {
            return new TailCommand(cmd_line, args[2], stoi(string(args[1]).substr(1)));
        } else {
            smashError::InvalidArguments("tail");
        }
    } else if (firstWord.compare("touch") == 0) {
        if (num_of_args != 3) {
            smashError::InvalidArguments("touch");
        } else {
            return new TouchCommand(cmd_line, args[1], args[2]);
        }
    } else if (firstWord.compare("timeout") == 0) {
        if (num_of_args > 2 && _isDigits(args[1]) && args[1][0] != '-') {

            return new TimeoutCommand(cmd_line, atoi(args[1]));
        } else {
            smashError::InvalidArguments("timeout");
        }
    } else {
        return new ExternalCommand(cmd_line);
    }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    Command *cmd = CreateCommand(cmd_line);
    if (cmd == nullptr) {
        return;
    }

    if (typeid(*cmd) == typeid(ExternalCommand) || typeid(*cmd) == typeid(TimeoutCommand))
    {
        pid_t pid = fork();

        if (pid == -1) {
            smashError::ForkFailed();
        }
        if (pid == 0) {
            setpgrp();
            cmd->execute();
        } else {
            if (!cmd->bg_command) {
                cmd->setCmdPID(pid);
                if (typeid(*cmd) == typeid(TimeoutCommand)) {
                    this->addTimeoutCMD(dynamic_cast<TimeoutCommand *>(cmd));
                }
                setActiveCMD(cmd);
                setActivePID(pid);
                waitpid(getActivePID(), nullptr, WUNTRACED);
            } else {
                cmd->setCmdPID(pid);
                if (typeid(*cmd) == typeid(TimeoutCommand)) {
                    this->addTimeoutCMD(dynamic_cast<TimeoutCommand *>(cmd));
                }
                _removeBackgroundSign(reinterpret_cast<char *>(cmd));
                addJobShell(cmd);
            }

        }
    } else {
        cmd->execute();
    }
}


void GetCurrDirCommand::execute() {
    char buf[PATH_MAX];
    cout << getcwd(buf, PATH_MAX) << endl;
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << getpid() << endl;
}

void ChangeDirCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    char buf[PATH_MAX];
    char *actual_path = getcwd(buf, PATH_MAX);
    int res;
    if (this->path == "-") {
        if (plastPwd == nullptr) {
            smashError::PWDNotSet("cd");
            return;
        }
        res = chdir(*this->plastPwd);
    } else {
        res = chdir(path.c_str());
    }
    if (res != 0) {
        smashError::SyscallFailed("chdir");
        return;
    }
    smash.setPlastPwd(actual_path);
}

void ExternalCommand::execute() {
    if (this->bg_command) {
        execl("/bin/bash", "/bin/bash", "-c", bg_cmd, NULL);
    } else {
        execl("/bin/bash", "/bin/bash", "-c", cmd_line, NULL);
    }
}

void TimeoutCommand::execute() {
    execl("/bin/bash", "/bin/bash", "-c", this->actual_cmd, NULL);
}

void JobsCommand::execute() {
    job_list->printJobsList();
}

void KillCommand::execute() {
    JobsList::JobEntry *cmd_to_kill = job_list->getJobById(commandID);
    if (cmd_to_kill == nullptr) {
        smashError::NotExist(commandID, "kill");
        return;
    }
    int res = kill(cmd_to_kill->getJobCMD()->getCmdPID(), sig_num);
    if (res != 0) {
        smashError::SyscallFailed("kill");
    }
    if (sig_num == SIGSTOP) {
        cmd_to_kill->setStoppedStatus(true);
    }
    if (sig_num == SIGCONT) {
        cmd_to_kill->setStoppedStatus(false);
    }
    cout << "signal number " << sig_num << " was sent to pid " << cmd_to_kill->getJobCMD()->getCmdPID() << endl;
}

void ForegroundCommand::execute() {
    SmallShell &smash = SmallShell::getInstance();
    JobsList::JobEntry *cmd_to_move_to_fg = nullptr;
    int dummy;
    if (pick_last) {
        cmd_to_move_to_fg = jobs_list_ptr->getLastJob(&dummy);
        if (cmd_to_move_to_fg == nullptr) {
            smashError::EmptyJobList("fg");
            return;
        }
    } else {
        if (commandID < 1) {
            smashError::NotExist(commandID, "fg");
            return;
        }
        cmd_to_move_to_fg = jobs_list_ptr->getJobById(commandID);
        if (cmd_to_move_to_fg == nullptr) {
            smashError::NotExist(commandID, "fg");
            return;
        }
    }

    cout << cmd_to_move_to_fg->getJobCMD()->cmd_line << " : " << cmd_to_move_to_fg->getJobCMD()->getCmdPID() << endl;
    cmd_to_move_to_fg->setStoppedStatus(false);
    pid_t pid = fork();
    if (pid == -1) {
        smashError::ForkFailed();
    } else if (pid == 0) {
        setpgrp();
        kill(cmd_to_move_to_fg->getJobCMD()->getCmdPID(), SIGCONT);
        exit(0);
    } else {
        smash.setActiveCMD(cmd_to_move_to_fg->getJobCMD());
        smash.setActivePID(cmd_to_move_to_fg->getJobCMD()->getCmdPID());
        waitpid(smash.getActivePID(), nullptr, WUNTRACED);
        if (!cmd_to_move_to_fg->getStoppedStatus()) {
            this->jobs_list_ptr->removeJobById(cmd_to_move_to_fg->getJobID());
        }
    }
}

void BackgroundCommand::execute() {

    JobsList::JobEntry *cmd_to_bg;
    if (pick_last) {
        int dummy;
        cmd_to_bg = job_list->getLastStoppedJob(&dummy);
        if (cmd_to_bg == nullptr) {
            smashError::NoneStoppedJobs("bg");
            return;
        }
    } else {
        if (commandID < 1) {
            smashError::NotExist(commandID, "bg");
            return;
        }
        cmd_to_bg = job_list->getJobById(commandID);
        if (cmd_to_bg == nullptr) {
            smashError::NotExist(commandID, "bg");
            return;
        }
        if (!cmd_to_bg->getStoppedStatus()) {
            smashError::AlreadyRunning(commandID, "bg");
            return;
        }
    }
    cmd_to_bg->setStoppedStatus(false);
    cout << cmd_to_bg->getJobCMD()->getCmdLine();
    cout << " : " << cmd_to_bg->getJobCMD()->getCmdPID() << endl;
    kill(cmd_to_bg->getJobCMD()->getCmdPID(), SIGCONT);
}

void QuitCommand::execute() {
    if (this->shouldKill) {
        cout << "smash: sending SIGKILL signal to ";
        cout << this->job_list->jobs_list->size();
        cout << " jobs:" << endl;
        for (auto &job: *this->job_list->jobs_list) {
            kill(job->getJobCMD()->getCmdPID(), SIGKILL);
            cout << job->getJobCMD()->getCmdPID() << ": ";
            cout << job->getJobCMD()->getCmdLine() << endl;
        }
    }

}

void RedirectionCommand::execute() {
    prepare();
    if (file_fd == -1) {
        return;
    }
    SmallShell &small_shell = SmallShell::getInstance();
    small_shell.executeCommand(this->RCCmd.c_str());
    cleanup();
}

void RedirectionCommand::prepare() {
    saved_stdout_fd = dup(STDOUT_FILENO);
    file_fd = open((this->RCOutputFile).c_str(), this->flags, 0655);
    if (file_fd == -1) {
        smashError::SyscallFailed("open");
        return;
    }
    if (dup2(STDOUT_FILENO, saved_stdout_fd) == -1) {
        smashError::SyscallFailed("dup2");
        return;
    }
    if (dup2(file_fd, STDOUT_FILENO) == -1) {
        smashError::SyscallFailed("dup2");
        return;
    }
}

void RedirectionCommand::cleanup() {
    dup2(saved_stdout_fd, STDOUT_FILENO);
    if (close(file_fd) == -1) {
        smashError::SyscallFailed("close");
        return;
    }
}

void PipeCommand::execute() {
    SmallShell &small_shell = SmallShell::getInstance();
    int out = 1;
    if (err == true) {
        out = 2;
    }
    int fd[2];
    pipe(fd);
    pid_t pid1 = fork();
    if (pid1 == -1) {
        smashError::ForkFailed();
    } else if (pid1 == 0) {
        dup2(fd[1], out);
        close(fd[0]);
        close(fd[1]);
        small_shell.executeCommand(this->cmd1.c_str());
        exit(0);
    }
    pid_t pid2 = fork();
    if (pid2 == -1) {
        smashError::ForkFailed();
    } else if (pid2 == 0) {
        dup2(fd[0], 0);
        close(fd[0]);
        close(fd[1]);
        small_shell.executeCommand(this->cmd2.c_str());
        exit(0);
    }
    close(fd[0]);
    close(fd[1]);
    while ((waitpid(P_ALL, nullptr, 0)) != -1) {}
}

void TailCommand::execute() {
    int file_fd = open(file.c_str(), O_RDONLY);
    if (file_fd == -1) {
        smashError::SyscallFailed("open");
        return;
    }
    //   off_t file_offset = lseek(file_fd,0, SEEK_END);
//    int i =0;
//    char str[10];
//    while(read(file_fd, str, file_offset) != -1){
//
//    }
//
//    int len = file_offset;
    char buffer[1];
    char last_char;
    std::memset(buffer, 0, sizeof buffer);
    size_t count = 0;
    size_t count_line = 1;
    ssize_t res;


    while (true) {
        res = read(file_fd, &buffer, 1);
        if (res == -1) {
            smashError::SyscallFailed("read");
            break;
        } else if (res == 0) {
            break;
        } else {
            if (buffer[0] == '\n' || buffer[0] == '\r') {
                count_line += 1;
            }
            last_char = buffer[0];
            count += res;
        }
    }
    if (last_char == '\n') {
        count_line--;
    }
    if (count == 0) {
        close(file_fd);
        return;
    }
    lseek(file_fd, 0, SEEK_SET);
    size_t pos_to_start_from = 0;
    if (count_line >= n) {
        unsigned int i = 0;
        while (i < count_line - n) {
            res = read(file_fd, &buffer, 1);
            if (res == -1) {
                smashError::SyscallFailed("read");
                break;
            } else if (res == 0) {
                break;
            } else {
                if (buffer[0] == '\n') {
                    i += 1;
                }
                pos_to_start_from += res;
            }
        }
    }
//    res = read(file_fd, whole_file, count);
//    if(res == -1){
//        smashError::SyscallFailed("read");
//    }
//    cout << string(whole_file).rfind("\n",0) << endl;
//    const std::string str = string(whole_file);
//    size_t pos_to_start_from = (string(whole_file).rfind("\n", count ,size_t(n+1))) + 1;
//    cout << pos_to_start_from <<endl;
//    lseek(file_fd, pos_to_start_from, SEEK_SET);
    lseek(file_fd, pos_to_start_from, SEEK_SET);
    char *whole_file = new char[count - pos_to_start_from];
    res = read(file_fd, whole_file, count - pos_to_start_from);
    if (res == -1) {
        smashError::SyscallFailed("read");
    }
    write(STDOUT_FILENO, whole_file, count - pos_to_start_from);
    delete[] whole_file;
    close(file_fd);
}

void TouchCommand::execute() {
    tm *time_info = new tm();
    utimbuf ubuf;
    int array[6] = {0};
    std::string temp = ":" + this->time;
    for (int &i: array) {
        i = stoi(temp.substr(temp.find_last_of(':') + 1));
        temp = temp.substr(0, temp.find_last_of(':'));
    }
    time_info->tm_year = array[0] - 1900;
    time_info->tm_mon = array[1] - 1;
    time_info->tm_mday = array[2];
    time_info->tm_hour = array[3];
    time_info->tm_min = array[4];
    time_info->tm_sec = array[5];

    time_t time_to_update = mktime(time_info);
    ubuf.modtime = time_to_update;
    ubuf.actime = time_to_update;
    int res = utime(file.c_str(), &ubuf);
    if (res != 0) {
        smashError::SyscallFailed("utime");
    }

}


void JobsList::removeFinishedJobs() {
    pid_t zombie_pid;
    if (this->jobs_list == nullptr || jobs_list->size() == 0) {
        return;
    }
    int list_initial_size = jobs_list->size();
    auto iter = this->jobs_list->begin();
    while (iter != this->jobs_list->end()) {
        int status;
        zombie_pid = waitpid((*iter)->getJobCMD()->getCmdPID(), &status, WNOHANG);
        if (*iter != nullptr && (*iter)->getJobCMD()->getCmdPID() == zombie_pid) {
            this->jobs_list->erase(iter);
        } else {
            iter++;
        }
    }
}







