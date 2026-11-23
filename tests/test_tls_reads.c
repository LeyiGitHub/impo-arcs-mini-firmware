/* Execute plat_tls_read itself with deterministic mbedTLS outcomes and clock. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mbedtls/ssl.h"
static unsigned calls, delays;
static uint64_t now_ms;
static int outcomes[8];
static size_t outcomes_n;
static int fake_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    (void)ssl; (void)buf; (void)len;
    assert(calls < 50); /* A regression must fail, rather than hang the host test. */
    unsigned i = calls++;
    return outcomes[i < outcomes_n ? i : outcomes_n - 1];
}
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define mbedtls_ssl_read fake_ssl_read
#include "../upstream/arcs/platform/arcs/plat_tls.c"
#undef mbedtls_ssl_read

const char plat_ca_bundle_pem[] = "";
uint64_t plat_uptime_ms(void) { return now_ms; }
bool plat_time_synced(void) { return true; }
void vTaskDelay(unsigned ms) { now_ms += ms; delays += ms; }
void test_log(const char *tag, const char *fmt, ...) { (void)tag; (void)fmt; }
int closesocket(int fd) { (void)fd; return 0; }
int mbedtls_hardware_poll(void *d, unsigned char *o, size_t n, size_t *got)
{ (void)d; memset(o, 0, n); *got = n; return 0; }
int mbedtls_net_connect(mbedtls_net_context *c, const char *h, const char *p, int proto)
{ (void)c; (void)h; (void)p; (void)proto; return -1; }
void mbedtls_net_init(mbedtls_net_context *c) { c->fd = -1; }
void mbedtls_net_free(mbedtls_net_context *c) { (void)c; }
int mbedtls_net_set_nonblock(mbedtls_net_context *c) { (void)c; return 0; }
int mbedtls_net_send(void *c, const unsigned char *b, size_t n)
{ (void)c; (void)b; (void)n; return -1; }
int mbedtls_net_recv_timeout(void *c, unsigned char *b, size_t n, uint32_t ms)
{ (void)c; (void)b; (void)n; (void)ms; return -1; }

static void sequence(int a, int b, size_t n)
{ outcomes[0] = a; outcomes[1] = b; outcomes_n = n; calls = delays = 0; now_ms = 0; }
int main(void)
{
    plat_tls_t t = {0}; unsigned char buf[64];
    sequence(MBEDTLS_ERR_SSL_WANT_READ, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == PLAT_TLS_TIMEOUT);
    assert(calls == 1 && delays == 0);
    sequence(MBEDTLS_ERR_SSL_WANT_WRITE, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == PLAT_TLS_TIMEOUT);
    assert(calls == 1 && delays == 0);
    sequence(MBEDTLS_ERR_SSL_WANT_READ, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 25) == PLAT_TLS_TIMEOUT);
    assert(delays == 25 && calls == 3);
    sequence(MBEDTLS_ERR_SSL_WANT_READ, 12, 2);
    assert(plat_tls_read(&t, buf, sizeof(buf), 100) == 12);
    assert(calls == 2 && delays == 10);
    sequence(MBEDTLS_ERR_SSL_TIMEOUT, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 100) == PLAT_TLS_TIMEOUT && delays == 0);
    sequence(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == 0);
    sequence(-0x7400, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == -0x7400);
    puts("TLS reads: nonblocking WANT_READ/WANT_WRITE, deadline, data, timeout, close and errors passed");
}
