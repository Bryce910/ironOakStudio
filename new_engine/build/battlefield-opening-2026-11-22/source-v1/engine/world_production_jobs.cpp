#include "engine/world_production_jobs.h"
#include "engine/world_platform.h"
#include <cerrno>
#include <cstring>
#include <filesystem>
#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <unistd.h>
extern char **environ;
namespace world_production {
namespace {
bool absolute(const std::string &s){return !s.empty()&&s.size()<4096&&s.find('\0')==std::string::npos&&std::filesystem::path(s).is_absolute();}
}
bool command(const Request &r,std::vector<std::string> &out,std::string &e){
    e.clear();if(!absolute(r.engine)){e="Select an absolute engine directory";return false;}
    const auto tool=(std::filesystem::path(r.engine)/"tools"/(r.task==Task::AuditFrames?"check_world_performance.py":"build_world_package.py")).string();
    std::vector<std::string> args={"python3",tool};
#ifdef _WIN32
    args[0]=vw_python_executable();
#endif
    switch(r.task){
    case Task::AuditFrames:
        if(!absolute(r.profile)||!absolute(r.output)){e="Choose an absolute frame profile and a new report filename";return false;}
        args.insert(args.end(),{r.profile,"--output",r.output});break;
    case Task::Validate:case Task::Package:
        if(!absolute(r.project)||!absolute(r.output)){e="Choose an absolute project and a new build destination";return false;}
        args.insert(args.end(),{"--project",r.project,"--engine-root",r.engine,"--output",r.output});
        if(r.task==Task::Validate)args.push_back("--validate-only");
        else if(r.bundle_libraries)args.push_back("--bundle-libs");
        break;
    default:e="Unknown production job";return false;
    }
    out=std::move(args);return true;
}
Result run(const Request &r){
    Result result;std::vector<std::string> arguments;if(!command(r,arguments,result.error))return result;
#ifdef _WIN32
    result.exit_code=vw_run_process(arguments,result.output,result.error);result.ok=result.exit_code==0;
    if(!result.ok&&result.error.empty())result.error="Production validation failed; see diagnostics";
    return result;
#else
    int stream[2];if(pipe(stream)!=0){result.error=std::strerror(errno);return result;}
    posix_spawn_file_actions_t actions;int status=posix_spawn_file_actions_init(&actions);
    if(status){close(stream[0]);close(stream[1]);result.error=std::strerror(status);return result;}
    status=posix_spawn_file_actions_adddup2(&actions,stream[1],STDOUT_FILENO);
    if(!status)status=posix_spawn_file_actions_adddup2(&actions,stream[1],STDERR_FILENO);
    if(!status)status=posix_spawn_file_actions_addclose(&actions,stream[0]);
    if(!status)status=posix_spawn_file_actions_addclose(&actions,stream[1]);
    std::vector<char *> argv;for(auto &a:arguments)argv.push_back(a.data());argv.push_back(nullptr);pid_t pid=0;
    if(!status)status=posix_spawnp(&pid,"python3",&actions,nullptr,argv.data(),environ);
    posix_spawn_file_actions_destroy(&actions);close(stream[1]);
    if(status){close(stream[0]);result.error=std::strerror(status);return result;}
    char buffer[4096];constexpr size_t limit=65536;bool truncated=false,read_failed=false;
    for(;;){ssize_t bytes=read(stream[0],buffer,sizeof buffer);if(bytes<0&&errno==EINTR)continue;if(bytes<0){read_failed=true;break;}if(!bytes)break;
        const auto n=std::min(size_t(bytes),limit-result.output.size());result.output.append(buffer,n);truncated|=n<size_t(bytes);
    }
    close(stream[0]);int child=0;while(waitpid(pid,&child,0)<0){if(errno==EINTR)continue;result.error=std::strerror(errno);return result;}
    if(truncated)result.output+="\n[Editor output limited to64KiB; worker output was fully drained.]\n";
    if(WIFEXITED(child))result.exit_code=WEXITSTATUS(child);
    else if(WIFSIGNALED(child)){result.exit_code=128+WTERMSIG(child);result.error="Production worker was terminated";}
    if(read_failed)result.error="Could not read complete production diagnostics";
    result.ok=result.exit_code==0&&!read_failed;
    if(!result.ok&&result.error.empty())result.error="Production validation failed; see diagnostics";
    return result;
#endif
}
}
