// Early bootstrap for the disposable test process. Loaded before its entry point.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <winternl.h>
#include <cstdio>
#include <cstring>
#include <cwchar>

static wchar_t root[32768];
static wchar_t profile[32768];
static wchar_t log_path[32768];
static HANDLE log_file = INVALID_HANDLE_VALUE;
static void log_line(const char* text) {
    if (log_file == INVALID_HANDLE_VALUE) return;
    DWORD wrote;
    WriteFile(log_file, text, (DWORD)strlen(text), &wrote, nullptr);
    WriteFile(log_file, "\r\n", 2, &wrote, nullptr);
    FlushFileBuffers(log_file);
}

static HRESULT WINAPI isolated_folder(HWND, int csidl, HANDLE, DWORD, LPWSTR out) {
    const wchar_t* suffix = L"\\Local";
    if ((csidl & 0xff) == CSIDL_APPDATA) suffix = L"\\Roaming";
    if ((csidl & 0xff) == CSIDL_PERSONAL) suffix = L"\\Documents";
    // GameMaker's SHGetFolderPathW call uses a MAX_PATH output buffer.
    if (wcslen(profile) + wcslen(suffix) >= MAX_PATH) return E_FAIL;
    wcscpy(out, profile); wcscat(out, suffix);
    char line[1024];
    snprintf(line, sizeof(line), "redirect SHGetFolderPathW csidl=%d", csidl);
    log_line(line);
    return S_OK;
}

static bool replace_import(HMODULE module, const char* name, void* replacement) {
    auto base = (BYTE*)module;
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return false;
    bool replaced = false;
    for (auto desc=(IMAGE_IMPORT_DESCRIPTOR*)(base+rva); desc->Name; ++desc) {
        if (!desc->OriginalFirstThunk) continue;
        auto names=(IMAGE_THUNK_DATA64*)(base+desc->OriginalFirstThunk);
        auto slots=(IMAGE_THUNK_DATA64*)(base+desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            auto item=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);
            if (strcmp((const char*)item->Name,name)) continue;
            DWORD old;
            if (!VirtualProtect(&slots->u1.Function,sizeof(void*),PAGE_READWRITE,&old)) continue;
            slots->u1.Function=(ULONGLONG)replacement;
            VirtualProtect(&slots->u1.Function,sizeof(void*),old,&old);
            replaced=true;
        }
    }
    return replaced;
}

// These public Steam API entry points return bool. Returning false keeps this
// disposable test process offline; no Steam login/cloud/leaderboard calls follow.
static void make_offline(void* base) {
    auto dos=(IMAGE_DOS_HEADER*)base;
    auto nt=(IMAGE_NT_HEADERS64*)((BYTE*)base+dos->e_lfanew);
    auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return;
    auto ex=(IMAGE_EXPORT_DIRECTORY*)((BYTE*)base+dir.VirtualAddress);
    auto names=(DWORD*)((BYTE*)base+ex->AddressOfNames);
    auto ords=(WORD*)((BYTE*)base+ex->AddressOfNameOrdinals);
    auto funcs=(DWORD*)((BYTE*)base+ex->AddressOfFunctions);
    for(DWORD i=0;i<ex->NumberOfNames;++i) {
        const char* name=(char*)base+names[i];
        if(strcmp(name,"SteamAPI_Init") && strcmp(name,"SteamAPI_InitSafe") &&
           strcmp(name,"SteamAPI_InitAnonymousUser") && strcmp(name,"SteamAPI_RestartAppIfNecessary")) continue;
        BYTE* fn=(BYTE*)base+funcs[ords[i]];
        DWORD old;
        if(VirtualProtect(fn,3,PAGE_EXECUTE_READWRITE,&old)) {
            fn[0]=0x31;fn[1]=0xc0;fn[2]=0xc3;
            FlushInstructionCache(GetCurrentProcess(),fn,3);
            VirtualProtect(fn,3,old,&old);
            log_line(name);
        }
    }
}

struct DllNotification {
    ULONG Flags;
    const UNICODE_STRING* FullDllName;
    const UNICODE_STRING* BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
};
static VOID CALLBACK loaded(ULONG reason, const void* raw, void*) {
    if(reason!=1) return;
    auto data=(const DllNotification*)raw;
    auto name=data->BaseDllName;
    if(name && name->Length==30 && !_wcsnicmp(name->Buffer,L"steam_api64.dll",15))
        make_offline(data->DllBase);
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if(reason!=DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    if(!GetEnvironmentVariableW(L"NOVACOOP_ROOT",root,32768)) return FALSE;
    if(!GetEnvironmentVariableW(L"NOVACOOP_PROFILE",profile,32768)) return FALSE;
    swprintf(log_path,32768,L"%ls\\sandbox.log",root);
    log_file=CreateFileW(log_path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    log_line("sandbox bootstrap loaded before game entry");
    if(!replace_import(GetModuleHandleW(nullptr),"SHGetFolderPathW",(void*)isolated_folder)) {
        log_line("FAIL: missing SHGetFolderPathW import; refusing startup");
        return FALSE;
    }
    auto ntdll=GetModuleHandleW(L"ntdll.dll");
    using Register=LONG (NTAPI*)(ULONG,void*,void*,void**);
    auto reg=(Register)GetProcAddress(ntdll,"LdrRegisterDllNotification");
    void* cookie=nullptr;
    if(!reg || reg(0,(void*)loaded,nullptr,&cookie)<0) {
        log_line("FAIL: cannot register offline guard; refusing startup");return FALSE;
    }
    auto steam=GetModuleHandleW(L"steam_api64.dll");
    if(steam) make_offline(steam);
    log_line("folder redirect and offline guard installed");
    return TRUE;
}
