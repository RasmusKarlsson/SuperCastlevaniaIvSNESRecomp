#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RecompRuntimeUi RecompRuntimeUi;

bool Cv4RuntimeSettingsImGuiInit(void *window, void *gl_context);
void Cv4RuntimeSettingsImGuiShutdown(void);
void Cv4RuntimeSettingsImGuiProcessEvent(const void *event);
void Cv4RuntimeSettingsImGuiRender(void *ui, int width, int height);

#ifdef __cplusplus
}
#endif
