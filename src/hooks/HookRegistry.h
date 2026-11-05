#pragma once

void registerAllHooks();
void hookClientInstanceUpdate();
void unregisterAllHooks();
bool initDX11Hook();
void shutdownInputIME();
void shutdownCacheWriteThread();
void shutdownDX11Hook();
