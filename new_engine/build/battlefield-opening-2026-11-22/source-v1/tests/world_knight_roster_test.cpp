#include "engine/world_knight_roster.h"
#include <cassert>
#include <cstdio>
#include <sstream>
using namespace world_knight_roster;
static std::string row(unsigned n){
    char id[20];std::snprintf(id,sizeof id,"knight%02u",n);
    return std::string("character ")+id+" \"Knight "+std::to_string(n)+"\" \"Contract fixture only\" \"portraits/"+id+".png\" \""+id+"/appearance.cfg\"\n";
}
static std::string catalog(){std::string text="VALWOOD_KNIGHT_ROSTER 1\ndefault knight09\n";for(unsigned n=1;n<=18;++n)text+=row(n);return text+"end\n";}
static bool parse(const std::string &text,Roster &out){std::istringstream input(text);std::string error;return read(input,out,error);}
static std::string bytes(const std::filesystem::path &path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
int main(){
    Roster roster;assert(parse(catalog(),roster)&&roster.entries.size()==18&&roster.default_id=="knight09");
    for(unsigned n=0;n<18;++n)assert(roster.index(roster.entries[n].id)==int(n));
    unsigned rejected=0;
    const std::vector<std::string> invalid={"", "VALWOOD_KNIGHT_ROSTER 2 end", "VALWOOD_KNIGHT_ROSTER 1 end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight09 end",catalog()+"extra",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01 default knight01\n"+row(1)+"end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\n"+row(1)+row(1)+"end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight19\n"+row(1)+"end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 Knight \"desc\" \"a.png\" \"a.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"desc\" \"../a.png\" \"a.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"desc\" \"/a.png\" \"a.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"desc\" \"a.png\" \"a//b.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"desc\" \"a.png\" \"a/./b.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"desc\" \"C:/a.png\" \"a.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\ncharacter knight01 \"Knight\" \"bad\ntext\" \"a.png\" \"a.cfg\" end",
        "VALWOOD_KNIGHT_ROSTER 1 default knight01\n"+row(1)};
    for(const auto &text:invalid){Roster prior=roster;assert(!parse(text,prior));assert(prior.default_id==roster.default_id&&prior.entries.size()==18&&prior.entries[17].id=="knight18");++rejected;}
    std::string too_many="VALWOOD_KNIGHT_ROSTER 1 default knight01\n";for(unsigned n=1;n<=33;++n)too_many+=row(n);too_many+="end";assert(!parse(too_many,roster));++rejected;
    assert(roster.entries.size()==18);
    for(const auto &entry:roster.entries){std::string chosen="knight09",error;std::istringstream input("VALWOOD_ARENA_PLAYER_CHARACTER 1 player "+entry.id+" end");assert(read_selection(input,roster,chosen,error)&&chosen==entry.id);}
    unsigned selection_rejected=0;
    for(const auto &text:{"", "VALWOOD_ARENA_PLAYER_CHARACTER 2 player knight01 end", "VALWOOD_ARENA_PLAYER_CHARACTER 1 player knight19 end",
                         "VALWOOD_ARENA_PLAYER_CHARACTER 1 player ../knight01 end", "VALWOOD_ARENA_PLAYER_CHARACTER 1 player knight01 end extra",
                         "VALWOOD_ARENA_PLAYER_CHARACTER 1 coins 100 player knight01 end", "VALWOOD_ARENA_PLAYER_CHARACTER 1 player knight01"}){
        std::string chosen="knight09",error;std::istringstream input(text);assert(!read_selection(input,roster,chosen,error)&&chosen=="knight09");++selection_rejected;
    }
    char pattern[]="/tmp/valwood-knight-choice-XXXXXX";const char *directory=mkdtemp(pattern);assert(directory);
    const std::filesystem::path root=directory,path=root/"arena_player_character.cfg",purse=root/"player.progress";
    std::ofstream(purse)<<"USER_PURSE_BYTES_MUST_NOT_CHANGE\n";const auto original_purse=bytes(purse);std::string chosen="knight09",error;
    assert(load_selection(path,roster,chosen,error)==SelectionLoad::Missing&&chosen=="knight09");
    assert(save_selection(path,roster,"knight18",error)&&load_selection(path,roster,chosen,error)==SelectionLoad::Loaded&&chosen=="knight18");
    const auto original=bytes(path);assert(!save_selection(path,roster,"knight19",error)&&bytes(path)==original);
    const auto occupied=root/"occupied";std::filesystem::create_directory(occupied);assert(!save_selection(occupied,roster,"knight01",error)&&std::filesystem::is_directory(occupied)&&bytes(path)==original);
    std::ofstream(path)<<"corrupt";chosen="knight18";assert(load_selection(path,roster,chosen,error)==SelectionLoad::Invalid&&chosen=="knight18"&&bytes(path)=="corrupt");
    assert(bytes(purse)==original_purse);
    for(const auto &file:std::filesystem::directory_iterator(root))assert(file.path().filename().string().find(".tmp-")==std::string::npos);
    std::filesystem::remove_all(root);
    std::printf("{\"passed\":true,\"fixture_entries\":18,\"malformed_rosters_rejected\":%u,\"selection_roundtrips\":18,\"malformed_selections_rejected\":%u,\"failed_operations_preserve_prior_state\":true,\"atomic_file_save_checked\":true,\"purse_bytes_unchanged\":true,\"original_asset_acceptance\":false}\n",rejected,selection_rejected);
}
