#ifndef VALWOOD_WORLD_PLATFORM_H
#define VALWOOD_WORLD_PLATFORM_H
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <wchar.h>
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x10000000
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0x20000000
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0x40000000
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x08000000
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifdef __cplusplus
extern "C" {
#endif
int vw_open(const char *,int,...);
int vw_replace(const char *,const char *);
int mkstemp(char *);
char *mkdtemp(char *);
int fsync(int);
int lstat(const char *,struct stat *);
int link(const char *,const char *);
char *realpath(const char *,char *);
int vw_directory_barrier(const char *);
int vw_mkdir(const char *,int);
#ifdef __cplusplus
}
#include <string>
#include <vector>
int vw_open(const wchar_t *,int,...);
int vw_replace(const char *,const wchar_t *);
int vw_replace(const wchar_t *,const wchar_t *);
int link(const char *,const wchar_t *);
int unlink(const wchar_t *);
std::string vw_temp_pattern(const char *);
std::string vw_python_executable();
int vw_run_process(const std::vector<std::string> &,std::string &,std::string &);
#endif
#else
#define vw_open open
#define vw_replace rename
#define vw_mkdir mkdir
#ifdef __cplusplus
#include <string>
inline std::string vw_temp_pattern(const char *name){return std::string("/tmp/")+name;}
#endif
#endif
#endif
