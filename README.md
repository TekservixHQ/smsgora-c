# smsgora: SMS Gora SMS Gateway for C and C++

**SMS Gora SMS Gateway**: the official C client by Tekservix. Send and receive SMS through your own Android phones and SIM cards, with delivery reports and signed webhooks. Works from C and C++, depends only on libcurl.

**You need a Tekservix account with SMS Gora and at least one Android phone linked with the SMS Gora app.** Create an API key under SMS Gora → API keys (it starts with `sgk_`). [Create an account](https://tekservix.com/smsgora/) · [API docs](https://tekservix.com/smsgora/docs/)

## Install

With CMake (needs libcurl):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

Then in your project:

```cmake
find_package(smsgora 1.0 REQUIRED)
target_link_libraries(your_app PRIVATE smsgora::smsgora)
```

pkg-config is supported too (`pkg-config --cflags --libs smsgora`).

## Use

```c
#include <smsgora/smsgora.h>
#include <stdio.h>

int main(void) {
    smsgora_global_init();
    smsgora_client *c = smsgora_client_new("sgk_your_key");
    smsgora_response r;

    const char *to[] = {"+250788123456"};
    if (smsgora_send_sms(c, to, 1, "Your order #1042 has shipped.", &r) == SMSGORA_OK)
        printf("queued: %s\n", r.body);           /* raw JSON, parse with any library */
    else
        printf("failed (HTTP %ld): %s\n", r.status, r.body ? r.body : r.error);
    smsgora_response_free(&r);

    /* From a chosen phone and SIM, later */
    smsgora_sms_options o = {0};
    o.device_id = 12;
    o.sim = 2;                                    /* SIM 2 */
    o.schedule_at = "2026-12-01T08:00:00Z";
    smsgora_send_sms_ex(c, to, 1, "Reminder: appointment tomorrow", &o, &r);
    smsgora_response_free(&r);

    smsgora_get_message(c, 1873, &r);             /* delivery status */
    smsgora_response_free(&r);
    smsgora_list_messages(c, "status=failed&limit=20", &r);
    smsgora_response_free(&r);
    smsgora_list_received_sms(c, 12, "limit=10", &r);
    smsgora_response_free(&r);

    smsgora_client_free(c);
    smsgora_global_cleanup();
    return 0;
}
```

Every call returns `SMSGORA_OK` for HTTP 2xx, `SMSGORA_ERR_HTTP` when the API answered with an error (see `status` and `body`), or `SMSGORA_ERR_NETWORK` when there was no answer (see `error`).

Webhooks: `smsgora_verify_webhook(raw_body, body_len, signature_header, secret)` checks `X-SmsGora-Signature` in constant time.

A client may be used from one thread at a time; create one per thread if you send in parallel.

## More SMS Gora integrations

[npm](https://www.npmjs.com/package/smsgora) · [PyPI](https://pypi.org/project/smsgora/) · [Java/Kotlin](https://central.sonatype.com/artifact/com.tekservix/smsgora) · [All docs](https://tekservix.com/smsgora/docs/)

support@tekservix.com · MIT licensed.
