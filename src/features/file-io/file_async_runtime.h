#ifndef QYNO_FILE_ASYNC_RUNTIME_H
#define QYNO_FILE_ASYNC_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
enum { FA_READ_TEXT,FA_WRITE_TEXT,FA_APPEND_TEXT,FA_READ_LINES,FA_WRITE_LINES,
       FA_READ_BYTES,FA_WRITE_BYTES,FA_APPEND_BYTES };
/* Only native addresses enter these structures. Never retain a GC pointer. */
enum { FA_CREATE_THREAD,FA_CREATE_EVENT,FA_SET_EVENT,FA_WAIT,FA_CLOSE,
 FA_ALLOC,FA_FREE,FA_ERROR,FA_OPEN,FA_READ,FA_WRITE,FA_KIND,FA_INFO,
 FA_FULL_PATH,FA_IMPORT_COUNT };
typedef struct FileAsyncPool FileAsyncPool;
typedef struct FileAsyncWorker FileAsyncWorker;
typedef struct FileAsyncJob FileAsyncJob;
struct FileAsyncJob {
 FileAsyncPool *pool;FileAsyncWorker *worker;
 uint64_t id;uint32_t cancel,phase;
 unsigned char *path,*data;uint64_t path_size,capacity,length,limit;
 uint64_t mode,step,error,code,close_error,fatal;
};
FileAsyncPool *file_async_pool(uint64_t windows,const uint64_t *imports,uint64_t *error);
uint64_t file_async_pool_error(FileAsyncPool *p);
FileAsyncJob *file_async_job(FileAsyncPool *p,const unsigned char *path,uint64_t length,uint64_t mode,uint64_t limit);
int file_async_reserve(FileAsyncJob *j,uint64_t bytes);
void file_async_launch(FileAsyncJob *j);
void file_async_cancel(FileAsyncJob *j);
int file_async_status(FileAsyncJob *j);
uint64_t file_async_slot(FileAsyncJob *j);
void file_async_release(FileAsyncJob *j);
uint32_t file_async_sequence(FileAsyncPool *p);
void file_async_wait(FileAsyncPool *p,uint32_t sequence,int64_t seconds,int64_t nanoseconds);
void file_async_stop(FileAsyncPool *p);
#endif
