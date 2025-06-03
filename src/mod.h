#include <windows.h>

#define AT_MOD(moduleName, addr, var) do { (var) = reinterpret_cast<decltype(var)>((char*)GetModuleHandle(moduleName) + addr); } while (0)
#define AT_EXE(rva, var) AT_MOD(nullptr, (rva) - 0x40'0000, var)
#define AT_DLL(moduleName, rva, var) AT_MOD(moduleName, (rva) - 0x100'0000, var)
