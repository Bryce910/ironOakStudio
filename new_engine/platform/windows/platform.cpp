#include "engine/world_platform.h"
#include <windows.h>
#include <bcrypt.h>
#include <io.h>
#include <direct.h>
#include <sys/file.h>
#include <algorithm>
#include <cstdarg>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

namespace {
std::wstring wide(const char *s){
    if(!s)return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,nullptr,0);
    if(!n){errno=EINVAL;return {};}std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out.data(),n);out.pop_back();return out;
}
std::string utf8(const wchar_t *s){
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,nullptr,0,nullptr,nullptr);
    if(!n)return {};std::string out(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,out.data(),n,nullptr,nullptr);out.pop_back();return out;
}
int failure(){switch(GetLastError()){
    case ERROR_FILE_NOT_FOUND:case ERROR_PATH_NOT_FOUND:errno=ENOENT;break;
    case ERROR_ALREADY_EXISTS:case ERROR_FILE_EXISTS:errno=EEXIST;break;
    case ERROR_SHARING_VIOLATION:case ERROR_LOCK_VIOLATION:errno=EWOULDBLOCK;break;
    default:errno=EACCES;break;
}return -1;}
bool ordinary_path(const std::wstring &path){
    // Reject junctions/reparse points in existing path components as well as
    // the leaf. The project layer separately validates relative containment.
    std::filesystem::path p(path),at;
    for(const auto &part:p){at/=part;auto name=at.native();if(name==L"Z:"||name.size()<=3)continue;
        DWORD a=GetFileAttributesW(name.c_str());if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT)){errno=ELOOP;return false;}}
    return true;
}
int open_file(const wchar_t *path,int flags){
    if(!path||!*path){errno=EINVAL;return -1;}
    if(flags&O_DIRECTORY){errno=ENOTSUP;return -1;}
    if((flags&O_NOFOLLOW)&&!ordinary_path(path))return -1;
    DWORD attrs=GetFileAttributesW(path);
    if(attrs==INVALID_FILE_ATTRIBUTES&&!(flags&O_CREAT))return failure();
    if(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){errno=ELOOP;return -1;}
    DWORD access=(flags&O_RDWR)?GENERIC_READ|GENERIC_WRITE:(flags&O_WRONLY)?GENERIC_WRITE:GENERIC_READ;
    DWORD creation=(flags&O_CREAT)?((flags&O_EXCL)?CREATE_NEW:(flags&O_TRUNC)?CREATE_ALWAYS:OPEN_ALWAYS):(flags&O_TRUNC)?TRUNCATE_EXISTING:OPEN_EXISTING;
    HANDLE h=CreateFileW(path,access,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,creation,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(h==INVALID_HANDLE_VALUE)return failure();
    BY_HANDLE_FILE_INFORMATION info{};
    if(GetFileType(h)!=FILE_TYPE_DISK||!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))){CloseHandle(h);errno=ELOOP;return -1;}
    int fd=_open_osfhandle(reinterpret_cast<intptr_t>(h),(flags&(_O_RDWR|_O_WRONLY|_O_APPEND))|_O_BINARY|_O_NOINHERIT);
    if(fd<0)CloseHandle(h);return fd;
}
bool random_suffix(char *pattern){
    const size_t n=std::strlen(pattern);if(n<6||std::strcmp(pattern+n-6,"XXXXXX")){errno=EINVAL;return false;}
    unsigned char bytes[6];if(BCryptGenRandom(nullptr,bytes,6,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0){errno=EIO;return false;}
    constexpr char chars[]="abcdefghijklmnopqrstuvwxyz0123456789";for(unsigned i=0;i<6;i++)pattern[n-6+i]=chars[bytes[i]%36];return true;
}
int replace_file(const wchar_t *from,const wchar_t *to){
    if(!ordinary_path(from)||!ordinary_path(to))return -1;
    if(MoveFileExW(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return 0;
    return failure();
}
std::wstring quoted(const std::string &arg){
    const auto text=wide(arg.c_str());std::wstring out=L"\"";unsigned slashes=0;
    for(wchar_t c:text){if(c==L'\\'){slashes++;continue;}if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}else{out.append(slashes,L'\\');out+=c;}slashes=0;}
    out.append(slashes*2,L'\\');out+=L'\"';return out;
}
}
extern "C" int vw_open(const char *path,int flags,...){auto p=wide(path);return open_file(p.c_str(),flags);}
int vw_open(const wchar_t *path,int flags,...){return open_file(path,flags);}
extern "C" int vw_replace(const char *from,const char *to){auto a=wide(from),b=wide(to);return replace_file(a.c_str(),b.c_str());}
int vw_replace(const char *from,const wchar_t *to){auto a=wide(from);return replace_file(a.c_str(),to);}
int vw_replace(const wchar_t *from,const wchar_t *to){return replace_file(from,to);}
extern "C" int fsync(int fd){return _commit(fd);}
extern "C" int mkstemp(char *pattern){
    std::string original=pattern;for(unsigned i=0;i<100;i++){std::strcpy(pattern,original.c_str());if(!random_suffix(pattern))return -1;
        int fd=vw_open(pattern,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd>=0||errno!=EEXIST)return fd;}errno=EEXIST;return -1;
}
extern "C" char *mkdtemp(char *pattern){
    std::string original=pattern;for(unsigned i=0;i<100;i++){std::strcpy(pattern,original.c_str());if(!random_suffix(pattern))return nullptr;
        auto p=wide(pattern);if(CreateDirectoryW(p.c_str(),nullptr))return pattern;if(GetLastError()!=ERROR_ALREADY_EXISTS){failure();return nullptr;}}
    errno=EEXIST;return nullptr;
}
extern "C" int lstat(const char *path,struct stat *out){
    auto p=wide(path);DWORD a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES)return failure();
    if(a&FILE_ATTRIBUTE_REPARSE_POINT){std::memset(out,0,sizeof(*out));out->st_mode=0xA000;return 0;}
    return stat(path,out);
}
extern "C" int link(const char *from,const char *to){auto a=wide(from),b=wide(to);if(!ordinary_path(a)||!ordinary_path(b))return -1;return CreateHardLinkW(b.c_str(),a.c_str(),nullptr)?0:failure();}
int link(const char *from,const wchar_t *to){auto a=wide(from);if(!ordinary_path(a)||!ordinary_path(to))return -1;return CreateHardLinkW(to,a.c_str(),nullptr)?0:failure();}
int unlink(const wchar_t *path){return DeleteFileW(path)?0:failure();}
extern "C" int vw_mkdir(const char *path,int){auto p=wide(path);return CreateDirectoryW(p.c_str(),nullptr)?0:failure();}
extern "C" char *realpath(const char *path,char *output){
    auto p=wide(path);std::vector<wchar_t> buffer(32768);if(!GetFullPathNameW(p.c_str(),DWORD(buffer.size()),buffer.data(),nullptr))return nullptr;
    auto s=utf8(buffer.data());std::replace(s.begin(),s.end(),'\\','/');if(s.size()>=PATH_MAX){errno=ENAMETOOLONG;return nullptr;}
    if(!output)output=static_cast<char *>(std::malloc(s.size()+1));if(output)std::memcpy(output,s.c_str(),s.size()+1);return output;
}
extern "C" int vw_directory_barrier(const char *path){
    auto p=wide(path);DWORD a=GetFileAttributesW(p.c_str());
    // Windows publication uses flushed files + MoveFileEx WRITE_THROUGH.
    // There is no POSIX directory-fsync contract; power-loss certification is
    // explicitly outside this port's process-interruption recovery checks.
    return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&ordinary_path(p)?0:-1;
}
extern "C" int flock(int fd,int operation){
    HANDLE h=reinterpret_cast<HANDLE>(_get_osfhandle(fd));OVERLAPPED o{};BOOL ok;
    if(operation&LOCK_UN)ok=UnlockFileEx(h,0,MAXDWORD,MAXDWORD,&o);
    else ok=LockFileEx(h,((operation&LOCK_EX)?LOCKFILE_EXCLUSIVE_LOCK:0)|((operation&LOCK_NB)?LOCKFILE_FAIL_IMMEDIATELY:0),0,MAXDWORD,MAXDWORD,&o);
    return ok?0:failure();
}
std::string vw_temp_pattern(const char *name){std::vector<wchar_t> buffer(32768);DWORD n=GetTempPathW(DWORD(buffer.size()),buffer.data());if(!n||n>=buffer.size())return {};return utf8(buffer.data())+name;}
std::string vw_python_executable(){
    std::vector<wchar_t> file(32768);DWORD n=GetModuleFileNameW(nullptr,file.data(),DWORD(file.size()));if(n&&n<file.size()){
        auto p=std::filesystem::path(file.data()).parent_path()/L"python"/L"python.exe";
        if(std::filesystem::is_regular_file(p))return utf8(p.c_str());}
    return "python";
}
int vw_run_process(const std::vector<std::string> &args,std::string &output,std::string &error){
    if(args.empty()){error="Missing worker command";return -1;}SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};HANDLE read=nullptr,write=nullptr;
    if(!CreatePipe(&read,&write,&security,0)||!SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)){if(read)CloseHandle(read);if(write)CloseHandle(write);error="Cannot create worker output pipe";return -1;}
    std::wstring command;for(const auto &a:args){if(!command.empty())command+=L' ';command+=quoted(a);}std::vector<wchar_t> mutable_command(command.begin(),command.end());mutable_command.push_back(0);
    STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESTDHANDLES;start.hStdOutput=start.hStdError=write;
    start.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION process{};auto executable=wide(args.front().c_str());
    BOOL created=CreateProcessW(executable.c_str(),mutable_command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&process);CloseHandle(write);
    if(!created){CloseHandle(read);error="Cannot launch bundled Python worker (Win32 "+std::to_string(GetLastError())+")";return -1;}
    char buffer[4096];DWORD got;bool truncated=false;while(ReadFile(read,buffer,sizeof(buffer),&got,nullptr)&&got){size_t n=std::min<size_t>(got,65536-output.size());output.append(buffer,n);truncated|=n<got;}
    CloseHandle(read);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    if(truncated)output+="\n[Worker output limited to 64 KiB; remaining output drained.]\n";return int(code);
}
