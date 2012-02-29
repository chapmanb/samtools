#ifndef WRITER_H_
#define WRITER_H_

typedef struct {
    BGZF *fp_bgzf;
    FILE *fp_file;
    queue_t *output;
    uint8_t compress;
    uint8_t is_done;
    uint8_t is_closed;
    block_pool_t *pool;
} writer_t;

#define WRITER_BLOCK_POOL_NUM 1000

writer_t*
writer_init(int fd, queue_t *output, uint8_t compress, int32_t compress_level, block_pool_t *pool);

void*
writer_run(void *arg);

void
writer_destroy(writer_t *r);

#endif
