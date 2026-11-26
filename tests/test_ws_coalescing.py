"""Run the production WebSocket writer with a deterministic TLS transport."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "upstream/esp32/main/noise_control.cpp"


class WebSocketWrites(unittest.TestCase):
    def test_masking_coalescing_and_failure_boundaries(self):
        source = SOURCE.read_text()
        writer = source[source.index("static ssize_t write_all_until("):source.index("static void make_uuid(")]
        binary = source[source.index("#ifndef NOISE_CTRL_WS_WRITE_BYTES"):source.index("// Send a zero-length masked WebSocket ping")]
        harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <sys/types.h>
struct esp_tls_t {};
static constexpr int ESP_TLS_ERR_SSL_WANT_READ=-0x6900, ESP_TLS_ERR_SSL_WANT_WRITE=-0x6880;
static constexpr int64_t WS_FRAME_TIMEOUT_US=15000000;
static bool s_running=true, partial, stall, allocation_fails;
static int64_t clock_us, per_call;
static unsigned calls, wants;
static int error;
static std::vector<uint8_t> wire;
static int64_t esp_timer_get_time(){return clock_us;}
static void vTaskDelay(unsigned n){clock_us+=n*1000;}
static uint32_t esp_random(){return 0x19436af7;}
[[maybe_unused]] static void *allocate(size_t n){return allocation_fails?nullptr:std::malloc(n);}
static ssize_t esp_tls_conn_write(esp_tls_t *,const void *data,size_t n){
    calls++;clock_us+=per_call;
    if(stall)return ESP_TLS_ERR_SSL_WANT_WRITE;
    if(wants){return --wants%2?ESP_TLS_ERR_SSL_WANT_READ:ESP_TLS_ERR_SSL_WANT_WRITE;}
    if(error)return error==1?0:error;
    size_t take=partial?std::min(n,size_t(7)):n;
    const auto *p=static_cast<const uint8_t *>(data);wire.insert(wire.end(),p,p+take);
    return static_cast<ssize_t>(take);
}
#define malloc allocate
''' + writer + binary + r'''
#undef malloc
static void reset(){wire.clear();calls=wants=0;clock_us=per_call=0;error=0;partial=stall=false;s_running=true;}
static void verify(const std::vector<uint8_t> &payload){
    assert(wire.size()>=6 && wire[0]==0x82 && (wire[1]&0x80));
    uint64_t n=wire[1]&127;size_t off=2;
    if(n==126){n=uint64_t(wire[2])<<8|wire[3];off=4;}
    else if(n==127){n=0;for(unsigned i=0;i<8;i++)n=n<<8|wire[2+i];off=10;}
    assert(n==payload.size() && wire.size()==off+4+n);
    for(size_t i=0;i<n;i++)assert((wire[off+4+i]^wire[off+(i&3)])==payload[i]);
}
int main(){
    esp_tls_t tls;
#if NOISE_CTRL_WS_WRITE_BYTES > 1024
    allocation_fails=true;assert(!ws_send_binary(&tls,nullptr,0) && calls==0);allocation_fails=false;
#endif
    for(size_t n:{0u,1u,125u,126u,127u,2048u,4096u,8192u,65535u,65536u}){
        std::vector<uint8_t> payload(n);for(size_t i=0;i<n;i++)payload[i]=uint8_t(i*17+9);
        const auto original=payload;reset();assert(ws_send_binary(&tls,payload.data(),n));verify(payload);assert(payload==original);
        assert(calls==(wire.size()+NOISE_CTRL_WS_WRITE_BYTES-1)/NOISE_CTRL_WS_WRITE_BYTES);
        if(n==2048 && NOISE_CTRL_WS_WRITE_BYTES==4096)assert(calls==1); // old path: header + four payload writes
        reset();partial=true;wants=3;assert(ws_send_binary(&tls,payload.data(),n));verify(payload);assert(payload==original);
    }
    std::vector<uint8_t> payload(10000,0xa5);
    reset();error=1;assert(!ws_send_binary(&tls,payload.data(),payload.size()) && calls==1 && wire.empty());
    reset();error=-99;assert(!ws_send_binary(&tls,payload.data(),payload.size()) && calls==1 && wire.empty());
    reset();stall=true;assert(!ws_send_binary(&tls,payload.data(),payload.size()) && clock_us==WS_FRAME_TIMEOUT_US);
    reset();per_call=8000000;assert(!ws_send_binary(&tls,payload.data(),payload.size()) && calls==2); // one deadline for the whole frame
    reset();s_running=false;assert(!ws_send_binary(&tls,payload.data(),payload.size()) && calls==0);
    reset();assert(write_all(&tls,"ping",4)==4 && wire.size()==4);
    std::puts("WebSocket: exact masks/lengths, coalesced writes, immutable payload, partial/WANT retry, shared frame deadline and terminal failures passed");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            driver = directory / "ws.cpp"
            driver.write_text(harness)
            for size in (512, 4096):
                binary = directory / f"ws-{size}"
                subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                                f"-DNOISE_CTRL_WS_WRITE_BYTES={size}", str(driver), "-o", str(binary)], check=True)
                subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
