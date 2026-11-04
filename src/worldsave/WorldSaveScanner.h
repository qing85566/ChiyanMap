#pragma once
#include <string>

namespace WorldSaveScanner {
    struct Progress {
        bool isScanning = false;
        bool isCompleted = false;
        int processedChunks = 0;
        int totalChunks = 0;
        float progressFraction = 0.0f;
        float elapsedSeconds = 0.0f;
        std::string statusMessage;
    };

    inline Progress GetProgress() {
        return Progress{};
    }

    inline void StartScan() {
    }

    inline void CancelScan() {
    }
}
