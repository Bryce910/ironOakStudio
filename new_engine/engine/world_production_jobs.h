#ifndef VALWOOD_WORLD_PRODUCTION_JOBS_H
#define VALWOOD_WORLD_PRODUCTION_JOBS_H
#include <string>
#include <vector>
namespace world_production {
enum class Task {Validate,Package,AuditFrames};
struct Request {
    Task task=Task::Validate;
    std::string engine,project,output,profile;
    bool bundle_libraries=false;
};
struct Result {bool ok=false;int exit_code=-1;std::string output,error;};
// Strict argv construction, no shell expansion. Worker-only blocking operation;
// the owner polls its future. Child output is drained even after64KiB so a
// verbose failing cook cannot deadlock or grow editor memory without bound.
bool command(const Request &,std::vector<std::string> &,std::string &error);
Result run(const Request &);
}
#endif
