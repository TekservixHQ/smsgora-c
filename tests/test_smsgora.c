/* Offline tests, plus one live call when SMSGORA_LIVE_TEST=true. */
#include <smsgora/smsgora.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(const char *name, int ok) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) failures++;
}

static void to_hex(const unsigned char *in, size_t n, char *out) {
    static const char h[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        out[i * 2] = h[in[i] >> 4];
        out[i * 2 + 1] = h[in[i] & 15];
    }
    out[n * 2] = '\0';
}

int main(void) {
    unsigned char mac[32];
    char hx[65];
    const char *body = "{\"event\":\"sms.received\"}";
    const char *good = "sha256=313fe9b54923f49c8b94d50c5b96fa44f3f5a7ca90ef52fd55afb2bfd3ae8f60";
    const char *live;
    char *js;

    /* RFC 4231 test case 2 */
    smsgora_hmac_sha256("Jefe", 4, "what do ya want for nothing?", 28, mac);
    to_hex(mac, 32, hx);
    check("HMAC-SHA256 RFC 4231 case 2",
          strcmp(hx, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843") == 0);
    /* RFC 4231 test case 6: key longer than the block size */
    {
        unsigned char key[131];
        const char *data = "Test Using Larger Than Block-Size Key - Hash Key First";
        memset(key, 0xaa, sizeof key);
        smsgora_hmac_sha256(key, sizeof key, data, strlen(data), mac);
        to_hex(mac, 32, hx);
        check("HMAC-SHA256 RFC 4231 case 6",
              strcmp(hx, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54") == 0);
    }

    check("accepts the right signature", smsgora_verify_webhook(body, strlen(body), good, "whsec_test") == 1);
    check("accepts surrounding whitespace",
          smsgora_verify_webhook(body, strlen(body),
                                 " sha256=313fe9b54923f49c8b94d50c5b96fa44f3f5a7ca90ef52fd55afb2bfd3ae8f60\n",
                                 "whsec_test") == 1);
    check("rejects a wrong secret", smsgora_verify_webhook(body, strlen(body), good, "other") == 0);
    check("rejects a changed body", smsgora_verify_webhook(body, strlen(body) - 1, good, "whsec_test") == 0);
    check("rejects an empty signature", smsgora_verify_webhook(body, strlen(body), "", "whsec_test") == 0);
    check("rejects a NULL signature", smsgora_verify_webhook(body, strlen(body), NULL, "whsec_test") == 0);

    js = smsgora_json_string("He said \"hi\"\n\\ \x01");
    check("JSON escaping", js && strcmp(js, "\"He said \\\"hi\\\"\\n\\\\ \\u0001\"") == 0);
    free(js);

    check("empty key is refused", smsgora_client_new("") == NULL);
    check("NULL key is refused", smsgora_client_new(NULL) == NULL);
    {
        smsgora_response r;
        smsgora_client *c = smsgora_client_new("sgk_x");
        check("client is created", c != NULL);
        check("send with no recipients is refused", smsgora_send_sms(c, NULL, 0, "hi", &r) == SMSGORA_ERR_ARGS);
        smsgora_client_free(c);
    }

    live = getenv("SMSGORA_LIVE_TEST");
    if (live && strcmp(live, "true") == 0) {
        smsgora_response r;
        smsgora_client *c;
        smsgora_result res;
        smsgora_global_init();
        c = smsgora_client_new("sgk_invalid_c_test");
        res = smsgora_get_account(c, &r);
        printf("     live: result=%d status=%ld error=%s\n", (int)res, r.status, r.error);
        check("live API rejects an invalid key with 401", res == SMSGORA_ERR_HTTP && r.status == 401);
        smsgora_response_free(&r);
        smsgora_client_free(c);
        smsgora_global_cleanup();
    } else {
        printf("skip live test (set SMSGORA_LIVE_TEST=true)\n");
    }

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
