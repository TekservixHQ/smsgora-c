/* Internal SHA-256 / HMAC-SHA256. Not part of the public API. */
#ifndef SG_SHA256_H
#define SG_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct sg_sha256_ctx {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t buf_len;
} sg_sha256_ctx;

void sg_sha256_init(sg_sha256_ctx *ctx);
void sg_sha256_update(sg_sha256_ctx *ctx, const void *data, size_t len);
void sg_sha256_final(sg_sha256_ctx *ctx, unsigned char out[32]);
void sg_hmac_sha256(const void *key, size_t key_len, const void *data, size_t data_len, unsigned char out[32]);

#endif
