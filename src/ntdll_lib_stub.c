// ntdll_lib_stub.c

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int __stdcall DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  return 1;
}

__declspec(dllexport) long __stdcall NtQueryInformationProcess(
HANDLE ProcessHandle,
ULONG ProcessInformationClass,
PVOID ProcessInformation,
ULONG ProcessInformationLength,
PULONG ReturnLength) {
  return 1;
}
