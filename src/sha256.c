/* SHA-256 (FIPS 180-4) and HMAC-SHA256 (RFC 2104), written for smsgora. MIT licensed. */
#include "sha256.h"

#include <string.h>

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static void transform(sg_sha256_ctx *ctx, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) | ((uint32_t)p[i * 4 + 2] << 8) |
               (uint32_t)p[i * 4 + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3];
    e = ctx->h[4]; f = ctx->h[5]; g = ctx->h[6]; h = ctx->h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

void sg_sha256_init(sg_sha256_ctx *ctx) {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(ctx->h, init, sizeof init);
    ctx->len = 0;
    ctx->buf_len = 0;
}

void sg_sha256_update(sg_sha256_ctx *ctx, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    ctx->len += len;
    while (len > 0) {
        size_t n = 64 - ctx->buf_len;
        if (n > len) n = len;
        memcpy(ctx->buf + ctx->buf_len, p, n);
        ctx->buf_len += n;
        p += n;
        len -= n;
        if (ctx->buf_len == 64) {
            transform(ctx, ctx->buf);
            ctx->buf_len = 0;
        }
    }
}

void sg_sha256_final(sg_sha256_ctx *ctx, unsigned char out[32]) {
    uint64_t bits = ctx->len * 8;
    unsigned char pad = 0x80, zero = 0, lenbuf[8];
    int i;
    sg_sha256_update(ctx, &pad, 1);
    while (ctx->buf_len != 56) sg_sha256_update(ctx, &zero, 1);
    for (i = 0; i < 8; i++) lenbuf[i] = (unsigned char)(bits >> (56 - 8 * i));
    sg_sha256_update(ctx, lenbuf, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)ctx->h[i];
    }
}

void sg_hmac_sha256(const void *key, size_t key_len, const void *data, size_t data_len, unsigned char out[32]) {
    unsigned char k[64], ipad[64], opad[64], inner[32];
    sg_sha256_ctx ctx;
    int i;
    memset(k, 0, sizeof k);
    if (key_len > 64) {
        sg_sha256_init(&ctx);
        sg_sha256_update(&ctx, key, key_len);
        sg_sha256_final(&ctx, k);
    } else if (key_len > 0) {
        memcpy(k, key, key_len);
    }
    for (i = 0; i < 64; i++) {
        ipad[i] = (unsigned char)(k[i] ^ 0x36);
        opad[i] = (unsigned char)(k[i] ^ 0x5c);
    }
    sg_sha256_init(&ctx);
    sg_sha256_update(&ctx, ipad, 64);
    sg_sha256_update(&ctx, data, data_len);
    sg_sha256_final(&ctx, inner);
    sg_sha256_init(&ctx);
    sg_sha256_update(&ctx, opad, 64);
    sg_sha256_update(&ctx, inner, 32);
    sg_sha256_final(&ctx, out);
}
