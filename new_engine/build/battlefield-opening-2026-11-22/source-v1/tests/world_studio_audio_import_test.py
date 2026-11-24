#!/usr/bin/env python3
"""CPU-only checks of the exact production Audio-tab file methods.

The Studio include is normally compiled inside the renderer. Extract only its
three file methods into a small host with the same fields, avoiding a GPU/audio
device and avoiding a second copy of the importer implementation.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

SOURCE = Path(__file__).resolve().parents[1]
VIEW = SOURCE / "engine/world_studio_audio_view.inc"


def method(text, signature):
    start = text.index(signature)
    begin = text.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    text = VIEW.read_text()
    methods = "\n".join(method(text, signature) for signature in (
        "void sound_catalog_refresh()", "static bool sound_wav_supported(",
        "bool sound_import_file()"))
    harness = r'''
#include "engine/world_soundscape.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace fs=std::filesystem;
struct Importer {
 std::string project_directory,message,sound_clip;
 char sound_import_path[1024]{};
 std::vector<std::string> sound_catalog;
 bool sound_catalog_scanned=false,editable=true;
 bool sound_can_edit(){return editable;}
 PRODUCTION_METHODS
};
unsigned checks=0;
void require(bool ok,const char*label){++checks;if(!ok){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}}
void u16(std::vector<unsigned char>&v,unsigned n){v.push_back(n);v.push_back(n>>8);}
void u32(std::vector<unsigned char>&v,unsigned n){for(unsigned i=0;i<4;i++)v.push_back(n>>(8*i));}
void chars(std::vector<unsigned char>&v,const char*s){v.insert(v.end(),s,s+4);}
std::vector<unsigned char> wav(unsigned tag=1,unsigned channels=1,unsigned bits=16,unsigned bytes=128){
 std::vector<unsigned char>v;chars(v,"RIFF");u32(v,36+bytes);chars(v,"WAVE");chars(v,"fmt ");u32(v,16);u16(v,tag);u16(v,channels);u32(v,48000);u32(v,48000*channels*bits/8);u16(v,channels*bits/8);u16(v,bits);chars(v,"data");u32(v,bytes);v.resize(v.size()+bytes);return v;
}
void write(const fs::path&p,const std::vector<unsigned char>&v){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f.write((const char*)v.data(),v.size());}
std::vector<unsigned char> read(const fs::path&p){std::ifstream f(p,std::ios::binary);return{std::istreambuf_iterator<char>(f),{}};}
void source(Importer&i,const fs::path&p){std::snprintf(i.sound_import_path,sizeof i.sound_import_path,"%s",p.u8string().c_str());}
int main(int argc,char**argv){
 require(argc==2,"private fixture path supplied");fs::path root=fs::u8path(argv[1]);fs::create_directories(root);
 auto valid=wav();std::string error;
 require(Importer::sound_wav_supported(valid,error),"PCM16 mono imports");
 require(Importer::sound_wav_supported(wav(3,2,32),error),"float32 stereo imports");
 require(!Importer::sound_wav_supported(wav(1,1,8),error),"8-bit PCM rejected before copy");
 require(!Importer::sound_wav_supported(wav(1,3,16),error),"unsupported channels rejected");
 auto invalid=valid;invalid[0]='X';require(!Importer::sound_wav_supported(invalid,error),"invalid RIFF rejected");
 invalid=valid;invalid.resize(40);require(!Importer::sound_wav_supported(invalid,error),"truncated chunk rejected");
 invalid=wav(1,1,16,1);invalid.push_back(0);invalid[4]=38;require(!Importer::sound_wav_supported(invalid,error),"partial sample rejected");
 const auto external=root/fs::u8path("external/Forest é wind.wav");write(external,valid);
 Importer i;i.project_directory=(root/"project").u8string();fs::create_directories(fs::u8path(i.project_directory));source(i,external);
 require(i.sound_import_file(),"valid source copies to private project");
 require(i.sound_clip.rfind("sounds/imported/",0)==0&&world_soundscape::asset_path(i.sound_clip),"import remains portable project-relative WAV");
 const auto imported=fs::u8path(i.project_directory)/i.sound_clip;
 require(read(imported)==valid&&read(external)==valid,"copy exact, original preserved");
 require(i.sound_import_file(),"repeat import reuses identical file");
 require(std::distance(fs::directory_iterator(imported.parent_path()),fs::directory_iterator{})==1,"deduplicated import has one file");
 auto replaced=valid;replaced.back()=91;write(imported,replaced);
 require(!i.sound_import_file()&&read(imported)==replaced,"conflicting existing destination preserved");
 fs::remove(imported);fs::create_symlink(external,imported);
 require(!i.sound_import_file()&&fs::is_symlink(imported)&&read(external)==valid,"symbolic-link destination rejected intact");
 fs::remove(imported);
 i.editable=false;require(!i.sound_import_file(),"imports blocked during Play/freeze");i.editable=true;
 auto invalid_source=root/"external/invalid.wav";write(invalid_source,{1,2,3});source(i,invalid_source);require(!i.sound_import_file(),"invalid input does not enter project");
 auto large=root/"external/large.wav";write(large,valid);fs::resize_file(large,33u*1024u*1024u);source(i,large);require(!i.sound_import_file(),"oversized import rejected");source(i,external);
 for(const char*level:{"sounds","sounds/imported"}){
  const auto project=root/(std::string("symlink-")+(std::string(level)=="sounds"?"root":"nested"));fs::create_directories(project);const auto outside=root/(project.filename().string()+"-outside");fs::create_directory(outside);
  if(std::string(level)!="sounds")fs::create_directory(project/"sounds");fs::create_directory_symlink(outside,project/level);i.project_directory=project.u8string();
  require(!i.sound_import_file(),"symlink destination parent rejected");require(fs::is_empty(outside),"shared destination bank unchanged");
 }
 i.project_directory=(root/"not-directory").u8string();fs::create_directory(fs::u8path(i.project_directory));write(fs::u8path(i.project_directory)/"sounds",valid);require(!i.sound_import_file(),"regular-file destination parent rejected");
 const auto bank=root/"immutable-audio-bank";write(bank/"nested/FIRE.WAV",valid);i.project_directory=(root/"catalog").u8string();fs::create_directories(fs::u8path(i.project_directory));fs::create_directory_symlink(bank,fs::u8path(i.project_directory)/"audio");write(fs::u8path(i.project_directory)/"sounds/imported/stream.wav",valid);write(fs::u8path(i.project_directory)/"sounds/ignore.txt",valid);
 i.sound_catalog_refresh();require(i.sound_catalog.size()==2,"catalog discovers local and immutable linked WAV bank");require(std::find(i.sound_catalog.begin(),i.sound_catalog.end(),"audio/nested/FIRE.WAV")!=i.sound_catalog.end(),"catalog preserves uppercase WAV path");require(std::all_of(i.sound_catalog.begin(),i.sound_catalog.end(),world_soundscape::asset_path),"catalog contains only portable validated WAV paths");
 std::cout<<"PASS "<<checks<<" production Audio import/catalog checks\n";
}
'''.replace("PRODUCTION_METHODS", methods)
    cpp=output/"actual-import-methods.cpp"
    cpp.write_text(harness)
    binary=output/"actual-import-methods-test"
    command=["/usr/bin/x86_64-linux-gnu-g++-13","-std=c++17","-O1","-Wall","-Wextra","-I"+str(SOURCE),str(cpp),"-o",str(binary)]
    compiled=subprocess.run(command,text=True,capture_output=True)
    (output/"compile.log").write_text(compiled.stdout+compiled.stderr)
    compiled.check_returncode()
    with tempfile.TemporaryDirectory(prefix="valwood-audio-import-") as fixture:
        run=subprocess.run([str(binary),fixture],text=True,capture_output=True)
    (output/"run.log").write_text(run.stdout+run.stderr)
    print(run.stdout+run.stderr,end="")
    receipt={"passed":run.returncode==0,"scope":"CPU file validation/import/catalog; exact methods extracted from the renderer include; no GPU/audio/UI execution", "source":str(VIEW),"source_sha256":hashlib.sha256(VIEW.read_bytes()).hexdigest(),"test_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"command":command,"run_returncode":run.returncode,"stdout":run.stdout,"stderr":run.stderr}
    (output/"RESULTS.json").write_text(json.dumps(receipt,indent=2)+"\n")
    run.check_returncode()


if __name__=="__main__":main()
