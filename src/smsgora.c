/* smsgora - SMS Gora SMS Gateway client for C. MIT licensed. */
#define SMSGORA_BUILDING
#include "smsgora/smsgora.h"

#include <curl/curl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

struct smsgora_client {
    char *key;
    char *base;
    long timeout;
};

/* ---- small string builder ------------------------------------------------ */

typedef struct sb {
    char *p;
    size_t len, cap;
    int oom;
} sb;

static void sb_add(sb *b, const char *s, size_t n) {
    if (b->oom) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *np;
        while (b->len + n + 1 > cap) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) {
            b->oom = 1;
            return;
        }
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void sb_str(sb *b, const char *s) { sb_add(b, s, strlen(s)); }

static void sb_fmt(sb *b, const char *fmt, ...) {
    char tmp[64];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) sb_add(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

static void sb_json(sb *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    sb_add(b, "\"", 1);
    for (; *p; p++) {
        switch (*p) {
            case '"': sb_add(b, "\\\"", 2); break;
            case '\\': sb_add(b, "\\\\", 2); break;
            case '\n': sb_add(b, "\\n", 2); break;
            case '\r': sb_add(b, "\\r", 2); break;
            case '\t': sb_add(b, "\\t", 2); break;
            case '\b': sb_add(b, "\\b", 2); break;
            case '\f': sb_add(b, "\\f", 2); break;
            default:
                if (*p < 0x20) sb_fmt(b, "\\u%04x", *p);
                else sb_add(b, (const char *)p, 1);
        }
    }
    sb_add(b, "\"", 1);
}

static void sb_json_array(sb *b, const char *const *items, size_t n) {
    size_t i;
    sb_add(b, "[", 1);
    for (i = 0; i < n; i++) {
        if (i) sb_add(b, ",", 1);
        sb_json(b, items[i]);
    }
    sb_add(b, "]", 1);
}

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *d = (char *)malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static void set_error(smsgora_response *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
}

/* ---- client ---------------------------------------------------------------- */

int smsgora_global_init(void) { return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1; }

void smsgora_global_cleanup(void) { curl_global_cleanup(); }

smsgora_client *smsgora_client_new(const char *api_key) {
    return smsgora_client_new_with_base(api_key, SMSGORA_DEFAULT_BASE_URL);
}

smsgora_client *smsgora_client_new_with_base(const char *api_key, const char *base_url) {
    smsgora_client *c;
    size_t n;
    if (!api_key || !*api_key || !base_url || !*base_url) return NULL;
    c = (smsgora_client *)calloc(1, sizeof *c);
    if (!c) return NULL;
    c->key = dupstr(api_key);
    c->base = dupstr(base_url);
    c->timeout = 30;
    if (!c->key || !c->base) {
        smsgora_client_free(c);
        return NULL;
    }
    n = strlen(c->base);
    while (n > 0 && c->base[n - 1] == '/') c->base[--n] = '\0';
    return c;
}

void smsgora_client_set_timeout(smsgora_client *c, long seconds) {
    if (c && seconds > 0) c->timeout = seconds;
}

void smsgora_client_free(smsgora_client *c) {
    if (!c) return;
    if (c->key) {
        memset(c->key, 0, strlen(c->key));
        free(c->key);
    }
    free(c->base);
    free(c);
}

void smsgora_response_free(smsgora_response *r) {
    if (!r) return;
    free(r->body);
    r->body = NULL;
    r->body_len = 0;
}

static size_t on_body(char *data, size_t size, size_t nmemb, void *userp) {
    sb *b = (sb *)userp;
    size_t n = size * nmemb;
    sb_add(b, data, n);
    return b->oom ? 0 : n;
}

smsgora_result smsgora_request(smsgora_client *c, const char *method, const char *path, const char *json_body,
                               smsgora_response *out) {
    CURL *h;
    CURLcode rc;
    struct curl_slist *hdrs = NULL;
    sb url = {0}, key = {0}, body = {0};
    smsgora_result res;

    if (!out) return SMSGORA_ERR_ARGS;
    memset(out, 0, sizeof *out);
    if (!c || !method || !path) {
        set_error(out, "invalid arguments");
        return SMSGORA_ERR_ARGS;
    }
    h = curl_easy_init();
    if (!h) {
        set_error(out, "curl_easy_init failed");
        return SMSGORA_ERR_NETWORK;
    }
    sb_str(&url, c->base);
    sb_str(&url, path);
    sb_str(&key, "x-api-key: ");
    sb_str(&key, c->key);
    if (url.oom || key.oom) {
        res = SMSGORA_ERR_MEMORY;
        set_error(out, "out of memory");
        goto done;
    }
    hdrs = curl_slist_append(hdrs, key.p);
    hdrs = curl_slist_append(hdrs, "Accept: application/json");
    if (json_body) hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

    curl_easy_setopt(h, CURLOPT_URL, url.p);
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method);
    curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "smsgora-c/" SMSGORA_VERSION);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, c->timeout);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, on_body);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
    if (json_body) {
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, json_body);
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)strlen(json_body));
    }

    rc = curl_easy_perform(h);
    if (body.oom) {
        res = SMSGORA_ERR_MEMORY;
        set_error(out, "out of memory");
        goto done;
    }
    out->body = body.p;
    out->body_len = body.len;
    body.p = NULL;
    if (rc != CURLE_OK) {
        res = SMSGORA_ERR_NETWORK;
        set_error(out, "%s", curl_easy_strerror(rc));
        goto done;
    }
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &out->status);
    if (out->status >= 200 && out->status < 300) {
        res = SMSGORA_OK;
    } else {
        res = SMSGORA_ERR_HTTP;
        set_error(out, "SMS Gora API error %ld", out->status);
    }

done:
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(h);
    if (key.p) {
        memset(key.p, 0, key.len);
        free(key.p);
    }
    free(url.p);
    free(body.p);
    return res;
}

/* ---- helpers ---------------------------------------------------------------- */

static smsgora_result run(smsgora_client *c, const char *method, sb *path, sb *body, smsgora_response *out) {
    smsgora_result r;
    if ((path && path->oom) || (body && body->oom)) {
        if (out) {
            memset(out, 0, sizeof *out);
            set_error(out, "out of memory");
        }
        r = SMSGORA_ERR_MEMORY;
    } else {
        r = smsgora_request(c, method, path->p, body ? body->p : NULL, out);
    }
    free(path->p);
    if (body) free(body->p);
    return r;
}

static smsgora_result get_simple(smsgora_client *c, const char *fmt, long id, const char *query,
                                 smsgora_response *out) {
    sb path = {0};
    sb_fmt(&path, fmt, id);
    if (query && *query) {
        sb_add(&path, "?", 1);
        sb_str(&path, query);
    }
    return run(c, "GET", &path, NULL, out);
}

static smsgora_result args_error(smsgora_response *out) {
    if (out) {
        memset(out, 0, sizeof *out);
        set_error(out, "invalid arguments");
    }
    return SMSGORA_ERR_ARGS;
}

smsgora_result smsgora_send_sms(smsgora_client *c, const char *const *to, size_t to_count, const char *message,
                                smsgora_response *out) {
    return smsgora_send_sms_ex(c, to, to_count, message, NULL, out);
}

smsgora_result smsgora_send_sms_ex(smsgora_client *c, const char *const *to, size_t to_count, const char *message,
                                   const smsgora_sms_options *opts, smsgora_response *out) {
    sb path = {0}, body = {0};
    if (!to || to_count == 0 || !message) return args_error(out);
    if (opts && opts->device_id > 0) sb_fmt(&path, "/devices/%ld/send-sms", opts->device_id);
    else sb_str(&path, "/send-sms");
    sb_str(&body, "{\"recipients\":");
    sb_json_array(&body, to, to_count);
    sb_str(&body, ",\"message\":");
    sb_json(&body, message);
    if (opts && (opts->sim == 1 || opts->sim == 2)) sb_fmt(&body, ",\"sim_slot\":%d", opts->sim - 1);
    if (opts && opts->schedule_at) {
        sb_str(&body, ",\"schedule_at\":");
        sb_json(&body, opts->schedule_at);
    }
    sb_str(&body, "}");
    return run(c, "POST", &path, &body, out);
}

smsgora_result smsgora_send_bulk_sms(smsgora_client *c, long device_id, const char *messages_json,
                                     const char *template_message, smsgora_response *out) {
    sb path = {0}, body = {0};
    if (device_id <= 0 || !messages_json) return args_error(out);
    sb_fmt(&path, "/devices/%ld/send-bulk-sms", device_id);
    sb_str(&body, "{\"messages\":");
    sb_str(&body, messages_json);
    if (template_message) {
        sb_str(&body, ",\"messageTemplate\":");
        sb_json(&body, template_message);
    }
    sb_str(&body, "}");
    return run(c, "POST", &path, &body, out);
}

smsgora_result smsgora_get_message(smsgora_client *c, long id, smsgora_response *out) {
    return get_simple(c, "/messages/%ld", id, NULL, out);
}

smsgora_result smsgora_cancel_message(smsgora_client *c, long id, smsgora_response *out) {
    sb path = {0};
    sb_fmt(&path, "/messages/%ld/cancel", id);
    return run(c, "POST", &path, NULL, out);
}

smsgora_result smsgora_list_messages(smsgora_client *c, const char *query, smsgora_response *out) {
    sb path = {0};
    sb_str(&path, "/messages");
    if (query && *query) {
        sb_add(&path, "?", 1);
        sb_str(&path, query);
    }
    return run(c, "GET", &path, NULL, out);
}

smsgora_result smsgora_list_received_sms(smsgora_client *c, long device_id, const char *query,
                                         smsgora_response *out) {
    if (device_id > 0) return get_simple(c, "/devices/%ld/get-received-sms", device_id, query, out);
    {
        sb path = {0};
        sb_str(&path, "/received-sms");
        if (query && *query) {
            sb_add(&path, "?", 1);
            sb_str(&path, query);
        }
        return run(c, "GET", &path, NULL, out);
    }
}

smsgora_result smsgora_get_batch(smsgora_client *c, long id, smsgora_response *out) {
    return get_simple(c, "/batches/%ld", id, NULL, out);
}

smsgora_result smsgora_list_devices(smsgora_client *c, smsgora_response *out) {
    return smsgora_request(c, "GET", "/devices", NULL, out);
}

smsgora_result smsgora_get_device(smsgora_client *c, long id, smsgora_response *out) {
    return get_simple(c, "/devices/%ld", id, NULL, out);
}

smsgora_result smsgora_get_account(smsgora_client *c, smsgora_response *out) {
    return smsgora_request(c, "GET", "/account", NULL, out);
}

smsgora_result smsgora_list_webhooks(smsgora_client *c, smsgora_response *out) {
    return smsgora_request(c, "GET", "/webhooks", NULL, out);
}

smsgora_result smsgora_create_webhook(smsgora_client *c, const char *url, const char *const *events,
                                      size_t event_count, long device_id, smsgora_response *out) {
    sb path = {0}, body = {0};
    if (!url || !events || event_count == 0) return args_error(out);
    sb_str(&path, "/webhooks");
    sb_str(&body, "{\"url\":");
    sb_json(&body, url);
    sb_str(&body, ",\"events\":");
    sb_json_array(&body, events, event_count);
    if (device_id > 0) sb_fmt(&body, ",\"device_id\":%ld", device_id);
    sb_str(&body, "}");
    return run(c, "POST", &path, &body, out);
}

smsgora_result smsgora_delete_webhook(smsgora_client *c, long id, smsgora_response *out) {
    sb path = {0};
    sb_fmt(&path, "/webhooks/%ld", id);
    return run(c, "DELETE", &path, NULL, out);
}

/* ---- webhooks --------------------------------------------------------------- */

void smsgora_hmac_sha256(const void *key, size_t key_len, const void *data, size_t data_len,
                         unsigned char out[32]) {
    sg_hmac_sha256(key, key_len, data, data_len, out);
}

int smsgora_verify_webhook(const void *raw_body, size_t body_len, const char *signature, const char *secret) {
    static const char hex[] = "0123456789abcdef";
    unsigned char mac[32];
    char expected[7 + 64 + 1];
    const char *s, *e;
    size_t n, i;
    unsigned char diff = 0;
    if (!signature || !secret || (!raw_body && body_len)) return 0;
    sg_hmac_sha256(secret, strlen(secret), raw_body ? raw_body : "", body_len, mac);
    memcpy(expected, "sha256=", 7);
    for (i = 0; i < 32; i++) {
        expected[7 + i * 2] = hex[mac[i] >> 4];
        expected[7 + i * 2 + 1] = hex[mac[i] & 15];
    }
    expected[71] = '\0';
    /* trim surrounding whitespace */
    s = signature;
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    n = (size_t)(e - s);
    if (n != 71) return 0;
    for (i = 0; i < 71; i++) diff |= (unsigned char)(expected[i] ^ s[i]);
    return diff == 0;
}

char *smsgora_json_string(const char *s) {
    sb b = {0};
    sb_json(&b, s);
    if (b.oom) {
        free(b.p);
        return NULL;
    }
    return b.p;
}
