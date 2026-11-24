#include "engine/world_editor_session.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <unistd.h>

using namespace world_editor;
using world_studio::Document;
namespace {
std::string bytes(const std::string &path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void write(const std::string &path,const std::string &text){std::ofstream f(path,std::ios::binary);f<<text;assert(f.good());}
struct Fixture {
    std::string directory,path,error;Document base;Config config{.2,1.,2};
    Fixture(){char temp[]="/tmp/valwood-editor-session-XXXXXX";const auto *p=mkdtemp(temp);assert(p);directory=p;path=directory+"/world.studio";base.source_signature="0123456789abcdef";assert(world_studio::save(path.c_str(),base,error));}
    ~Fixture(){std::filesystem::remove_all(directory);}
    void save(Session &s,Document &d,double now){assert(s.changed(now));assert(s.tick(now+.21,d,true,error));assert(s.wait(error));}
};
void restart_and_generations(){
    Fixture f;const auto original=bytes(f.path);Document draft=f.base;
    {
        Session s(f.config);assert(s.open(f.path,f.base,0,f.error));assert(s.probe(f.error)==Recovery::None);
        // Passive ticks, disabled work and active debounce do not touch disk.
        assert(s.tick(.1,draft,true,f.error));assert(!std::filesystem::exists(s.recovery_path()));
        draft.look.sun=1.1f;assert(s.changed(.2));assert(s.tick(.3,draft,true,f.error));assert(s.tick(.5,draft,false,f.error));assert(!std::filesystem::exists(s.recovery_path()));
        assert(s.tick(.6,draft,true,f.error));assert(s.wait(f.error));assert(s.stats().writes==1&&!s.stats().dirty);
        assert(bytes(f.path)==original);
        draft.look.sun=1.2f;f.save(s,draft,1);
        draft.look.sun=1.3f;f.save(s,draft,2);
        assert(s.stats().writes==3);assert(std::filesystem::exists(s.recovery_path()+".1"));assert(std::filesystem::exists(s.recovery_path()+".2"));assert(!std::filesystem::exists(s.recovery_path()+".3"));
        unsigned files=0;for(const auto &entry:std::filesystem::directory_iterator(f.directory)){(void)entry;files++;}assert(files==4);
    }
    Session restarted(f.config);assert(restarted.open(f.path,f.base,0,f.error));Document candidate=f.base;
    assert(restarted.probe(f.error)==Recovery::Available);assert(candidate.look.sun==1);assert(restarted.recover(candidate,1,f.error));assert(std::abs(candidate.look.sun-1.3)<1e-6);
    assert(bytes(f.path)==original);assert(restarted.probe(f.error,1)==Recovery::Available);assert(restarted.recover(candidate,2,f.error));assert(std::abs(candidate.look.sun-1.2)<1e-6);
    assert(restarted.probe(f.error,2)==Recovery::Available);assert(restarted.recover(candidate,3,f.error));assert(std::abs(candidate.look.sun-1.1)<1e-6);
    assert(restarted.discard(f.error));assert(!std::filesystem::exists(restarted.recovery_path()));assert(bytes(f.path)==original);
}
void continuous_edits_and_revision(){
    Fixture f;Session s(f.config);assert(s.open(f.path,f.base,0,f.error));Document draft=f.base;
    // Continued edits defeat debounce, but the maximum dirty interval wins.
    for(unsigned i=1;i<=9;i++){draft.look.wind=float(i)/10;assert(s.changed(i*.1));assert(s.tick(i*.1+.01,draft,true,f.error));assert(!s.stats().worker_running);}
    assert(s.changed(1.05));assert(s.tick(1.11,draft,true,f.error));assert(s.stats().worker_running);
    // A newer revision cannot be marked clean by a prior in-flight snapshot.
    draft.look.wind=1.2f;assert(s.changed(1.12));assert(s.wait(f.error));assert(s.stats().dirty);assert(s.stats().saved_revision<s.stats().revision);
    assert(s.tick(1.15,draft,true,f.error));assert(!s.stats().worker_running); // fresh maximum interval after an in-flight edit
    assert(s.tick(1.4,draft,true,f.error));assert(s.wait(f.error));assert(!s.stats().dirty&&s.stats().writes==2);
    const auto previous=bytes(s.recovery_path());draft.look.sun=std::numeric_limits<float>::quiet_NaN();assert(s.changed(2));assert(!s.tick(2.3,draft,true,f.error));assert(bytes(s.recovery_path())==previous);
    assert(!s.changed(1));assert(!s.changed(std::numeric_limits<double>::quiet_NaN()));
}
void corrupt_stale_and_interruption(){
    Fixture f;Session initial(f.config);assert(initial.open(f.path,f.base,0,f.error));Document draft=f.base;draft.look.sun=1.2f;f.save(initial,draft,1);draft.look.sun=1.4f;f.save(initial,draft,2);
    const auto valid=bytes(initial.recovery_path());const auto original=bytes(f.path);
    // A process killed before rename leaves a private temp, not a partial current.
    write(initial.recovery_path()+".tmp.abandoned","VALWOOD_EDITOR_RECOVERY 1\npartial");
    Session restart(f.config);assert(restart.open(f.path,f.base,0,f.error));assert(restart.probe(f.error)==Recovery::Available);
    Document candidate=f.base;assert(restart.recover(candidate,1,f.error));assert(std::abs(candidate.look.sun-1.4)<1e-6);
    auto damaged=valid;damaged.back()=damaged.back()=='\n'?'x':'\n';write(initial.recovery_path(),damaged);
    candidate=f.base;assert(restart.probe(f.error)==Recovery::Corrupt);assert(!restart.recover(candidate,2,f.error));assert(candidate.look.sun==1);
    assert(restart.probe(f.error,1)==Recovery::Available);assert(restart.recover(candidate,2,f.error));assert(std::abs(candidate.look.sun-1.2)<1e-6);
    write(initial.recovery_path(),valid);
    // A changed parsed/applied saved base rejects a previously valid recovery.
    Document wrong_base=f.base;wrong_base.look.sun=1.8f;Session wrong(f.config);assert(wrong.open(f.path,wrong_base,0,f.error));assert(wrong.probe(f.error)==Recovery::Stale);
    Document new_disk=f.base;new_disk.look.sun=1.5f;assert(world_studio::save(f.path.c_str(),new_disk,f.error));assert(restart.probe(f.error)==Recovery::Stale);assert(bytes(initial.recovery_path())==valid);
    write(f.path,original);assert(restart.probe(f.error)==Recovery::Available);
    // A successful Apply after inspection requires a fresh explicit inspection.
    assert(restart.applied(new_disk,f.error));candidate=f.base;assert(!restart.recover(candidate,3,f.error));assert(candidate.look.sun==1);
    assert(restart.probe(f.error)==Recovery::Available);assert(restart.recover(candidate,3,f.error));
    // Metadata/header truncation also fails without altering caller data.
    write(initial.recovery_path(),"VALWOOD_EDITOR_RECOVERY 1\n1 missing");assert(restart.probe(f.error)==Recovery::Corrupt);assert(bytes(f.path)==original);
}
void play_snapshots_and_save(){
    Fixture f;Session s(f.config);assert(s.open(f.path,f.base,0,f.error));Document draft=f.base,applied=f.base;
    draft.look.sun=1.1f;applied.look.sun=1.2f;assert(s.play_begin(draft,applied,f.error));assert(!s.save_allowed());assert(!s.changed(.2));
    assert(!s.clean_saved(draft,f.error));assert(!s.play_begin(draft,applied,f.error));
    draft.look.sun=std::numeric_limits<float>::quiet_NaN();applied.look.sun=1.9f;
    assert(s.tick(.5,draft,true,f.error));assert(!s.stats().worker_running);assert(!s.play_stop(StopChoice::DiscardPlayChanges,draft,applied,.6,f.error));
    assert(s.request_stop());assert(!s.request_stop());assert(!s.save_allowed());
    assert(!s.play_stop(StopChoice::KeepAuthoringChanges,draft,applied,.6,f.error));assert(s.stats().mode==Mode::StopPending);
    assert(s.play_stop(StopChoice::DiscardPlayChanges,draft,applied,.6,f.error));assert(s.save_allowed());assert(std::abs(draft.look.sun-1.1)<1e-6&&std::abs(applied.look.sun-1.2)<1e-6);
    assert(s.play_begin(draft,applied,f.error));draft.look.sun=1.7f;applied.look.sun=1.6f;assert(s.request_stop());assert(s.play_stop(StopChoice::KeepAuthoringChanges,draft,applied,1,f.error));assert(std::abs(draft.look.sun-1.7)<1e-6&&std::abs(applied.look.sun-1.6)<1e-6);assert(s.stats().dirty);
    assert(s.tick(1.3,draft,true,f.error));assert(s.wait(f.error));assert(std::filesystem::exists(s.recovery_path()));
    // Acknowledging a save without actually writing that document is rejected.
    assert(!s.clean_saved(draft,f.error));assert(std::filesystem::exists(s.recovery_path()));
    // Join any in-flight worker before cleanup; a stale result cannot resurrect.
    draft.look.sun=1.8f;assert(s.changed(2));assert(s.tick(2.3,draft,true,f.error));assert(world_studio::save(f.path.c_str(),draft,f.error));assert(s.clean_saved(draft,f.error));assert(!s.stats().dirty&&!s.stats().worker_running);assert(s.probe(f.error)==Recovery::None);
    for(unsigned i=0;i<=4;i++)assert(!std::filesystem::exists(s.recovery_path()+(i?"."+std::to_string(i):"")));
    Session restart(f.config);assert(restart.open(f.path,draft,0,f.error));assert(restart.probe(f.error)==Recovery::None);
}
void unsaved_apply_and_missing_project(){
    Fixture f;Session s(f.config);assert(s.open(f.path,f.base,0,f.error));Document a=f.base,d=f.base;a.look.sun=1.3f;d.look.sun=1.4f;assert(s.applied(a,f.error));f.save(s,d,1);
    // Last applied may be unsaved; its own payload hash is validated while the
    // restart anchor remains the saved document. Legitimate unsaved work survives.
    Session restart(f.config);assert(restart.open(f.path,f.base,0,f.error));assert(restart.probe(f.error)==Recovery::Available);Document recovered;assert(restart.recover(recovered,1,f.error));assert(std::abs(recovered.look.sun-1.4)<1e-6);
    const auto missing=f.directory+"/new.studio";Session fresh(f.config);assert(fresh.open(missing,f.base,0,f.error));f.save(fresh,d,1);Session new_restart(f.config);assert(new_restart.open(missing,f.base,0,f.error));assert(new_restart.probe(f.error)==Recovery::Available);assert(!std::filesystem::exists(missing));
}
void failed_publication_and_backoff(){
    Fixture f;Session s(f.config);assert(s.open(f.path,f.base,0,f.error));Document draft=f.base;draft.look.sun=1.1f;f.save(s,draft,1);
    const auto old=bytes(s.recovery_path());
    // Simulate a publication obstacle without any artificial production hooks.
    // Failure while retaining the previous generation must preserve current.
    assert(std::filesystem::create_directory(s.recovery_path()+".1"));
    draft.look.sun=1.3f;assert(s.changed(2));assert(s.tick(2.3,draft,true,f.error));assert(!s.wait(f.error));assert(bytes(s.recovery_path())==old);assert(s.stats().dirty&&s.stats().failures==1);
    std::filesystem::remove(s.recovery_path()+".1");
    assert(s.tick(2.4,draft,true,f.error));assert(!s.stats().worker_running); // bounded failure retry cadence
    assert(s.tick(3.4,draft,true,f.error));assert(s.wait(f.error));assert(s.stats().writes==2&&!s.stats().dirty);
    // External saved-file writes also prevent stale background publication.
    Document outside=f.base;outside.look.sun=1.9f;assert(world_studio::save(f.path.c_str(),outside,f.error));
    const auto before=bytes(s.recovery_path());draft.look.sun=1.4f;assert(s.changed(4));assert(s.tick(4.3,draft,true,f.error));assert(!s.wait(f.error));assert(bytes(s.recovery_path())==before);
    Session invalid(Config{0,0,5});assert(!invalid.open(f.path,f.base,0,f.error));
}
void explicit_latest_flush(){
    Fixture f;Session s(f.config);assert(s.open(f.path,f.base,0,f.error));Document draft=f.base;const auto original=bytes(f.path);
    // The latest edit is younger than debounce. Explicit close still leaves a
    // durable recoverable draft without changing clocks into the future.
    draft.look.sun=1.1f;assert(s.changed(.1));assert(s.tick(.11,draft,true,f.error));assert(!s.stats().worker_running);
    assert(s.flush_recovery(draft,.12,f.error));assert(!s.stats().worker_running&&!s.stats().dirty&&s.stats().writes==1&&bytes(f.path)==original);
    Session restart(f.config);assert(restart.open(f.path,f.base,0,f.error));assert(restart.probe(f.error)==Recovery::Available);Document recovered=f.base;assert(restart.recover(recovered,.13,f.error));assert(std::abs(recovered.look.sun-1.1)<1e-6);
    // A running older snapshot cannot overwrite the final changed revision.
    draft.look.sun=1.2f;assert(s.changed(1));assert(s.tick(1.21,draft,true,f.error));assert(s.stats().worker_running);
    draft.look.sun=1.4f;assert(s.changed(1.22));assert(s.flush_recovery(draft,1.23,f.error));assert(s.stats().writes==3&&s.stats().saved_revision==s.stats().revision&&!s.stats().dirty);
    Session newest(f.config);assert(newest.open(f.path,f.base,0,f.error));assert(newest.probe(f.error)==Recovery::Available);assert(newest.recover(recovered,1.24,f.error));assert(std::abs(recovered.look.sun-1.4)<1e-6);
    const auto valid=bytes(s.recovery_path());Document applied=f.base;
    assert(s.play_begin(draft,applied,f.error));assert(!s.flush_recovery(draft,2,f.error)&&bytes(s.recovery_path())==valid);assert(s.request_stop());assert(!s.flush_recovery(draft,2,f.error)&&bytes(s.recovery_path())==valid);
    assert(s.play_stop(StopChoice::DiscardPlayChanges,draft,applied,2,f.error));
    auto invalid=draft;invalid.look.sun=std::numeric_limits<float>::quiet_NaN();assert(!s.flush_recovery(invalid,2.01,f.error)&&bytes(s.recovery_path())==valid);
    assert(std::filesystem::remove(s.recovery_path()+".1"));assert(std::filesystem::create_directory(s.recovery_path()+".1"));draft.look.sun=1.6f;assert(s.changed(2.02));
    assert(!s.flush_recovery(draft,2.03,f.error)&&bytes(s.recovery_path())==valid&&s.stats().dirty&&bytes(f.path)==original);
    std::filesystem::remove(s.recovery_path()+".1");assert(s.flush_recovery(draft,2.04,f.error));assert(!s.stats().dirty&&!s.stats().worker_running&&bytes(f.path)==original);
    assert(!s.flush_recovery(draft,2.03,f.error));
}
}
int main(){restart_and_generations();continuous_edits_and_revision();corrupt_stale_and_interruption();play_snapshots_and_save();unsaved_apply_and_missing_project();failed_publication_and_backoff();explicit_latest_flush();std::puts("world_editor_session: restart/recovery, bounded generations/revisions, integrity/stale-base rejection, interrupted/failed publication and retry backoff, clean-save join, explicit latest-edit durable flush and Play rollback/Keep passed");}
