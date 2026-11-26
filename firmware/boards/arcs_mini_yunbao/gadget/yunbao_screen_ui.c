/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "yunbao_screen_ui.h"
#include "impo_product_font.h"
#include "impo_product_config.h"

#define RGB(r,g,b) ((uint16_t)(((r)>>3)<<11 | ((g)>>2)<<5 | ((b)>>3)))
#define BG RGB(8,16,24)
#define WHITE RGB(235,245,250)
#define MUTED RGB(112,137,151)
#define CYAN RGB(66,214,232)
#define GREEN RGB(110,232,170)
#define AMBER RGB(248,190,84)

static void pixel(uint16_t *p, int x, int y, uint16_t color)
{
    if ((unsigned)x < YUNBAO_SCREEN_WIDTH && (unsigned)y < YUNBAO_SCREEN_HEIGHT)
        p[y * YUNBAO_SCREEN_WIDTH + x] = color;
}
static void rect(uint16_t *p, int x, int y, int w, int h, uint16_t c)
{
    for (int yy = y; yy < y+h; yy++) for (int xx = x; xx < x+w; xx++) pixel(p, xx, yy, c);
}
static void ellipse(uint16_t *p, int x, int y, int rx, int ry, uint16_t c)
{
    for (int dy = -ry; dy <= ry; dy++) for (int dx = -rx; dx <= rx; dx++)
        if (dx*dx*ry*ry + dy*dy*rx*rx <= rx*rx*ry*ry) pixel(p, x+dx, y+dy, c);
}
static const yunbao_glyph_t *glyph(uint32_t cp)
{
    size_t lo=0, hi=sizeof(yunbao_glyphs)/sizeof(yunbao_glyphs[0]);
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(yunbao_glyphs[mid].codepoint==cp)return &yunbao_glyphs[mid];
        if(yunbao_glyphs[mid].codepoint<cp)lo=mid+1;else hi=mid;
    }
    return NULL;
}
static uint32_t next(const unsigned char **s)
{
    unsigned char c = *(*s)++;
    if (c < 128) return c;
    if(c<194 || c>244)return 0xfffd;
    uint32_t cp = c & (c < 224 ? 31u : c < 240 ? 15u : 7u);
    unsigned n = c < 224 ? 1 : c < 240 ? 2 : 3;
    while(n--) { if((**s & 0xc0)!=0x80)return 0xfffd; cp=(cp<<6)|(*(*s)++ & 63u); }
    if(cp<128 || (c>=224 && cp<2048) || (c>=240 && cp<65536) ||
       cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return 0xfffd;
    return cp;
}
static void text(uint16_t *p, const char *str, int y, uint16_t color)
{
    const unsigned char *s = (const unsigned char *)str;
    int width = 0;
    while (*s) { const yunbao_glyph_t *g = glyph(next(&s)); width += g ? g->advance : 24; }
    int x = (YUNBAO_SCREEN_WIDTH-width)/2;
    s = (const unsigned char *)str;
    while (*s) {
        const yunbao_glyph_t *g = glyph(next(&s));
        if (g) {
            for (int yy = 0; yy < 24; yy++) for (int xx = 0; xx < 24; xx++)
                if (g->bits[yy*3+xx/8] & (0x80u >> (xx%8)))
                    pixel(p, x+xx-(24-g->advance)/2, y+yy, color);
            x += g->advance;
        } else x += 24;
    }
}

bool yunbao_screen_render_voice(uint16_t *p, unsigned width, unsigned height,
    gadget_status_t state, uint32_t ms, bool test, bool wake_available, bool dialogue_active,
    impo_arcs_voice_phase_t voice_phase)
{
    if (!p || width != YUNBAO_SCREEN_WIDTH || height != YUNBAO_SCREEN_HEIGHT) return false;
    for (unsigned i = 0; i < width*height; i++) p[i] = BG;
    if (test) {
        const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff};
        for (int i = 0; i < 4; i++) rect(p, i*60, 60, 60, 100, colors[i]);
        text(p, IMPO_SCREEN_TITLE, 15, WHITE); text(p, "IMPO", 190, WHITE);
        rect(p, 0, 0, 240, 2, WHITE); rect(p, 0, 238, 240, 2, WHITE);
        rect(p, 0, 0, 2, 240, WHITE); rect(p, 238, 0, 2, 240, WHITE);
        return true;
    }
    static const char *const labels[] = {
        "启动中", "等待配对", "连接中", "待机", "我在听", "思考中", "正在回答",
        "网络断开", "电量低", "充电中", "更新中"
    };
    unsigned st = (unsigned)state < sizeof(labels)/sizeof(labels[0]) ? (unsigned)state : 0;
    bool warn = st == GADGET_STATUS_UNPAIRED || st == GADGET_STATUS_CONNECTING ||
                st == GADGET_STATUS_OFFLINE || st == GADGET_STATUS_LOW_BATTERY;
    uint16_t accent = warn ? AMBER : st == GADGET_STATUS_SPEAKING ? GREEN : CYAN;
    bool blink = ms % 5000 >= 4800;
    int look = st == GADGET_STATUS_THINKING ? ((int)((ms/500)%3)-1)*5 : 0;
    text(p, IMPO_SCREEN_TITLE, 14, WHITE);
    for (int eye = 0; eye < 2; eye++) {
        int x = eye ? 165 : 75;
        if (blink || st == GADGET_STATUS_OFFLINE) {
            rect(p, x-20, 105, 40, 6, accent);
        } else {
            int ry = st == GADGET_STATUS_LISTENING ? 33 : 28;
            ellipse(p, x+look, 105, 21, ry, accent);
            ellipse(p, x+look-6, 94, 5, 7, WHITE);
        }
    }
    if (st == GADGET_STATUS_THINKING || st == GADGET_STATUS_CONNECTING) {
        for (int i = 0; i < 3; i++) ellipse(p, 102+i*18, 151, 3, 3, (ms/400)%3 == (unsigned)i ? WHITE : MUTED);
    } else if (st == GADGET_STATUS_LISTENING || st == GADGET_STATUS_SPEAKING) {
        for (int i = 0; i < 9; i++) {
            int h = 3 + (i*7+(int)(ms/250)*3)%10;
            rect(p, 85+i*8, 152-h/2, 4, h, accent);
        }
    } else {
        rect(p, 106, 149, 28, 3, accent); rect(p, 101, 144, 4, 5, accent); rect(p, 135, 144, 4, 5, accent);
    }
    const char *label = labels[st];
    if (state == GADGET_STATUS_IDLE || state == GADGET_STATUS_THINKING || state == GADGET_STATUS_SPEAKING) {
        switch (voice_phase) {
        case IMPO_VOICE_UPLOADING: label = "正在上传"; break;
        case IMPO_VOICE_WAITING: label = "等待回复"; break;
        case IMPO_VOICE_PREPARING: label = "准备语音"; break;
        case IMPO_VOICE_PLAYING: label = "正在回答"; break;
        case IMPO_VOICE_CANCELLING: label = "取消中"; break;
        case IMPO_VOICE_FAILED: label = "语音失败"; break;
        default: break;
        }
    }
    text(p, label, 169, WHITE);
    rect(p, 48, 202, 144, 1, RGB(32,52,66));
    text(p, dialogue_active ? "短按结束" : wake_available ? IMPO_WAKE_LABEL : "短按说话", 210, MUTED);
    return true;
}
bool yunbao_screen_render(uint16_t *p, unsigned width, unsigned height,
    gadget_status_t state, uint32_t ms, bool test, bool wake_available, bool dialogue_active)
{
    return yunbao_screen_render_voice(p, width, height, state, ms, test, wake_available,
                                     dialogue_active, IMPO_VOICE_IDLE);
}

bool impo_arcs_companion_text_supported(const char *str)
{
    if(!str || strlen(str)>192)return false;
    const unsigned char *s=(const unsigned char *)str;
    unsigned width=0, lines=1;
    while(*s) {
        if(*s=='\n'){s++;width=0;if(++lines>3)return false;continue;}
        uint32_t cp=next(&s);const yunbao_glyph_t *g=glyph(cp);
        if(!g || cp<32 || cp==127 || cp==0xfffd)return false;
        if(width+g->advance>216){width=0;if(++lines>3)return false;}
        width+=g->advance;
    }
    return true;
}
static void card(uint16_t *p,const char *str)
{
    const unsigned char *s=(const unsigned char *)str;
    char line[193];unsigned used=0,width=0,row=0;
    while(*s && row<3) {
        const unsigned char *begin=s;uint32_t cp=next(&s);
        const yunbao_glyph_t *g=glyph(cp);unsigned advance=g?g->advance:24;
        if(cp=='\n' || width+advance>216) {
            line[used]=0;text(p,line,73+(int)row*29,WHITE);used=width=0;row++;
            if(cp=='\n')continue;
        }
        size_t n=(size_t)(s-begin);
        if(row>=3 || used+n>=sizeof(line))break;
        memcpy(line+used,begin,n);used+=(unsigned)n;width+=advance;
    }
    if(row<3){line[used]=0;text(p,line,73+(int)row*29,WHITE);}
}
bool yunbao_screen_render_companion(uint16_t *p,unsigned width,unsigned height,
    gadget_status_t state,uint32_t ms,bool test,bool wake,bool dialogue,
    impo_arcs_voice_phase_t phase,const impo_arcs_companion_view_t *v)
{
    if(!yunbao_screen_render_voice(p,width,height,state,ms,test,wake,dialogue,phase))return false;
    if(!v || test || (state!=GADGET_STATUS_IDLE && state!=GADGET_STATUS_SPEAKING) ||
       phase==IMPO_VOICE_FAILED || phase==IMPO_VOICE_CANCELLING)return true;
    if(v->text[0]) {
        rect(p,12,65,216,99,BG);card(p,v->text);return true;
    }
    if(v->timer_running) {
        rect(p,12,65,216,99,BG);char clock[24];
        unsigned seconds=v->timer_remaining;
        snprintf(clock,sizeof(clock),"%02u:%02u:%02u",seconds/3600,seconds/60%60,seconds%60);
        text(p,clock,88,CYAN);text(p,"倒计时",124,WHITE);return true;
    }
    const char *e=v->expression;if(!*e)return true;
    rect(p,35,65,170,99,BG);
    bool happy=!strcmp(e,"happy"),sad=!strcmp(e,"sad"),sleepy=!strcmp(e,"sleepy");
    bool surprised=!strcmp(e,"surprised"),curious=!strcmp(e,"curious"),wink=!strcmp(e,"wink");
    for(int eye=0;eye<2;eye++) {
        int x=eye?165:75, y=curious?(eye?96:111):103;
        if(sleepy || (wink && eye==1) || ms%5000>=4800)rect(p,x-19,y,38,5,CYAN);
        else if(happy) { ellipse(p,x,y,21,18,GREEN);ellipse(p,x,y+10,24,17,BG); }
        else { ellipse(p,x,y,curious && eye?16:21,surprised?32:25,sad?AMBER:CYAN);ellipse(p,x-6,y-8,5,7,WHITE); }
    }
    if(surprised)ellipse(p,120,148,8,10,CYAN);
    else if(sad){ellipse(p,120,157,18,10,AMBER);ellipse(p,120,162,20,10,BG);}
    else if(happy){ellipse(p,120,141,18,13,GREEN);rect(p,99,126,42,15,BG);}
    else rect(p,108,148,24,3,CYAN);
    return true;
}
