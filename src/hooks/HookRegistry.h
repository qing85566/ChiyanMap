#pragma once

void registerAllHooks();
void hookClientInstanceUpdate();
void unregisterAllHooks();
bool initDX11Hook();
void shutdownCacheWriteThread();
void shutdownDX11Hook();
