#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <sys/wait.h>
#include <vector>
#include <unistd.h>

#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>
#include <iostream>
#include <sstream>
#include <ctime>
#include <utime.h>
#include <climits>
#include <cstring>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

#define YEARS_OFFSET    1900
#define MONTHS_OFFSET   1
#define FAILURE         (-1)
#define SUCCESS         0
#define MIN_JOB_ID      1
#define OPEN_FAILED     (-1)
#define PIPE_READ       0
#define PIPE_WRITE      1

inline void freeArgs(char** args,int num_of_args)
{
    for(int i = 0 ; i < num_of_args ; i++)
    {
        free(args[i]);
    }
    delete[] args;
}

inline bool isDigits(const std::string &str) {
    return (str.find_first_not_of("0123456789") == std::string::npos
            || (str.substr(0, 1).find_first_not_of("-123456789") == std::string::npos &&
                str.substr(1).find_first_not_of("0123456789") == std::string::npos));

}

class smashError {
public:
    static void TooManyArguments(const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "too many arguments";
        std::cerr << error_msg << std::endl;
    }

    static void PWDNotSet(const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "OLDPWD not set";
        std::cerr << error_msg << std::endl;
    }

    static void InvalidArguments(const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "invalid arguments";
        std::cerr << error_msg << std::endl;
    }

    static void NotExist(int jobID, const std::string& func) {
        std::string error_msg = "smash error: " + func +  ": " + "job-id " + std::to_string(jobID) + " does not exist"  ;
        std::cerr << error_msg << std::endl;
    }

    static void EmptyJobList(const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "jobs list is empty";
        std::cerr << error_msg << std::endl;
    }

    static void AlreadyRunning(int jobID, const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "job-id " + std::to_string(jobID);
        error_msg += " is already running in the background";
        std::cerr << error_msg << std::endl;
    }

    static void NoneStoppedJobs(const std::string& func) {
        std::string error_msg = "smash error: " + func + ": " + "there is no stopped jobs to resume";
        std::cerr << error_msg << std::endl;
    }

    static void ForkFailed() {
        std::string error_msg = "smash error: forked failed";
        perror(error_msg.c_str());
    }

    static void SyscallFailed(const std::string& syscall) {
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
    char *actual_cmd;
    bool error = false;

public:
    explicit Command(const char *cmd_line);

    virtual ~Command() =default;

    virtual void execute() = 0;

    pid_t getCmdPID() const {
        return cmd_pid;
    }

    const char *getCmdLine() const {
        return cmd_line;
    }
    void setError(){
        this->error = true;
    }
    bool getError() const{
        return this->error;
    }

    void setCmdPID(pid_t new_pid) {
        cmd_pid = new_pid;
    }
    virtual void prepare() { };
    virtual void cleanup() { };


};

class BuiltInCommand : public Command {
protected:
    char **args;
    int num_of_args;
public:
    explicit BuiltInCommand(const char *cmd_line);
    virtual ~BuiltInCommand()  =default;
};

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

    virtual ~ExternalCommand()  =default;

    void execute() override;
};

class PipeCommand : public Command {
    std::string cmd1;
    std::string cmd2;
    bool err;
public:
    PipeCommand(const char *cmd_line,std::string cmd1, std::string cmd2, bool err):
            Command(cmd_line),cmd1(cmd1),cmd2(cmd2),err(err){};

    virtual ~PipeCommand()=default;

    void execute() override;
};

class RedirectionCommand : public Command {
    std::string RCCmd;
    std::string RCOutputFile;
    int flags;
    int file_fd;
    int saved_stdout_fd;
public:
    RedirectionCommand(const char *cmd_line,std::string RCCmd, std::string RCOutputFile,int flags):Command(cmd_line),
                                                                                                   RCCmd(RCCmd),RCOutputFile(RCOutputFile), flags(flags), file_fd(-1) ,saved_stdout_fd(STDOUT_FILENO){}


    virtual ~RedirectionCommand()=default;

    void execute() override;
    void prepare() override;
    void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char **plastPwd;
    std::string path;
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line),plastPwd(plastPwd){
        if (this->num_of_args > 2) {
            smashError::TooManyArguments("cd");
            this->setError();
        } else {
            this->path= this->args[1];
        }
    }

    virtual ~ChangeDirCommand() =default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {};

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    JobsList *job_list;
    bool shouldKill = false;
public:
    QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line),job_list(jobs)
    {
        if (num_of_args >= 2 && strcmp(args[1], "kill") == 0) {
            this->shouldKill = true;
        }
    }

    virtual ~QuitCommand()= default;

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

        void setStoppedStatus(bool stop) {
            this->stopped = stop;
        }
        void setTimeInserted(){
            this->time_inserted= time(nullptr);
        }
        void setCMD(Command* command){
            this->cmd =command;
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

    void killAllJobs(){
        for (auto &job: *this->jobs_list) {
            if (kill(job->getJobCMD()->getCmdPID(), SIGKILL) !=SUCCESS){
                smashError::SyscallFailed("kill");
            }
            std::cout << job->getJobCMD()->getCmdPID() << ": ";
            std::cout << job->getJobCMD()->getCmdLine() << std::endl;
        }
    }

    void removeFinishedJobs() const {
        pid_t zombie_pid;
        if (this->jobs_list == nullptr || jobs_list->empty()) {
            return;
        }
        auto iter = this->jobs_list->begin();
        while (iter != this->jobs_list->end()) {
            zombie_pid = waitpid((*iter)->getJobCMD()->getCmdPID(), nullptr, WNOHANG);
            if (*iter != nullptr && (*iter)->getJobCMD()->getCmdPID() == zombie_pid) {
                this->jobs_list->erase(iter);
            } else {
                iter++;
            }
        }
    }

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

};

class JobsCommand : public BuiltInCommand {
    JobsList *job_list;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), job_list(jobs) {};

    virtual ~JobsCommand()=default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *job_list;
    int commandID;
    int sig_num;
public:
    KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), job_list(jobs) {
        if (num_of_args != 3 || args[1][0] != '-' || !isDigits(std::string(args[1]).substr(1)) || !isDigits(args[2]) ||
            stoi(std::string(args[1]).substr(1)) < 1 || stoi(std::string(args[1]).substr(1)) > 63) {
            smashError::InvalidArguments("kill");
            this->setError();
        } else {
            this->commandID = std::stoi(args[2]);
            this->sig_num = std::stoi(std::string(args[1]).substr(1));
        }
    }

    virtual ~KillCommand()=default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobs_list_ptr;
    int jobID;
    bool pick_last = false;
public:
    ForegroundCommand(const char *cmd_line, JobsList *job_list) : BuiltInCommand(cmd_line) ,jobs_list_ptr(job_list)
    {
        if (num_of_args == 1) {
            this->jobID= -1;
            this->pick_last= true;
        } else if (num_of_args > 2 || (!(isDigits(args[1])))) {
            smashError::InvalidArguments("fg");
            this->setError();
        } else {
            this->jobID= atoi(args[1]);
        }
    }

    virtual ~ForegroundCommand()=default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    JobsList *job_list;
    int jobID;
    bool pick_last;
public:
    BackgroundCommand(const char *cmd_line, JobsList *job_list) : BuiltInCommand(cmd_line),job_list(job_list)
    {
        if (num_of_args == 1) {
            this->jobID= -1;
            this->pick_last= true;
        } else if (num_of_args > 2 || (!(isDigits(args[1])))) {
            smashError::InvalidArguments("bg");
            this->setError();
        } else {
            jobID= atoi(args[1]);
        }
    }
    virtual ~BackgroundCommand()=default;
    void execute() override;
};

class TimeoutCommand : public BuiltInCommand {
    int timeout;
    time_t time_create = time(nullptr);
public:
    explicit TimeoutCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
    {
        if (num_of_args > 2 && isDigits(args[1]) && args[1][0] != '-') {

            this->timeout = atoi(args[1]);
        } else {
            smashError::InvalidArguments("timeout");
            this->setError();
        }
    }
    int getTimeOut(){
        return this->timeout;
    }
    time_t getTimeCreate(){
        return this->time_create;
    }
    pid_t getCmdPid(){
        return this->cmd_pid;
    }
    virtual ~TimeoutCommand()=default;
    void execute() override;
};

class TailCommand : public BuiltInCommand {
    std::string file;
    int n = 10;
public:
    explicit TailCommand(const char *cmd_line) : BuiltInCommand(cmd_line)
    {
        if (num_of_args == 2) {
            file = args[1];
        } else if (num_of_args == 3 && args[1][0] == '-' && isDigits(std::string(args[1]).substr(1))) {
            file = args[2],
                    n = stoi(std::string(args[1]).substr(1));
        } else {
            smashError::InvalidArguments("tail");
            this->setError();
        }
    }
    virtual ~TailCommand()=default;
    void execute() override;
};

class TouchCommand : public BuiltInCommand {
    std::string file;
    std::string time;
public:
    explicit TouchCommand(const char *cmd_line): BuiltInCommand(cmd_line)
    {
        if (num_of_args != 3)
        {
            smashError::InvalidArguments("touch");
            this->setError();
        }
        else
        {
            this->file = args[1];
            this->time = args[2];
        }
    }
    virtual ~TouchCommand()=default;

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



    Command *getActiveCMD() {
        return this->fg_command;
    }
    void setActiveCMD(Command* cmd) {
        this->fg_command = cmd;
    }

    void quitShell() {
        this->shellActive = false;
    }

    bool getActiveStatus() const {
        return this->shellActive;
    }

    char *getPlastPwd() {
        return this->plastPwd;
    }

    void setPlastPwd(char *plast_pwd) {
        strcpy(this->plastPwd,plast_pwd);
    }

    void removeJobByPID(pid_t pid){
        this->job_list.removeJobByPID(pid);
    }
    void addTimeoutCMD(TimeoutCommand* timout_cmd){
        this->timeout_list->push_back(timout_cmd);
        this->timeoutAlarm();
    }
    pid_t getTimeoutPid() const{
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
        if (this->timeout_list == nullptr || timeout_list->empty()) {
            return;
        }
        auto iter = this->timeout_list->begin();
        TimeoutCommand* to_alart = *iter;
        while (iter != this->timeout_list->end()) {
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
