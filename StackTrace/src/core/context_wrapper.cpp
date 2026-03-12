#include "context.h"
#include <winnt.h>

void CaptureContext(CONTEXT* ctx)
{
    if (!ctx)
        return;

    RtlZeroMemory(ctx, sizeof(CONTEXT));
    ctx->ContextFlags = CONTEXT_FULL;

    RtlCaptureContext(ctx);
}