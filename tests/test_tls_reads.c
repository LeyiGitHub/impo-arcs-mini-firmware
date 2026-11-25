/* Execute plat_tls_read itself with deterministic mbedTLS outcomes and clock. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
static int fake_seed(mbedtls_ctr_drbg_context *ctx,
                     int (*entropy)(void *, unsigned char *, size_t), void *arg,
                     const unsigned char *custom, size_t len)
{ (void)ctx; (void)entropy; (void)arg; (void)custom; (void)len; return 0; }
static unsigned calls, delays;
static uint64_t now_ms;
static int outcomes[8];
static size_t outcomes_n;
static unsigned socket_option_calls;
static int net_send_result = -999;
static int (*net_send_hook)(void *, const unsigned char *, size_t);
static int socket_option_result;
static char dial_host[128], dial_port[8];
static int handshake_result;
static char log_output[8192];
static int fake_handshake(mbedtls_ssl_context *ssl)
{
    assert(ssl->conf->authmode == MBEDTLS_SSL_VERIFY_REQUIRED);
    /* 0.1.8 recv_timeout(0) waits forever: the session must use a poll BIO. */
    assert(ssl->f_recv == mbedtls_net_recv && ssl->f_recv_timeout == NULL);
    return handshake_result;
}
static bool real_read;
static bool real_write;
static unsigned write_calls;
static int write_result;
static const unsigned char *expected_write;
static size_t expected_write_len;
static int real_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{ return mbedtls_ssl_write(ssl, buf, len); }
static int fake_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{
    if (real_write) return real_ssl_write(ssl, buf, len);
    assert(buf == expected_write && len == expected_write_len);
    assert(++write_calls < 10000);
    return write_result;
}
static unsigned recv_calls;
static int real_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{ return mbedtls_ssl_read(ssl, buf, len); }
static int fake_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    if (real_read) return real_ssl_read(ssl, buf, len);
    (void)ssl; (void)buf; (void)len;
    assert(calls < 50); /* A regression must fail, rather than hang the host test. */
    unsigned i = calls++;
    return outcomes[i < outcomes_n ? i : outcomes_n - 1];
}
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define mbedtls_ssl_read fake_ssl_read
#define mbedtls_ssl_write fake_ssl_write
#define mbedtls_ssl_handshake fake_handshake
#define mbedtls_ctr_drbg_seed fake_seed
#include "../upstream/arcs/platform/arcs/plat_tls.c"
#undef mbedtls_ssl_read
#undef mbedtls_ssl_write
#undef mbedtls_ssl_handshake
#undef mbedtls_ctr_drbg_seed

uint64_t plat_uptime_ms(void) { return now_ms; }
bool plat_time_synced(void) { return true; }
void vTaskDelay(unsigned ms) { now_ms += ms; delays += ms; }
void test_log(const char *tag, const char *fmt, ...) {
    (void)tag;size_t used=strlen(log_output);va_list ap;va_start(ap,fmt);
    vsnprintf(log_output+used,sizeof(log_output)-used,fmt,ap);va_end(ap);
}
int closesocket(int fd) { (void)fd; return 0; }
int sal_setsockopt(int fd, int level, int option, const void *value, socklen_t size)
{
    assert(fd == 123 && level == IPPROTO_TCP && option == TCP_NODELAY);
    assert(size == sizeof(int) && *(const int *)value == 1);
    socket_option_calls++;
    return socket_option_result;
}
int mbedtls_hardware_poll(void *d, unsigned char *o, size_t n, size_t *got)
{ (void)d; memset(o, 0, n); *got = n; return 0; }
int mbedtls_net_connect(mbedtls_net_context *c, const char *h, const char *p, int proto)
{ assert(proto == MBEDTLS_NET_PROTO_TCP); c->fd=123;
  snprintf(dial_host,sizeof(dial_host),"%s",h);snprintf(dial_port,sizeof(dial_port),"%s",p);return 0; }
void mbedtls_net_init(mbedtls_net_context *c) { c->fd = -1; }
void mbedtls_net_free(mbedtls_net_context *c) { (void)c; }
int mbedtls_net_set_nonblock(mbedtls_net_context *c) { (void)c; return 0; }
int mbedtls_net_send(void *c, const unsigned char *b, size_t n)
{
    if (net_send_result != -999) return net_send_result;
    return net_send_hook ? net_send_hook(c,b,n) : (int)n;
}
int mbedtls_net_recv(void *c, unsigned char *b, size_t n)
{ (void)c; (void)b; (void)n; recv_calls++; return MBEDTLS_ERR_SSL_WANT_READ; }
int mbedtls_net_recv_timeout(void *c, unsigned char *b, size_t n, uint32_t ms)
{ (void)c; (void)b; (void)n; (void)ms; assert(!"timeout BIO must not be used"); return -1; }
static int test_rng(void *ctx, unsigned char *buf, size_t len)
{ (void)ctx; memset(buf, 1, len); return 0; }

static void sequence(int a, int b, size_t n)
{ outcomes[0] = a; outcomes[1] = b; outcomes_n = n; calls = delays = 0; now_ms = 0; }
#include "test_tls_wire.h"
int main(void)
{
    plat_tls_t t = {0}; unsigned char buf[64];
    t.net.fd = 123;
    tune_tcp(&t.net);
    assert(socket_option_calls == 1);
    socket_option_result = -1;
    tune_tcp(&t.net); /* An unavailable optimization does not close the connection. */
    assert(socket_option_calls == 2);
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
    assert(t.rx_bytes==12 && t.last_rx_ms==10);
    sequence(MBEDTLS_ERR_SSL_TIMEOUT, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 100) == PLAT_TLS_TIMEOUT && delays == 0);
    sequence(MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == 0);
    sequence(-0x7400, 0, 1);
    assert(plat_tls_read(&t, buf, sizeof(buf), 0) == -0x7400);
    unsigned char alert[] = {MBEDTLS_SSL_ALERT_LEVEL_FATAL, MBEDTLS_SSL_ALERT_MSG_INTERNAL_ERROR};
    t.ssl.in_msg=alert;t.ssl.in_msglen=2;t.ssl.in_msgtype=MBEDTLS_SSL_MSG_ALERT;
    sequence(MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE,0,1);now_ms=100;
    log_output[0]=0;
    assert(plat_tls_read(&t,buf,sizeof(buf),0)==MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
    assert(strstr(log_output,"level=2 description=80") && strstr(log_output,"rx_bytes=12"));
    alert[1]=MBEDTLS_SSL_ALERT_MSG_BAD_RECORD_MAC;log_output[0]=0;
    assert(plat_tls_read(&t,buf,sizeof(buf),0)==MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
    assert(strstr(log_output,"level=2 description=20"));
    t.ssl.in_msglen=1;log_output[0]=0;
    assert(plat_tls_read(&t,buf,sizeof(buf),0)==MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
    assert(strstr(log_output,"tls_peer_alert unavailable") && !strstr(log_output,"description="));
    /* Do not expose arbitrary application bytes when no alert record exists. */
    t.ssl.in_msg=(unsigned char *)"PRIVATE-TLS-PAYLOAD";t.ssl.in_msgtype=MBEDTLS_SSL_MSG_APPLICATION_DATA;
    t.ssl.in_msglen=19;log_output[0]=0;
    assert(plat_tls_read(&t,buf,sizeof(buf),0)==MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE);
    assert(!strstr(log_output,"PRIVATE-TLS-PAYLOAD"));
    /* Exercise the real TLS configuration while replacing the network handshake.
     * The socket peer and verified/SNI identity must stay independent. */
    int err = 0;
    plat_tls_t *named = plat_tls_connect_named("gadget.itoy.cn", 9443, "gw.itoy.cn", 5000, &err);
    assert(named && !err && !strcmp(dial_host,"gadget.itoy.cn") && !strcmp(dial_port,"9443"));
    assert(!strcmp(named->ssl.hostname,"gw.itoy.cn"));
    assert(named->ssl.f_send == tls_send);
    /* Let real mbedTLS start its handshake against a quiet peer. Both a poll
     * and a bounded read must yield through the configured socket BIO. */
    mbedtls_ssl_conf_rng(&named->conf, test_rng, NULL);
    real_read = true;
    now_ms = delays = recv_calls = 0;
    assert(plat_tls_read(named, buf, sizeof(buf), 0) == PLAT_TLS_TIMEOUT);
    assert(recv_calls > 0 && delays == 0);
    recv_calls = 0;
    assert(plat_tls_read(named, buf, sizeof(buf), 25) == PLAT_TLS_TIMEOUT);
    assert(recv_calls > 0 && delays == 25);
    real_read = false;
    /* Parse the firmware's real trust bundle, including the relay's new root. */
    bool relay_root_found = false;
    const unsigned char root_sha256[32] = {
        0xc3,0x2f,0xfd,0x9f,0x46,0xf9,0x36,0xd1,0x6c,0x36,0x73,0x99,0x09,0x59,0x43,0x4b,
        0x9a,0xd6,0x0a,0xaf,0xbb,0x9e,0x7c,0xf3,0x36,0x54,0xf1,0x44,0xcc,0x1b,0xa1,0x43
    };
    for (mbedtls_x509_crt *ca = &s_ca; ca && ca->raw.p; ca = ca->next) {
        unsigned char digest[32];
        assert(mbedtls_sha256_ret(ca->raw.p, ca->raw.len, digest, 0) == 0);
        if (!memcmp(digest, root_sha256, sizeof(digest))) relay_root_found = true;
    }
    assert(relay_root_found);
    plat_tls_close(named);
    named = plat_tls_connect("custom.example",443,5000,&err);
    assert(named && !strcmp(named->ssl.hostname,"custom.example"));plat_tls_close(named);
    handshake_result = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    assert(!plat_tls_connect_named("gadget.itoy.cn",9443,"gw.itoy.cn",5000,&err));
    assert(err == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
    expected_write = buf; expected_write_len = sizeof(buf);
    write_calls = delays = 0; write_result = 0;
    assert(plat_tls_write(&t, buf, sizeof(buf)) == MBEDTLS_ERR_NET_CONN_RESET);
    assert(write_calls == 1 && delays == 0);
    net_send_result = 0;
    assert(tls_send(&t.net, buf, sizeof(buf)) == MBEDTLS_ERR_NET_CONN_RESET);
    assert(tls_send(&t.net, buf, 0) == 0);
    net_send_result = MBEDTLS_ERR_SSL_WANT_WRITE;
    assert(tls_send(&t.net, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_WANT_WRITE);
    net_send_result = -999;
    write_result = MBEDTLS_ERR_SSL_WANT_WRITE; now_ms = delays = write_calls = 0;
    assert(plat_tls_write(&t, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_TIMEOUT);
    assert(delays <= WRITE_TIMEOUT_MS + POLL_MS && write_calls > 1);
    write_calls = 0; write_result = MBEDTLS_ERR_NET_SEND_FAILED;
    assert(plat_tls_write(&t, buf, sizeof(buf)) == MBEDTLS_ERR_NET_SEND_FAILED);
    test_tls_wire();
    puts("TLS route: separate peer/SNI, required certificate verification and failure propagation passed");
    puts("TLS reads: nonblocking WANT_READ/WANT_WRITE, deadline, data, timeout, close and errors passed");
    puts("TLS real receive BIO: quiet peer yields immediately and honors the caller deadline");
}
