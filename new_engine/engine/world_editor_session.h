#ifndef VALWOOD_WORLD_EDITOR_SESSION_H
#define VALWOOD_WORLD_EDITOR_SESSION_H

#include "engine/world_studio.h"
#include <cstdint>
#include <future>
#include <string>

// Owner-thread API. Only a single bounded autosave worker touches recovery files;
// the worker owns copies and never reads the live document or GL/runtime state.
namespace world_editor {
enum class Mode { Editing, Playing, StopPending };
enum class Recovery { None, Available, Stale, Corrupt, Busy };
enum class StopChoice { DiscardPlayChanges, KeepAuthoringChanges };
struct Config {
    double debounce_seconds=2,maximum_interval_seconds=30;
    unsigned backup_count=2; // bounded0..4; .recovery plus this many generations
};
struct Stats {
    std::uint64_t revision=0,saved_revision=0,writes=0,failures=0;
    bool dirty=false,worker_running=false;
    Mode mode=Mode::Editing;
    Recovery recovery=Recovery::None;
};
struct SaveResult {bool ok=false;std::uint64_t revision=0;std::string error;};

class Session {
public:
    explicit Session(Config config={});
    ~Session(); // joins a pending bounded worker; no project save is performed
    Session(const Session &)=delete;
    Session &operator=(const Session &)=delete;

    // Explicit startup/load event. Reads the saved file once and fingerprints
    // its parsed base. Never rewrites project_path, and never auto-recovers.
    bool open(const std::string &project_path,const world_studio::Document &saved_base,
              double now,std::string &error);
    // Completed authoring edit/checkpoint only: no copies, hashes or file IO.
    bool changed(double now);
    // Event after successful Apply. Keeps an immutable authoring checkpoint;
    // callers must never pass transient actor/physics state as a Document.
    bool applied(const world_studio::Document &,std::string &error);
    // Constant-time until due. allow_work=false during loading, profiling,
    // active manipulation or gameplay. At most one snapshot/worker at a time.
    bool tick(double now,const world_studio::Document &draft,bool allow_work,std::string &error);
    bool poll(std::string &error); // ready check only; never waits on work
    bool wait(std::string &error); // explicit Save/Load/shutdown boundary only
    // Explicit "keep recovery and close" boundary. Joins an older worker and
    // durably publishes this latest draft without debounce or a synthetic clock.
    // Never saves the project, and refuses Playing/StopPending transient state.
    bool flush_recovery(const world_studio::Document &,double now,std::string &error);

    // Explicit startup banner action; inspection cannot change any document.
    Recovery probe(std::string &error,unsigned generation=0);
    // Stages the recovered draft. Does NOT commit or load runtime resources.
    // Saved-base and applied-snapshot hashes are checked again before staging.
    bool recover(world_studio::Document &draft,double now,std::string &error);
    bool discard(std::string &error);
    // Only after an external explicit project save succeeds. Refuses Play.
    // Joins worker before removing all generations, preventing resurrection.
    bool clean_saved(const world_studio::Document &,std::string &error);

    bool play_begin(const world_studio::Document &draft,const world_studio::Document &applied,
                    std::string &error);
    bool request_stop(); // disables gameplay; choice remains pending
    bool play_stop(StopChoice,world_studio::Document &draft,world_studio::Document &applied,
                   double now,std::string &error);
    bool save_allowed() const {return opened_&&stats_.mode==Mode::Editing;}
    Stats stats() const {return stats_;}
    const std::string &recovery_path() const {return recovery_path_;}
private:
    Config config_;Stats stats_;
    bool opened_=false;double first_dirty_=0,last_change_=0,last_clock_=0,next_attempt_=0;
    std::string project_path_,recovery_path_,base_file_hash_,base_document_hash_;
    world_studio::Document applied_,play_draft_,play_applied_;
    std::future<SaveResult> worker_;
    std::uint64_t applied_revision_=0,probed_applied_revision_=0;
    unsigned recovery_generation_=0;
};
}
#endif
