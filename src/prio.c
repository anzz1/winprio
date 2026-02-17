// prio.c

// Windows XP and up
#define WINVER 0x501
#define _WIN32_WINNT 0x0501

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include "version.h"

#pragma comment (lib, "ntdll.lib")

#ifndef VERSION_STR
#error VERSION_STR undefined
#endif

__declspec(dllimport) long __stdcall NtQueryInformationProcess(
  HANDLE ProcessHandle,
  ULONG ProcessInformationClass,
  PVOID ProcessInformation,
  ULONG ProcessInformationLength,
  PULONG ReturnLength
);

__forceinline static int __wcsicmp(const unsigned short* ws1, const unsigned short* ws2) {
  unsigned short wc1, wc2;
  do {
    if (*ws1 == 0 && *ws2 == 0) return 0;
    wc1 = (*ws1>64 && *ws1<91) ? (*ws1+32):*ws1; // A-Z -> a-z
    wc2 = (*ws2>64 && *ws2<91) ? (*ws2+32):*ws2; // A-Z -> a-z
    ws1++; ws2++;
  } while (wc1 == wc2);
  return (*ws1 > *ws2) ? 1 : -1;
}

__forceinline static unsigned short* __wcsrchr(const unsigned short* ws, unsigned short wc) {
  unsigned short *p = 0;
  while (*ws != 0) {
    if (*ws == wc) p = (unsigned short*)ws;
    ws++;
  }
  return p;
}

__forceinline static size_t __strlen(const char* s) {
  size_t i = 0;
  while (s[i]) i++;
  return i;
}

__forceinline static size_t __wcslen(const unsigned short* ws) {
  size_t i = 0;
  while (ws[i]) i++;
  return i;
}

static void print(const char* cbuf) {
  DWORD u = 0;
  WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), cbuf, (DWORD)__strlen(cbuf), &u, 0);
}

static void perr(const char* cbuf) {
  DWORD u = 0;
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), cbuf, (DWORD)__strlen(cbuf), &u, 0);
}

static void fmt_error(const char *fmt, DWORD_PTR arg1, DWORD_PTR arg2) {
  char* fmt_str = 0;
  DWORD_PTR pArgs[] = { (DWORD_PTR)arg1, (DWORD_PTR)arg2 };
  if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY, fmt, 0, 0, (LPSTR)&fmt_str, 0, (va_list*)pArgs)) {
    perr(fmt_str);
    LocalFree(fmt_str);
  }
}

static BOOL PIDIsProcess(DWORD pid) {
  PROCESSENTRY32W lppew;
  HANDLE hSnapshot;
  BOOL found = FALSE;
  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot) {
    __stosb((PBYTE)&lppew, 0, sizeof(PROCESSENTRY32W));
    lppew.dwSize = sizeof(lppew);
    if (Process32FirstW(hSnapshot,&lppew)) {
      do {
        if(pid == lppew.th32ProcessID) {
          found = TRUE;
          break;
        }
      } while (Process32NextW(hSnapshot,&lppew));
    }
    CloseHandle(hSnapshot);
  }
  return found;
}

static DWORD GetPIDForProcessW(const wchar_t* process) {
  PROCESSENTRY32W lppew;
  wchar_t* pname;
  HANDLE hSnapshot;
  DWORD pid = 0;
  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot) {
    __stosb((PBYTE)&lppew, 0, sizeof(PROCESSENTRY32W));
    lppew.dwSize = sizeof(lppew);
    if (Process32FirstW(hSnapshot,&lppew)) {
      do {
        pname = __wcsrchr(lppew.szExeFile, L'\\');
        if(!__wcsicmp(process, (pname ? pname+1 : lppew.szExeFile))) {
          pid = lppew.th32ProcessID;
          break;
        }
      } while (Process32NextW(hSnapshot,&lppew));
    }
    CloseHandle(hSnapshot);
  }
  return pid;
}

DWORD GetParentProcessId() {
  ULONG_PTR pbi[6];
  ULONG ulSize = 0;
  if(NtQueryInformationProcess((HANDLE)-1, 0, &pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi))
    return (DWORD)pbi[5];
  return (DWORD)-1;
}

void print_help(void) {
  print("prio " VERSION_STR " - Set priority of a process\r\n"
        "Usage: prio <PRIORITY> [<PROCESSNAME> | <PROCESSID>]\r\n\r\n"
        "  0: low\n"
        "  1: belownormal\r\n"
        "  2: normal\r\n"
        "  3: abovenormal\r\n"
        "  4: high\r\n"
        "  5: realtime\r\n\r\n"
        "If neither PROCESSNAME nor PROCESSID is set, sets priority of the calling process\r\n");
}

int main(void) {
  wchar_t **argv;
  int argc;
  HANDLE pproc;
  DWORD prio;
  DWORD err;
  wchar_t *s;
  DWORD pid = 0;

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) {
    perr("prio: (!) shell32:CommandLineToArgvW() failed\r\n");
    ExitProcess(-1);
  }

  if (argc < 2) {
    print_help();
    ExitProcess(1);
  }

  s = argv[1];
  while (*s == L'-' || *s == L'/')
    s++;
  if ((*s == L'\0') || (*s == L' ') || (*s == L'\t') || (*s == L'\r') || (*s == L'\n') || (*s == L'?') ||
      ((*s == L'h' || *s == L'H') && (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' ||
                                      ((*(s+1) == L'E' || *(s+1) == L'e') && (*(s+2) == L'L' || *(s+2) == L'l') && (*(s+3) == L'P' || *(s+3) == L'p')))) ||
      ((*s == L'V' || *s == L'v') &&
       (*(s+1) == L'E' || *(s+1) == L'e') &&
       (*(s+2) == L'R' || *(s+2) == L'r') &&
       (*(s+3) == L'\0' || *(s+3) == L' ' || *(s+3) == L'\t' || *(s+3) == L'/' || *(s+3) == L'-') ||
       ((*(s+3) == L'S' || *(s+3) == L's') &&
        (*(s+4) == L'I' || *(s+4) == L'i') &&
        (*(s+5) == L'O' || *(s+5) == L'o') &&
        (*(s+6) == L'N' || *(s+6) == L'n')))) {
    print_help();
    ExitProcess(1);
  } else if (((*s == L'0') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'L' || *s == L'l') &&
              (*(s+1) == L'O' || *(s+1) == L'o') &&
              (*(s+2) == L'W' || *(s+2) == L'w') &&
              (*(s+3) == L'\0' || *(s+3) == L' ' || *(s+3) == L'\t' || *(s+3) == L'/' || *(s+3) == L'-' || *(s+3) == L'\r' || *(s+3) == L'\n')) ||
             ((*s == L'I' || *s == L'i') &&
              (*(s+1) == L'D' || *(s+1) == L'd') &&
              (*(s+2) == L'L' || *(s+2) == L'l') &&
              (*(s+3) == L'E' || *(s+3) == L'e') &&
              (*(s+4) == L'\0' || *(s+4) == L' ' || *(s+4) == L'\t' || *(s+4) == L'/' || *(s+4) == L'-' || *(s+4) == L'\r' || *(s+4) == L'\n'))) {
    prio = IDLE_PRIORITY_CLASS;
  } else if (((*s == L'1') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'B' || *s == L'b') &&
              (*(s+1) == L'E' || *(s+1) == L'e') &&
              (*(s+2) == L'L' || *(s+2) == L'l') &&
              (*(s+3) == L'O' || *(s+3) == L'o') &&
              (*(s+4) == L'W' || *(s+4) == L'w') &&
              (*(s+5) == L'N' || *(s+5) == L'n') &&
              (*(s+6) == L'O' || *(s+6) == L'o') &&
              (*(s+7) == L'R' || *(s+7) == L'r') &&
              (*(s+8) == L'M' || *(s+8) == L'm') &&
              (*(s+9) == L'A' || *(s+9) == L'a') &&
              (*(s+10) == L'L' || *(s+10) == L'l') &&
              (*(s+11) == L'\0' || *(s+11) == L' ' || *(s+11) == L'\t' || *(s+11) == L'/' || *(s+11) == L'-' || *(s+11) == L'\r' || *(s+11) == L'\n'))) {
    prio = BELOW_NORMAL_PRIORITY_CLASS;
  } else if (((*s == L'2') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'N' || *s == L'n') &&
              (*(s+1) == L'O' || *(s+1) == L'o') &&
              (*(s+2) == L'R' || *(s+2) == L'r') &&
              (*(s+3) == L'M' || *(s+3) == L'm') &&
              (*(s+4) == L'A' || *(s+4) == L'a') &&
              (*(s+5) == L'L' || *(s+5) == L'l') &&
              (*(s+6) == L'\0' || *(s+6) == L' ' || *(s+6) == L'\t' || *(s+6) == L'/' || *(s+6) == L'-' || *(s+6) == L'\r' || *(s+6) == L'\n'))) {
    prio = NORMAL_PRIORITY_CLASS;
  } else if (((*s == L'3') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'A' || *s == L'a') &&
              (*(s+1) == L'B' || *(s+1) == L'b') &&
              (*(s+2) == L'O' || *(s+2) == L'o') &&
              (*(s+3) == L'V' || *(s+3) == L'v') &&
              (*(s+4) == L'E' || *(s+4) == L'e') &&
              (*(s+5) == L'N' || *(s+5) == L'n') &&
              (*(s+6) == L'O' || *(s+6) == L'o') &&
              (*(s+7) == L'R' || *(s+7) == L'r') &&
              (*(s+8) == L'M' || *(s+8) == L'm') &&
              (*(s+9) == L'A' || *(s+9) == L'a') &&
              (*(s+10) == L'L' || *(s+10) == L'l') &&
              (*(s+11) == L'\0' || *(s+11) == L' ' || *(s+11) == L'\t' || *(s+11) == L'/' || *(s+11) == L'-' || *(s+11) == L'\r' || *(s+11) == L'\n'))) {
    prio = ABOVE_NORMAL_PRIORITY_CLASS;
  } else if (((*s == L'4') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'H' || *s == L'h') &&
              (*(s+1) == L'I' || *(s+1) == L'i') &&
              (*(s+2) == L'G' || *(s+2) == L'g') &&
              (*(s+3) == L'H' || *(s+3) == L'h') &&
              (*(s+4) == L'\0' || *(s+4) == L' ' || *(s+4) == L'\t' || *(s+4) == L'/' || *(s+4) == L'-' || *(s+4) == L'\r' || *(s+4) == L'\n'))) {
    prio = HIGH_PRIORITY_CLASS;
  } else if (((*s == L'5') &&
              (*(s+1) == L'\0' || *(s+1) == L' ' || *(s+1) == L'\t' || *(s+1) == L'/' || *(s+1) == L'-' || *(s+1) == L'\r' || *(s+1) == L'\n')) ||
             ((*s == L'R' || *s == L'r') &&
              (*(s+1) == L'E' || *(s+1) == L'e') &&
              (*(s+2) == L'A' || *(s+2) == L'a') &&
              (*(s+3) == L'L' || *(s+3) == L'l') &&
              (*(s+4) == L'T' || *(s+4) == L't') &&
              (*(s+5) == L'I' || *(s+5) == L'i') &&
              (*(s+6) == L'M' || *(s+6) == L'm') &&
              (*(s+7) == L'E' || *(s+7) == L'e') &&
              (*(s+8) == L'\0' || *(s+8) == L' ' || *(s+8) == L'\t' || *(s+8) == L'/' || *(s+8) == L'-' || *(s+8) == L'\r' || *(s+8) == L'\n'))) {
    prio = REALTIME_PRIORITY_CLASS;
  } else {
    fmt_error("prio: unknown command line option: '%1!.*ws!'\r\n", (DWORD_PTR)__wcslen(s), (DWORD_PTR)s);
    ExitProcess(2);
  }

  if (argc >= 3) {
    s = argv[2];
    for (unsigned int i = 0; s[i] != L'\0' && s[i] != L' ' && s[i] != L'\t' && s[i] != L'\r' && s[i] != L'\n'; i++) {
      if (s[i] < 48 || s[i] > 57) {
        pid = 0;
        break;
      }
      pid = pid * 10 + s[i] - 48;
    }
    if (pid) {
      if (!PIDIsProcess(pid)) {
        fmt_error("prio: could not find process with pid: %1!u!\r\n", (DWORD_PTR)pid, (DWORD_PTR)"");
        ExitProcess(-2);
      }
    } else {
      pid = GetPIDForProcessW(s);
      if (!pid) {
        fmt_error("prio: could not find process with name: '%1!.*ws!'\r\n", (DWORD_PTR)__wcslen(s), (DWORD_PTR)s);
        ExitProcess(-3);
      }
    }
  } else {
    pid = GetParentProcessId();
    if (pid == -1) {
      err = GetLastError();
      fmt_error("prio: (!) ntdll:NtQueryInformationProcess() failed; error code = 0x%1!08X!\r\n", (DWORD_PTR)err, (DWORD_PTR)"");
      ExitProcess(-4);
    }
  }

  pproc = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
  if (!pproc) {
    err = GetLastError();
    fmt_error("prio: (!) kernel32:OpenProcess() failed; error code = 0x%1!08X!\r\n", (DWORD_PTR)err, (DWORD_PTR)"");
    ExitProcess(-5);
  }
  if (!SetPriorityClass(pproc, prio)) {
    err = GetLastError();
    CloseHandle(pproc);
    fmt_error("prio: (!) kernel32:SetPriorityClass() failed; error code = 0x%1!08X!\r\n", (DWORD_PTR)err, (DWORD_PTR)"");
    ExitProcess(-6);
  }
  CloseHandle(pproc);
  ExitProcess(0);
}
