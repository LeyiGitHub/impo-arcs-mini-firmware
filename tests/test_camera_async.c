#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "link_camera.h"
#include "gadget_core.h"
static void (*worker)(void *);
static void *worker_arg;
static unsigned sent,deleted;
static int create_ok=1;
int xTaskCreate(void (*w)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{
    assert(!strcmp(name,"camera_capture") && stack==8192/sizeof(uint32_t) && priority==4 && !handle);
    if (!create_ok) return 0;
    worker=w;worker_arg=arg;return 1;
}
void vTaskDelete(void *arg) { assert(!arg); deleted++; }
cJSON *gadget_command_error(const char *why)
{
    cJSON *r=cJSON_CreateObject(); cJSON_AddBoolToObject(r,"ok",false);
    cJSON_AddStringToObject(r,"error",why); return r;
}
cJSON *gadget_run_command(const char *name,const cJSON *params)
{
    assert(!strcmp(name,"camera.capture"));
    assert(cJSON_GetObjectItem(params,"value")->valueint==42);
    return cJSON_CreateObject();
}
void noise_ctrl_send_command_result(noise_ctrl_session_generation_t generation,const char *request,cJSON *result)
{
    assert(generation==9 && !strcmp(request,"request-camera-123"));
    assert(result);sent++;cJSON_Delete(result);
}
int main(void)
{
    cJSON *params=cJSON_Parse("{\"value\":42}");
    cJSON *r=link_camera_invoke(params,"request-camera-123",9);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(r,"_async"))); cJSON_Delete(r);
    cJSON_Delete(params); /* Worker must own an independent deep copy. */
    r=link_camera_invoke(NULL,"second",9);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok")) && !sent);cJSON_Delete(r);
    worker(worker_arg); assert(sent==1 && deleted==1);
    create_ok=0;
    for (unsigned n=0;n<3;n++) {
        r=link_camera_invoke(NULL,"creation-failed",9);
        assert(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok"))); cJSON_Delete(r);
    }
    r=link_camera_invoke(NULL,"request-camera-123",0);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok"))); cJSON_Delete(r);
    char long_id[65];memset(long_id,'x',64);long_id[64]=0;
    r=link_camera_invoke(NULL,long_id,9);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(r,"ok"))); cJSON_Delete(r);
    puts("Camera async ownership and request scoping tests passed");
}
