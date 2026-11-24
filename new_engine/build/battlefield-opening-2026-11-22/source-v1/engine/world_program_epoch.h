#ifndef VALWOOD_WORLD_PROGRAM_EPOCH_H
#define VALWOOD_WORLD_PROGRAM_EPOCH_H
// Owner-thread shader replacement invalidates caches even when GL recycles IDs.
inline unsigned long long world_program_epoch=1;
#endif
