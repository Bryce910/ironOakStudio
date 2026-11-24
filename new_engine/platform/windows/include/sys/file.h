#ifndef VALWOOD_WINDOWS_FLOCK_H
#define VALWOOD_WINDOWS_FLOCK_H
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
#ifdef __cplusplus
extern "C" {
#endif
int flock(int,int);
#ifdef __cplusplus
}
#endif
#endif
