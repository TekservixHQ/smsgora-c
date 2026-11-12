/* Usage: SMSGORA_API_KEY=sgk_... ./smsgora_send +250788123456 "Hello from C" */
#include <smsgora/smsgora.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *key = getenv("SMSGORA_API_KEY");
    smsgora_client *c;
    smsgora_response r;
    smsgora_result res;
    const char *to[1];

    if (argc < 3 || !key) {
        fprintf(stderr, "usage: SMSGORA_API_KEY=sgk_... %s <number> <message>\n", argv[0]);
        return 2;
    }
    smsgora_global_init();
    c = smsgora_client_new(key);
    to[0] = argv[1];
    res = smsgora_send_sms(c, to, 1, argv[2], &r);
    printf("HTTP %ld\n%s\n", r.status, r.body ? r.body : r.error);
    smsgora_response_free(&r);
    smsgora_client_free(c);
    smsgora_global_cleanup();
    return res == SMSGORA_OK ? 0 : 1;
}
