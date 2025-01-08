#include <windows.h>
#include <psapi.h>
#include <cwchar>

using namespace std;

const wchar_t dangerous_help_msg[] =
    L"F1 - instantly kill boss phase\n"
    L"F2 - cycle through hyper charge states\n"
    L"F3 - rewind to fixed points in stage\n"
    L"F4 - advance to fixed points in stage\n"
    L"F5 - save state 1\n"
    L"F6 - load state 1\n"
    L"F7 - save state 2\n"
    L"F8 - load state 2\n";

struct Process {
	HANDLE hProc;
	char *base;
};

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
        if (process.hProc)
            break;
        if (IDCANCEL == MessageBox(NULL, L"Could not find/access process", L"Error", MB_RETRYCANCEL | MB_ICONERROR))
            return false;
        CloseHandle(process.hProc);
    }
	if (!check(process)) {
        MessageBox(NULL, L"Mismatched version.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}
	auto size = wcslen(dll_path) * sizeof *dll_path;
	auto buffer = VirtualAllocEx(process.hProc, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buffer) {
        MessageBox(NULL, L"Failed to allocate memory in process.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}
	if (!WriteProcessMemory(process.hProc, buffer, dll_path, size, nullptr)) {
        MessageBox(NULL, L"Failed to write to process memory.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}
	if (!CreateRemoteThread(process.hProc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibrary), buffer, 0, nullptr)) {
        MessageBox(NULL, L"Failed to create remote thread.", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

bool check_dangerous(Process &process)
{
    wchar_t window_title[32];
    ReadProcessMemory(process.hProc, process.base + 0x37da50, window_title, sizeof(window_title), NULL);
    return !memcmp(window_title, L"QP Shooting - Dangerous!! 1.4.1", sizeof window_title);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	wchar_t dll_dir[MAX_PATH], dll_path[MAX_PATH];

    SetProcessDPIAware();
	GetCurrentDirectory(MAX_PATH, dll_dir);
	swprintf(dll_path, MAX_PATH, L"%ls\\%ls", dll_dir, L"dangerous.dll");
	if (inject(L"QP Shooting.exe", dll_path, check_dangerous))
        MessageBox(NULL, dangerous_help_msg, L"Probably works now", MB_OK);
    return 0;
}
