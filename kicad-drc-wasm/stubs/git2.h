#pragma once
// git2.h stub for WASM build - libgit2 not available
// Project git utils will be non-functional

typedef struct git_repository git_repository;
typedef struct git_reference git_reference;
typedef struct git_remote git_remote;
typedef struct git_oid { unsigned char id[20]; } git_oid;
typedef struct git_commit git_commit;
typedef struct git_status_list git_status_list;
typedef struct git_status_entry { } git_status_entry;

#define GIT_DIFF_STATS_FULL 0

inline int git_libgit2_init(void) { return 0; }
inline int git_libgit2_shutdown(void) { return 0; }
inline int git_repository_open(git_repository**, const char*) { return -1; }
inline void git_repository_free(git_repository*) {}
inline int git_repository_head(git_reference**, git_repository*) { return -1; }
inline void git_reference_free(git_reference*) {}
inline const char* git_reference_shorthand(const git_reference*) { return ""; }
inline int git_reference_name_to_id(git_oid*, git_repository*, const char*) { return -1; }
inline int git_commit_lookup(git_commit**, git_repository*, const git_oid*) { return -1; }
inline void git_commit_free(git_commit*) {}
inline const char* git_commit_message(const git_commit*) { return ""; }
