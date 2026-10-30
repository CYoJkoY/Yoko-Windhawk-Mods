#pragma once

// Cursor sample ring buffer and trail reset helpers.

#include "platform.h"
#include "context.h"

void HistoryClear() { context.historyHead = 0; context.historyCount = 0; }
static void ResetTrailState() {
    HistoryClear();
    context.isSmearing = false;
    context.lowVelocityFrames = 0;
    context.fadeAlpha = 0.0f;
}
void HistoryPushFront(const POINT& point) {
    if (context.historyCount == kHistoryCapacity) --context.historyCount;
    context.historyHead = (context.historyHead - 1 + kHistoryCapacity) % kHistoryCapacity;
    context.history[context.historyHead] = point;
    ++context.historyCount;
}

void HistoryPopBack() { if (context.historyCount > 0) --context.historyCount; }

const POINT& HistoryAt(int index) { return context.history[(context.historyHead + index) % kHistoryCapacity]; }
