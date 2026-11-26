#pragma once
int xTaskCreate(void (*worker)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle);
void vTaskDelete(void *handle);
