#include <windows.h>
#include <psapi.h>
#include <cwchar>
#include "const_obfuscate.h"

OBF_IMPORT_DECL(ReadProcessMemory);
OBF_IMPORT_DECL(WriteProcessMemory);
OBF_IMPORT_DECL(CreateRemoteThread);

using namespace std;

const wchar_t help_msg[] =
    L"F1 - instantly kill boss phase / skip cutscene\n"
    L"F2 - cycle through hyper charge states\n"
    L"F3 - rewind to fixed points in stage\n"
    L"F4 - advance to fixed points in stage\n"
    L"F5 - save state 1\n"
    L"F6 - load state 1\n"
    L"F7 - save state 2\n"
    L"F8 - load state 2\n"
    L"F9 - open GUI";

struct Process {
    HANDLE handle{};
    char *base{};

    ~Process() { if (handle) CloseHandle(handle); }
    Process& operator=(const Process &other)
    {
        DuplicateHandle(GetCurrentProcess(), other.handle, GetCurrentProcess(),
            &handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        base = other.base;
        return *this;
    }

    bool read(const void *address, void *buffer, SIZE_T size);
    bool write(void *address, const void *data, SIZE_T size);
};

bool Process::read(const void *address, void *buffer, SIZE_T size)
{
    SIZE_T read_bytes;
    if (!OBF_FUNC(ReadProcessMemory)(handle, address, buffer, size, &read_bytes))
        return false;
    return read_bytes == size;
}

bool Process::write(void *address, const void *data, SIZE_T size)
{
    SIZE_T written_bytes;
    if (!OBF_FUNC(WriteProcessMemory)(handle, address, data, size, &written_bytes))
        return false;
    return written_bytes == size;
}

bool is_already_running(const Process &process)
{
    wchar_t mutexName[32] = L"ShootingPractice_";
    DWORD procId = GetProcessId(process.handle);
    for (wchar_t *p = mutexName + lstrlen(mutexName); procId; procId >>= 4)
        *p++ = L'A' + (procId & 15);
    HANDLE runOnceMutex = OpenMutex(SYNCHRONIZE, FALSE, mutexName);
    if (runOnceMutex) {
        CloseHandle(runOnceMutex);
        return true;
    }
    return false;
}

Process findProcess(const wchar_t *needle, DWORD desiredAccess)
{
    static DWORD pids[16384];
    wchar_t queryModuleName[MAX_PATH];

    HANDLE hProc{};
    HMODULE hMod{};
    MODULEINFO modInfo;
    DWORD cbNeeded{};

    if (0 == EnumProcesses(pids, sizeof pids, &cbNeeded))
        return {};

    for (int n = cbNeeded / sizeof * pids, i = 0; i < n; i++, hProc && CloseHandle(hProc)) {
        hProc = OpenProcess(desiredAccess | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pids[i]);
        if (!hProc)
            continue;
        if (!EnumProcessModulesEx(hProc, &hMod, sizeof hMod, &cbNeeded, LIST_MODULES_ALL))
            continue;
        if (cbNeeded < sizeof hMod)
            continue;
        if (!GetModuleBaseName(hProc, hMod, queryModuleName, sizeof queryModuleName / sizeof *queryModuleName))
            continue;
        if (wcscmp(queryModuleName, needle))
            continue;
        if (0 == GetModuleInformation(hProc, hMod, &modInfo, sizeof modInfo))
            continue;
        return {hProc, reinterpret_cast<char*>(modInfo.lpBaseOfDll)};
    }
    return {};
}

bool inject(const wchar_t *target_exe, const wchar_t *dll_path, bool (*check)(Process&))
{
    Process process;
    while (1) {
        process = findProcess(target_exe, PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION);
        if (process.handle)
            break;
        if (IDCANCEL == MessageBox(nullptr, L"Could not find/access process", L"Error", MB_RETRYCANCEL | MB_ICONERROR))
            return false;
    }
    if (is_already_running(process))
        return true;
    if (!check(process)) {
        MessageBox(nullptr, L"Unsupported game version.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    auto size = wcslen(dll_path) * sizeof *dll_path;
    auto buffer = VirtualAllocEx(process.handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) {
        MessageBox(nullptr, L"Failed to allocate memory in process.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!process.write(buffer, dll_path, size)) {
        MessageBox(nullptr, L"Failed to write to process memory.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    auto loadlib_thread = OBF_FUNC(CreateRemoteThread)(process.handle, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary), buffer, 0, nullptr);
    if (!loadlib_thread) {
        MessageBox(nullptr, L"Failed to create remote thread.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    WaitForSingleObject(loadlib_thread, INFINITE);
    VirtualFreeEx(process.handle, buffer, 0, MEM_RELEASE);
    DWORD exitCode;
    if (!GetExitCodeThread(loadlib_thread, &exitCode) || !exitCode) {
        MessageBox(nullptr, L"Failed to load DLL", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

bool check_dangerous(Process &process)
{
    IMAGE_DOS_HEADER dos{};
    if (!process.read(process.base, &dos, sizeof dos))
        return false;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    IMAGE_NT_HEADERS32 nt{};
    if (!process.read(process.base + dos.e_lfanew, &nt, sizeof nt))
        return false;
    if (nt.Signature != IMAGE_NT_SIGNATURE)
        return false;
    switch (nt.FileHeader.TimeDateStamp) {
        // Before 1.0: absolutely not, update your game
        case 1359384688: // 1.3, last patch for doujin version
            // TODO: maybe 1.3 support some day
            return false;
        case 1428050881: // 1.4.1
            return true;
        default:
            return false;
    }
}

static void init_obfuscated_functions()
{
    OBF_IMPORT(kernel32, ReadProcessMemory);
    OBF_IMPORT(kernel32, WriteProcessMemory);
    OBF_IMPORT(kernel32, CreateRemoteThread);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    wchar_t dll_dir[MAX_PATH], dangerous_dll[MAX_PATH];

    init_obfuscated_functions();

    SetProcessDPIAware();
    GetCurrentDirectory(MAX_PATH, dll_dir);
    swprintf(dangerous_dll, MAX_PATH, L"%ls\\%ls", dll_dir, L"dangerous.dll");
    if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(dangerous_dll)) {
        MessageBox(nullptr, L"dangerous.dll missing", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    if (inject(L"QP Shooting.exe", dangerous_dll, check_dangerous))
        MessageBox(nullptr, help_msg, L"Success", MB_OK);
    return 0;
}
