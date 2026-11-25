/* Real TLS 1.2/AES-GCM records through deliberately short nonblocking BIOs.
 * Test-only certificate and deterministic RNG; production trust stays covered
 * by test_tls_reads.c. No network, device credentials or request replay. */
#include "mbedtls/certs.h"
#include "mbedtls/ssl_internal.h"
#include "mbedtls/pk.h"

typedef struct { unsigned char data[32768]; size_t used; unsigned sends, wants; } wire_pipe;
typedef struct { wire_pipe *tx, *rx; } wire_end;
static int wire_send(void *arg, const unsigned char *buf, size_t len)
{
    wire_pipe *p=((wire_end *)arg)->tx;
    if (++p->sends % 7 == 0) { p->wants++; return MBEDTLS_ERR_SSL_WANT_WRITE; }
    size_t n=len > 113 ? 113 : len;
    assert(p->used+n <= sizeof(p->data));
    memcpy(p->data+p->used,buf,n);p->used+=n;return (int)n;
}
static int wire_recv(void *arg, unsigned char *buf, size_t len)
{
    wire_pipe *p=((wire_end *)arg)->rx;
    if (!p->used) return MBEDTLS_ERR_SSL_WANT_READ;
    size_t n=len < p->used ? len : p->used;
    if (n>71) n=71;
    memcpy(buf,p->data,n);p->used-=n;memmove(p->data,p->data+n,p->used);return (int)n;
}
static int wire_rng(void *arg, unsigned char *buf, size_t len)
{
    uint32_t *state=arg;
    for (size_t i=0;i<len;i++) { *state=*state*1664525u+1013904223u;buf[i]=(unsigned char)(*state>>24); }
    return 0;
}
static void test_tls_wire(void)
{
    plat_tls_t client={0};mbedtls_ssl_context server;mbedtls_ssl_config sc;
    mbedtls_x509_crt crt;mbedtls_pk_context key;
    wire_pipe cs={0},ss={0};wire_end ce={&cs,&ss},se={&ss,&cs};
    uint32_t cr=11,sr=29;
    mbedtls_ssl_init(&client.ssl);mbedtls_ssl_init(&server);
    mbedtls_ssl_config_init(&client.conf);mbedtls_ssl_config_init(&sc);
    mbedtls_x509_crt_init(&crt);mbedtls_pk_init(&key);
    assert(!mbedtls_x509_crt_parse(&crt,(const unsigned char *)mbedtls_test_srv_crt_ec,mbedtls_test_srv_crt_ec_len));
    assert(!mbedtls_pk_parse_key(&key,(const unsigned char *)mbedtls_test_srv_key_ec,mbedtls_test_srv_key_ec_len,NULL,0));
    assert(!mbedtls_ssl_config_defaults(&client.conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT));
    assert(!mbedtls_ssl_config_defaults(&sc,MBEDTLS_SSL_IS_SERVER,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT));
    static const int suites[]={MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,0};
    mbedtls_ssl_conf_ciphersuites(&client.conf,suites);mbedtls_ssl_conf_ciphersuites(&sc,suites);
    mbedtls_ssl_conf_authmode(&client.conf,MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&client.conf,wire_rng,&cr);mbedtls_ssl_conf_rng(&sc,wire_rng,&sr);
    assert(!mbedtls_ssl_conf_own_cert(&sc,&crt,&key));
    assert(!mbedtls_ssl_setup(&client.ssl,&client.conf));assert(!mbedtls_ssl_setup(&server,&sc));
    net_send_hook=wire_send;
    mbedtls_ssl_set_bio(&client.ssl,&ce,tls_send,wire_recv,NULL);
    mbedtls_ssl_set_bio(&server,&se,wire_send,wire_recv,NULL);
    for (unsigned step=0;client.ssl.state!=MBEDTLS_SSL_HANDSHAKE_OVER || server.state!=MBEDTLS_SSL_HANDSHAKE_OVER;step++) {
        assert(step<2000);
        if (client.ssl.state!=MBEDTLS_SSL_HANDSHAKE_OVER) {
            int r=mbedtls_ssl_handshake(&client.ssl);
            assert(!r || r==MBEDTLS_ERR_SSL_WANT_READ || r==MBEDTLS_ERR_SSL_WANT_WRITE);
        }
        if (server.state!=MBEDTLS_SSL_HANDSHAKE_OVER) {
            int r=mbedtls_ssl_handshake(&server);
            assert(!r || r==MBEDTLS_ERR_SSL_WANT_READ || r==MBEDTLS_ERR_SSL_WANT_WRITE);
        }
    }
    assert(strstr(mbedtls_ssl_get_ciphersuite(&client.ssl),"AES-128-GCM"));
    unsigned char payload[8192],received[8192];size_t total=0;
    const size_t sizes[]={8,44,512,2048,6144,8192};
    real_write=real_read=true;
    for (unsigned round=0;round<1024;round++) {
        size_t n=sizes[round%6];
        for (size_t i=0;i<n;i++) payload[i]=(unsigned char)(i*31+round);
        assert(plat_tls_write(&client,payload,n)==(int)n);
        size_t used=0;
        while (used<n) {
            int r=mbedtls_ssl_read(&server,received+used,n-used);assert(r>0);used+=(size_t)r;
        }
        assert(!memcmp(payload,received,n));total+=n;
        /* Echo a reply, then return to a quiet receive poll before next TX. */
        int r;do { r=mbedtls_ssl_write(&server,payload,32); }
        while (r==MBEDTLS_ERR_SSL_WANT_WRITE || r==MBEDTLS_ERR_SSL_WANT_READ);
        assert(r==32);used=0;
        while (used<32) { r=plat_tls_read(&client,received+used,32-used,0);assert(r>0);used+=(size_t)r; }
        assert(!memcmp(payload,received,32));
        assert(plat_tls_read(&client,received,sizeof(received),0)==PLAT_TLS_TIMEOUT);
    }
    assert(total>2800000 && cs.wants>1000 && ss.wants>100);
    assert(client.tx_bytes==total && client.rx_bytes==32768);
    /* Execute real ssl_write: without the BIO guard a zero send can report
     * success while leaving the ciphertext pending, losing the next payload. */
    assert(!cs.used);
    net_send_result=0;
    assert(plat_tls_write(&client,payload,32)==MBEDTLS_ERR_NET_CONN_RESET);
    assert(client.tx_bytes==total && !cs.used && client.ssl.out_left>0);
    net_send_result=-999;net_send_hook=NULL;
    real_write=real_read=false;
    mbedtls_ssl_free(&client.ssl);mbedtls_ssl_free(&server);
    mbedtls_ssl_config_free(&client.conf);mbedtls_ssl_config_free(&sc);
    mbedtls_x509_crt_free(&crt);mbedtls_pk_free(&key);
    puts("TLS wire: >2.8 MB AES-GCM, partial writes, WANT retries, RX polls and zero-send rejection passed");
}
