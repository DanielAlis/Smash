#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <sys/wait.h>
#include <vector>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)



class smashError {
public:
    static void TooManyArguments(std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "too many arguments";
        std::cerr << error_msg << std::endl;
    }

    static void PWDNotSet(std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "OLDPWD not set";
        std::cerr << error_msg << std::endl;
    }

    static void InvalidArguments(std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "invalid arguments";
        std::cerr << error_msg << std::endl;
    }

    static void NotExist(int jobID, std::string func) {
        std::string error_msg = "smash error: " + func +  ": " + "job-id " + std::to_string(jobID) + " does not exist"  ;
        std::cerr << error_msg << std::endl;
    }

    static void EmptyJobList(std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "jobs list is empty";
        std::cerr << error_msg << std::endl;
    }

    static void AlreadyRunning(int jobID, std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "job-id " + std::to_string(jobID);
        error_msg += " is already running in the background";
        std::cerr << error_msg << std::endl;
    }

    static void NoneStoppedJobs(std::string func) {
        std::string error_msg = "smash error: " + func + ": " + "there is no stopped jobs to resume";
        std::cerr << error_msg << std::endl;
    }

    static void ForkFailed() {
        std::string error_msg = "smash error: forked failed";
        perror(error_msg.c_str());
    }

    static void SyscallFailed(std::string syscall) {
        std::string error_msg = "smash error: " + syscall + " failed";
        perror(error_msg.c_str());
    }

};

class Command {
public:
    char *cmd_line;
    char *bg_cmd;
    pid_t cmd_pid;
    bool bg_command;
    bool is_timeout;
    char *actual_cmd;

public:
    Command(const char *cmd_line);

    virtual ~Command() {}

    virtual void execute() = 0;

    pid_t getCmdPID() {
        return cmd_pid;
    }

    const char *getCmdLine() {
        return cmd_line;
    }

    void setCmdPID(pid_t new_pid) {
        cmd_pid = new_pid;
    }
    virtual void prepare() { };
    virtual void cleanup() { };



    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~ExternalCommand() {}

    void execute() override;
};

class PipeCommand : public Command {
    std::string cmd1;
    std::string cmd2;
    bool err;
public:
    PipeCommand(const char *cmd_line,std::string cmd1, std::string cmd2, bool err):
            Command(cmd_line),cmd1(cmd1),cmd2(cmd2),err(err){};

    virtual ~PipeCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    std::string RCCmd;
    std::string RCOutputFile;
    int flags;
    int file_fd;
    int saved_stdout_fd;
public:
    RedirectionCommand(const char *cmd_line,std::string RCCmd, std::string RCOutputFile,int flags):Command(cmd_line),RCCmd(RCCmd),RCOutputFile(RCOutputFile), flags(flags),
                                                                                                   file_fd(-1) ,saved_stdout_fd(STDOUT_FILENO){}
//        this->RCCmd = RCCmd;
//        this->RCOutputFile = RCOutputFile;
//        this->flags = flags;
//        this->file_fd = -1;
//        this->saved_stdout_fd = STDOUT_FILENO;
//    }


    virtual ~RedirectionCommand() {}

    void execute() override;
    void prepare() override;
    void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char **plastPwd;
    std::string path;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd, std::string path) :
            BuiltInCommand(cmd_line), plastPwd(plastPwd), path(path) {}

    virtual ~ChangeDirCommand() {}

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() {}

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    JobsList *job_list;
    bool shouldKill;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs, bool shouldKill = false) : BuiltInCommand(cmd_line),
                                                                                 job_list(jobs),
                                                                                 shouldKill(shouldKill) {};

    virtual ~QuitCommand() {}

    void execute() override;
};


class JobsList {
public:
    class JobEntry {
        int jobID;

        Command *cmd;
        time_t time_inserted;
        bool stopped;

    public:
        JobEntry(int jobID, Command *cmd, time_t time_inserted, bool stopped) : jobID(jobID), cmd(cmd),
                                                                                time_inserted(time_inserted),
                                                                                stopped(stopped) {}

        int getJobID() {
            return this->jobID;
        }

        Command *getJobCMD() {
            return this->cmd;
        }

        bool getStoppedStatus() {
            return this->stopped;
        }

        void setStoppedStatus(bool stopped) {
            this->stopped = stopped;
        }
        void setTimeInserted(){
            this->time_inserted= time(nullptr);
        }
        void setCMD(Command* cmd){
            this->cmd =cmd;
        }

        friend std::ostream &operator<<(std::ostream &os, const JobEntry &jobEntry);
    };

    std::vector<JobEntry *> *jobs_list;
public:
    JobsList() : jobs_list(new std::vector<JobEntry *>()) {}

    ~JobsList() = default;

    void addJob(Command *cmd, bool isStopped = false) {
        int index = 1;
        if (!jobs_list->empty()) {
            index = jobs_list->back()->getJobID() + 1;
        }
        for(auto iter = jobs_list->begin(); iter != jobs_list->end() ; iter++)
        {
            if((*iter)->getJobCMD()->getCmdPID() == cmd->getCmdPID())
            {
                (*iter)->setTimeInserted();
                (*iter)->setStoppedStatus(isStopped);
                (*iter)->setCMD(cmd);
                return;
            }
        }
        JobEntry *new_job = new JobEntry(index, cmd, time(nullptr), isStopped);
        jobs_list->push_back(new_job);
    }

    void printJobsList() {
        for (auto &job: *this->jobs_list) {
            std::cout << *job;
        }
    }

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId) {
        for (auto &job: *this->jobs_list) {
            if (job->getJobID() == jobId) {
                return job;
            }
        }
        return nullptr;
    }

    void removeJobById(int jobId) {
        if (!jobs_list) {
            return;
        }
        size_t i = 0;
        for (auto iter = this->jobs_list->begin(); iter != this->jobs_list->end(); iter++) {
            if ((*iter)->getJobID() == jobId) {
                jobs_list->erase(iter);
                break;
            }
            i++;
        }
    }
    void removeJobByPID(pid_t pid) {
        if (!jobs_list) {
            return;
        }
        size_t i = 0;
        for (auto iter = this->jobs_list->begin(); iter != this->jobs_list->end(); iter++) {
            if ((*iter)->getJobCMD()->getCmdPID() == pid) {
                jobs_list->erase(iter);
                break;
            }
            i++;
        }
    }

    JobEntry *getLastJob(int *lastJobId) {
        if (!jobs_list) {
            return nullptr;
        }
        if (jobs_list->empty()) {
            return nullptr;
        }
        *lastJobId = jobs_list->at(jobs_list->size() - 1)->getJobID();
        return jobs_list->at(jobs_list->size() - 1);
    }

    JobEntry *getLastStoppedJob(int *jobId) {

        if (jobs_list->empty()) {
            return nullptr;
        }
        JobEntry *temp = nullptr;
        for (auto iter = this->jobs_list->begin(); *iter != nullptr && iter != this->jobs_list->end(); iter++) {
            if ((*iter)->getStoppedStatus() == true) {
                temp = (*iter);
            }
        }
        if (temp != nullptr) {
            *jobId = temp->getJobID();
        }
        return temp;
    }

    friend class SmallShell;
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    JobsList *job_list;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), job_list(jobs) {};

    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *job_list;
    int commandID;
    int sig_num;
public:
    KillCommand(const char *cmd_line, JobsList *jobs, std::string commandID, std::string sig_num) : BuiltInCommand(
            cmd_line), job_list(jobs) {
        this->commandID = std::stoi(commandID);
        this->sig_num = std::stoi(sig_num);
    }

    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobs_list_ptr;
    int commandID;
    bool pick_last;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs, int commandID, bool pick_last = false) : BuiltInCommand(cmd_line),
                                                                                                     jobs_list_ptr(jobs), commandID(commandID), pick_last(pick_last) {}

    virtual ~ForegroundCommand() {}

    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList *job_list;
    int commandID;
    bool pick_last;
public:
    BackgroundCommand(const char *cmd_line, JobsList *jobs, int commandID , bool pick_last = false) : BuiltInCommand(cmd_line),
                                                                                                      job_list(jobs),commandID(commandID),pick_last(pick_last) {}

    virtual ~BackgroundCommand() {}

    void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
    int timeout;
    time_t time_create;
public:
    TimeoutCommand(const char *cmd_line,int timeout) : BuiltInCommand(cmd_line),timeout(timeout),time_create(time(nullptr)){};
    int getTimeOut(){
        return this->timeout;
    }
    time_t getTimeCreate(){
        return this->time_create;
    }
    pid_t getCmdPid(){
        return this->cmd_pid;
    }
    virtual ~TimeoutCommand() {}

    void execute() override;
};

class TailCommand : public BuiltInCommand {
    std::string file;
    unsigned int n;
public:
    TailCommand(const char *cmd_line, std::string file, int n = 10) : BuiltInCommand(cmd_line),file(file),n(n){};

    virtual ~TailCommand() {}

    void execute() override;
};

class TouchCommand : public BuiltInCommand {
    std::string file;
    std::string time;
public:
    TouchCommand(const char *cmd_line,std::string file,std::string time): BuiltInCommand(cmd_line),file(file),time(time){};

    virtual ~TouchCommand() {}

    void execute() override;
};


class SmallShell {
private:
    std::string chprompt;
    char *plastPwd;
    JobsList job_list;
    Command *fg_command;
    bool shellActive;
    std::string cmdLine;
    std::vector<TimeoutCommand*>* timeout_list;
    pid_t timeout_pid;
    std::string timeout_cmd_line;
    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);

    void setChprompt() {
        this->chprompt = "smash> ";
    }

    void setChprompt(std::string chp) {
        this->chprompt = chp + "> ";
    }

    std::string getChprompt() {
        return this->chprompt;
    }

    void addJobShell(Command *cmd, bool isStopped = false) {
        job_list.addJob(cmd, isStopped);
    }

    pid_t getActivePID() {
        return this->fg_command->getCmdPID();
    }

    void setActivePID(pid_t pid) {
        this->fg_command->setCmdPID(pid);
    }

    Command *getActiveCMD() {
        return this->fg_command;
    }

    void setActiveCMD(Command *cmd) {
        this->fg_command = cmd;
    }

    void quitShell() {
        this->shellActive = false;
    }

    bool getActiveStatus() {
        return this->shellActive;
    }

    char *getPlastPwd() {
        return this->plastPwd;
    }

    void setPlastPwd(char *plast_pwd) {
        strcpy(this->plastPwd,plast_pwd);
    }

    std::string getCmdLine() {
        return this->cmdLine;
    }

    void setCmdLine(std::string cmd_line) {
        this->cmdLine = cmd_line;
    }
    void removeFinishedJobs(){
        this->job_list.removeFinishedJobs();
    }
    void removeJobByPID(pid_t pid){
        this->job_list.removeJobByPID(pid);
    }
    void addTimeoutCMD(TimeoutCommand* timout_cmd){
        this->timeout_list->push_back(timout_cmd);
        this->timeoutAlarm();
    }
    pid_t getTimeoutPid(){
        return this->timeout_pid;
    }
    std::string getTimeoutCmdLine(){
        return this->timeout_cmd_line;
    }
    void timeoutRemoveByPID(pid_t pid){
        for(auto iter = this->timeout_list->begin() ; iter != timeout_list->end() ; iter++)
        {
            if ((*iter)->getCmdPID() == pid){
                this->timeout_list->erase(iter);
                break;
            }
        }
        this->timeoutAlarm();
    }
    void timeoutAlarm()
    {
        pid_t zombie_pid;
        if (this->timeout_list == nullptr || timeout_list->empty()) {
            return;
        }
        auto iter = this->timeout_list->begin();
        TimeoutCommand* to_alart = *iter;
        while (iter != this->timeout_list->end()) {
            int status;
            int min_time = to_alart->getTimeOut() - difftime(time(nullptr), to_alart->getTimeCreate());
            int iter_time = (*iter)->getTimeOut() - difftime(time(nullptr), (*iter)->getTimeCreate());
            if(iter_time < min_time && iter_time >= 0) {
                to_alart = *iter;
            }
                iter++;
            }
        alarm(to_alart->getTimeOut() - difftime(time(nullptr), to_alart->getTimeCreate()));
        this->timeout_pid = to_alart->getCmdPID();
        this->timeout_cmd_line = to_alart->getCmdLine();
    }
};

//int res = kill((*iter)->getCmdPID(), SIGKILL);
//if( res!= 0){
//smashError::SyscallFailed("kill");
//return;
//}
//std::cout << "smash: " << (*iter)->getCmdLine()  << " timed out!" << std::endl;
//this->timeout_list->erase(iter);


inline std::ostream &operator<<(std::ostream &os, const JobsList::JobEntry &jobEntry) {
    os << "[" << jobEntry.jobID << "] ";
    os << jobEntry.cmd->getCmdLine() << " : ";
    os << jobEntry.cmd->getCmdPID() << " ";
    os << difftime(time(nullptr), jobEntry.time_inserted) << " secs";
    if (jobEntry.stopped) {
        os << " (stopped)";
    }
    os << std::endl;
    return os;
}


#endif //SMASH_COMMAND_H_
