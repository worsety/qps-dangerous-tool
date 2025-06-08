#pragma once
#include <memory>
typedef std::unique_ptr<void, int (__stdcall *)(void*)> SmartHandle;
