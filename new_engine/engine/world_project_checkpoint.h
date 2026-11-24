#ifndef VALWOOD_WORLD_PROJECT_CHECKPOINT_H
#define VALWOOD_WORLD_PROJECT_CHECKPOINT_H
#include <string>
#include <vector>
namespace world_project {
struct File {std::string path,bytes;bool exists=true;};
using Files=std::vector<File>;
// Text authoring assets only: 128 files, 4 MiB each, 32 MiB combined.
// Relative confined paths, no symlinks. All operations are explicit boundaries
// or worker jobs; none belong in a frame tick. Model binaries are not copied.
bool validate(const Files &,std::string &);
bool read(const std::string &root,const std::vector<std::string> &,Files &,std::string &);
bool load(const std::string &root,const std::string &checkpoint,Files &,std::string &);
// Durable write-ahead journal; takes an exclusive project lock. expected is an
// optimistic concurrency guard captured before the UI starts a background job.
bool commit(const std::string &root,const std::string &checkpoint,const Files &after,
            const Files &expected,std::string &);
bool pending(const std::string &root,const std::string &checkpoint);
// Refuses an externally changed working file. A crash after any file rename
// leaves a replayable journal. Rollback also handles files absent before Save.
bool recover(const std::string &root,const std::string &checkpoint,bool finish,std::string &);
// Exposed staging boundary for recovery tools and fault-injection tests.
bool stage(const std::string &root,const std::string &checkpoint,const Files &after,
           const Files &expected,std::string &);
}
#endif
