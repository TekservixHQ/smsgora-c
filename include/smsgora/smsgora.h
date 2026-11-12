/*
 * smsgora - SMS Gora SMS Gateway client for C and C++, by Tekservix.
 *
 * Send and receive SMS through your own Android phones and SIM cards.
 * You need a Tekservix account with SMS Gora and an API key (it starts with
 * "sgk_"). Create one at https://tekservix.com/smsgora/
 * API reference: https://tekservix.com/smsgora/docs/
 *
 * Every call fills an smsgora_response with the HTTP status and the raw JSON
 * body, so you can parse it with any JSON library. Free it with
 * smsgora_response_free().
 *
 * MIT licensed.
 */
#ifndef SMSGORA_H
#define SMSGORA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(SMSGORA_SHARED)
#  ifdef SMSGORA_BUILDING
#    define SMSGORA_API __declspec(dllexport)
#  else
#    define SMSGORA_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define SMSGORA_API __attribute__((visibility("default")))
#else
#  define SMSGORA_API
#endif

#define SMSGORA_VERSION "1.0.0"
#define SMSGORA_DEFAULT_BASE_URL "https://tekservix.com/smsgora/api/v1"

/* Result codes. */
typedef enum smsgora_result {
    SMSGORA_OK = 0,          /* HTTP 2xx */
    SMSGORA_ERR_HTTP = 1,    /* the API answered with HTTP 4xx/5xx; see status and body */
    SMSGORA_ERR_NETWORK = 2, /* no answer (DNS, TLS, timeout...); see error */
    SMSGORA_ERR_ARGS = 3,    /* invalid arguments */
    SMSGORA_ERR_MEMORY = 4   /* out of memory */
} smsgora_result;

typedef struct smsgora_response {
    long status;      /* HTTP status, 0 when there was no answer */
    char *body;       /* raw JSON body, NUL-terminated (may be NULL) */
    size_t body_len;
    char error[256];  /* human-readable error for non-OK results */
} smsgora_response;

typedef struct smsgora_client smsgora_client;

/* Call once at program start / end (wraps curl_global_init/cleanup). Optional
 * if your program already initialises libcurl. */
SMSGORA_API int smsgora_global_init(void);
SMSGORA_API void smsgora_global_cleanup(void);

/* Creates a client. Returns NULL if api_key is empty or memory runs out. */
SMSGORA_API smsgora_client *smsgora_client_new(const char *api_key);
/* Same with another base URL (for tests or a proxy). */
SMSGORA_API smsgora_client *smsgora_client_new_with_base(const char *api_key, const char *base_url);
/* Request timeout in seconds (default 30). */
SMSGORA_API void smsgora_client_set_timeout(smsgora_client *c, long seconds);
SMSGORA_API void smsgora_client_free(smsgora_client *c);

SMSGORA_API void smsgora_response_free(smsgora_response *r);

/* Calls any endpoint. path starts with "/", json_body may be NULL. */
SMSGORA_API smsgora_result smsgora_request(smsgora_client *c, const char *method, const char *path,
                                           const char *json_body, smsgora_response *out);

/* Optional settings for smsgora_send_sms_ex. Zero-initialise, then set:
 * device_id > 0 picks the phone, sim = 1 or 2 picks SIM 1 or SIM 2 (0 = default),
 * schedule_at = ISO 8601 time to send later (NULL = now). */
typedef struct smsgora_sms_options {
    long device_id;
    int sim;
    const char *schedule_at;
} smsgora_sms_options;

/* One text to up to 1,000 numbers, from your best online phone. */
SMSGORA_API smsgora_result smsgora_send_sms(smsgora_client *c, const char *const *to, size_t to_count,
                                            const char *message, smsgora_response *out);
/* Same, choosing the phone, SIM or a later time (opts may be NULL). */
SMSGORA_API smsgora_result smsgora_send_sms_ex(smsgora_client *c, const char *const *to, size_t to_count,
                                               const char *message, const smsgora_sms_options *opts,
                                               smsgora_response *out);
/* Different texts for different people from one phone (up to 5,000 recipients).
 * messages_json is a JSON array such as
 * [{"recipients":["+250788123456"],"message":"Hi"}]; template_message may be NULL. */
SMSGORA_API smsgora_result smsgora_send_bulk_sms(smsgora_client *c, long device_id, const char *messages_json,
                                                 const char *template_message, smsgora_response *out);

/* Messages. query is a URL query without "?" (e.g. "status=failed&limit=20") or NULL. */
SMSGORA_API smsgora_result smsgora_get_message(smsgora_client *c, long message_id, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_cancel_message(smsgora_client *c, long message_id, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_list_messages(smsgora_client *c, const char *query, smsgora_response *out);
/* Received SMS for one phone, or all phones when device_id <= 0. */
SMSGORA_API smsgora_result smsgora_list_received_sms(smsgora_client *c, long device_id, const char *query,
                                                     smsgora_response *out);
SMSGORA_API smsgora_result smsgora_get_batch(smsgora_client *c, long batch_id, smsgora_response *out);

/* Phones and account. */
SMSGORA_API smsgora_result smsgora_list_devices(smsgora_client *c, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_get_device(smsgora_client *c, long device_id, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_get_account(smsgora_client *c, smsgora_response *out);

/* Webhooks. device_id <= 0 means all phones. */
SMSGORA_API smsgora_result smsgora_list_webhooks(smsgora_client *c, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_create_webhook(smsgora_client *c, const char *url, const char *const *events,
                                                  size_t event_count, long device_id, smsgora_response *out);
SMSGORA_API smsgora_result smsgora_delete_webhook(smsgora_client *c, long webhook_id, smsgora_response *out);

/* Checks the X-SmsGora-Signature header ("sha256=" + hex HMAC-SHA256 of the raw
 * body, keyed with your webhook secret) in constant time. Returns 1 if valid. */
SMSGORA_API int smsgora_verify_webhook(const void *raw_body, size_t body_len, const char *signature,
                                       const char *secret);

/* Lower-level helpers, exposed for tests and advanced use. */
SMSGORA_API void smsgora_hmac_sha256(const void *key, size_t key_len, const void *data, size_t data_len,
                                     unsigned char out[32]);
/* Escapes s as a JSON string including the quotes. Caller frees. */
SMSGORA_API char *smsgora_json_string(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* SMSGORA_H */
