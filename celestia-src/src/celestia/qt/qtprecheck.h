// HarmonyOS-specific Qt RHI diagnostics.
// Kept in a separate TU that includes only Qt headers, so that the
// Qt-GL vs epoxy header clash never occurs here.
#pragma once

#ifdef CELESTIA_OHOS
namespace celestia::qt {
// Create a QOpenGLContext with the default surface format and report the
// renderable type / version of the actually-created context. Returns 0 on
// success, nonzero on failure.
int qtPrecheckContextFormat();
void qtPrecheckResource();
}
#endif
