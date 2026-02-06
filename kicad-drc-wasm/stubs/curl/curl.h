#pragma once
// Minimal curl stub for WASM build - no network support

typedef void CURL;
typedef void CURLM;
typedef int CURLcode;
typedef int CURLMcode;
typedef int CURLoption;
typedef int CURLINFO;

#define CURLE_OK 0
#define CURLE_FAILED_INIT 2
#define CURLM_OK 0
#define CURLOPT_URL 10002
#define CURLOPT_WRITEFUNCTION 20011
#define CURLOPT_WRITEDATA 10001
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_FOLLOWLOCATION 52
#define CURLOPT_SSL_VERIFYPEER 64
#define CURLOPT_TIMEOUT 13
#define CURLOPT_NOBODY 44
#define CURLOPT_NOPROGRESS 43
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_POSTFIELDSIZE 60
#define CURLOPT_CUSTOMREQUEST 10036
#define CURLOPT_USERAGENT 10018
#define CURLOPT_XFERINFOFUNCTION 20219
#define CURLOPT_XFERINFODATA 10057
#define CURLOPT_ERRORBUFFER 10010
#define CURLINFO_RESPONSE_CODE 0x200002
#define CURLINFO_CONTENT_TYPE 0x100012
#define CURL_ERROR_SIZE 256

struct curl_slist;
inline struct curl_slist* curl_slist_append(struct curl_slist* list, const char* string) { return nullptr; }
inline void curl_slist_free_all(struct curl_slist* list) {}

inline CURL* curl_easy_init() { return nullptr; }
inline void curl_easy_cleanup(CURL* curl) {}
inline CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) { return CURLE_OK; }
inline CURLcode curl_easy_perform(CURL* curl) { return CURLE_FAILED_INIT; }
inline CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) { return CURLE_OK; }
inline const char* curl_easy_strerror(CURLcode code) { return "CURL not available in WASM"; }
inline CURLcode curl_global_init(long flags) { return CURLE_OK; }
inline void curl_global_cleanup() {}
inline char* curl_easy_escape(CURL* curl, const char* string, int length) { return nullptr; }
inline void curl_free(void* p) {}
inline const char* curl_version() { return "curl-stub/0.0 (WASM)"; }

typedef struct { const char* version; } curl_version_info_data;
#define CURLVERSION_NOW 0
inline curl_version_info_data* curl_version_info(int) {
    static curl_version_info_data info = { "stub" };
    return &info;
}
