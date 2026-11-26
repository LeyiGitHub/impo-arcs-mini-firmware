/* SPDX-License-Identifier: MIT
 * One asynchronous public HTTPS download, decoded outside Link/display tasks.
 * Only the screen task writes the panel; image storage is copied under a mutex.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "http_client.h"
#include "platform_arcs.h"
#include "impo_arcs_image.h"

typedef struct {
    SemaphoreHandle_t lock;
    impo_arcs_image_snapshot_t view;
    uint16_t *frame;
    uint64_t until;
    uint32_t generation, next_id;
} image_t;
typedef struct { char url[512];uint32_t generation;unsigned seconds; } job_t;
static image_t *s;
static void take(void) { xSemaphoreTake(s->lock,portMAX_DELAY); }
static void give(void) { xSemaphoreGive(s->lock); }
static void expire(void)
{
    if (s->until && plat_uptime_ms()>=s->until) {
        free(s->frame);s->frame=NULL;s->until=0;
        strcpy(s->view.state,"expired");s->view.revision++;
    }
}
int impo_arcs_image_init(void)
{
    if (s) return 0;
    image_t *p=calloc(1,sizeof(*p)>1024?sizeof(*p):1025);
    if (!p) return -1;
    p->lock=xSemaphoreCreateMutex();
    if (!p->lock) { free(p);return -1; }
    strcpy(p->view.state,"idle");s=p;return 0;
}
static bool valid_url(const char *url)
{
    if (!url || strncmp(url,"https://",8) || strlen(url)>=512) return false;
    for (const unsigned char *p=(const unsigned char *)url;*p;p++) if (*p<=32 || *p>=127) return false;
    http_url_t parsed;
    return http_parse_url(url,&parsed)==0 && parsed.host[0];
}
static void download(void *arg)
{
    job_t *job=arg;http_response_t response={0};const char *why=NULL;
    unsigned width=0,height=0;uint16_t *frame=NULL;
    for (unsigned hop=0;;hop++) {
        if (http_request("GET",job->url,"Accept: image/png, image/jpeg\r\n",NULL,0,
                         IMPO_IMAGE_MAX_BYTES+8192,&response)) { why="image download failed";break; }
        if (response.status>=300 && response.status<400 && hop<3 && valid_url(response.location)) {
            snprintf(job->url,sizeof(job->url),"%s",response.location);
            http_response_free(&response);memset(&response,0,sizeof(response));continue;
        }
        if (response.status!=200) why="image server refused the file or redirect";
        else if (response.truncated || response.body_len>IMPO_IMAGE_MAX_BYTES) why="image file exceeds 256 KiB";
        else frame=impo_arcs_image_decode((const uint8_t *)response.body,response.body_len,&width,&height,&why);
        break;
    }
    http_response_free(&response);
    take();s->view.busy=false;
    if (job->generation==s->generation) {
        if (frame) {
            s->frame=frame;frame=NULL;s->view.width=width;s->view.height=height;
            s->until=plat_uptime_ms()+(uint64_t)job->seconds*1000;
            strcpy(s->view.state,"ready");
        } else {
            strcpy(s->view.state,"failed");
            snprintf(s->view.error,sizeof(s->view.error),"%s",why?why:"image decoding failed");
        }
        s->view.revision++;
    }
    give();free(frame);free(job);vTaskDelete(NULL);
}
int impo_arcs_image_start(const char *url,unsigned seconds,uint32_t *id,const char **why)
{
    *why="image service unavailable";
    if (!s) return -1;
    if (!valid_url(url) || seconds<1 || seconds>300) {
        *why="public HTTPS URL up to 511 ASCII characters and duration 1..300 seconds required";return -1;
    }
    job_t *job=calloc(1,sizeof(*job)>1024?sizeof(*job):1025);
    if (!job) { *why="out of memory for image request";return -1; }
    snprintf(job->url,sizeof(job->url),"%s",url);job->seconds=seconds;
    take();
    if (s->view.busy) { give();free(job);*why="another image download is still running";return -1; }
    free(s->frame);s->frame=NULL;s->until=0;s->generation++;
    job->generation=s->generation;s->view.image_id=++s->next_id;
    if (!s->view.image_id) s->view.image_id=++s->next_id;
    s->view.width=s->view.height=0;s->view.error[0]=0;s->view.busy=true;
    strcpy(s->view.state,"fetching");s->view.revision++;*id=s->view.image_id;give();
    if (xTaskCreate(download,"image_download",12288/sizeof(StackType_t),job,2,NULL)!=pdPASS) {
        take();s->view.busy=false;strcpy(s->view.state,"failed");
        strcpy(s->view.error,"out of memory for image task");s->view.revision++;give();free(job);
        *why="out of memory for image task";return -1;
    }
    *why=NULL;return 0;
}
void impo_arcs_image_clear(void)
{
    if (!s) return;
    take();s->generation++;free(s->frame);s->frame=NULL;s->until=0;
    s->view.width=s->view.height=0;s->view.error[0]=0;strcpy(s->view.state,"idle");s->view.revision++;give();
}
void impo_arcs_image_snapshot(impo_arcs_image_snapshot_t *out)
{
    memset(out,0,sizeof(*out));strcpy(out->state,"unavailable");if (!s) return;
    take();expire();*out=s->view;uint64_t now=plat_uptime_ms();
    if (s->until>now) out->remaining_seconds=(unsigned)((s->until-now+999)/1000);
    give();
}
bool impo_arcs_image_copy(uint16_t *pixels,unsigned width,unsigned height)
{
    if (!s || !pixels || width!=IMPO_IMAGE_WIDTH || height!=IMPO_IMAGE_HEIGHT) return false;
    take();expire();bool ready=s->frame!=NULL;
    if (ready) memcpy(pixels,s->frame,IMPO_IMAGE_WIDTH*IMPO_IMAGE_HEIGHT*sizeof(*pixels));
    give();return ready;
}
