#pragma once
typedef struct { int unused; } Shell;
Shell *shellGetCurrent(void);
int shellPrint(Shell *, const char *, ...);
#define SHELL_EXPORT_CMD(...)
