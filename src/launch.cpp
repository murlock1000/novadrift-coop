#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cctype>
#include <cstring>

struct DriveMap {
    std::wstring device, target;
    ~DriveMap() {
        if(!device.empty()) DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|DDD_EXACT_MATCH_ON_REMOVE|DDD_NO_BROADCAST_SYSTEM,device.c_str(),target.c_str());
    }
    bool create(const std::wstring& path) {
        target=L"\\??\\UNC\\"+path.substr(2);
        wchar_t buffer[1024];
        for(wchar_t letter=L'Z';letter>=L'K';--letter) {
            std::wstring candidate(1,letter);candidate+=L":";
            if(QueryDosDeviceW(candidate.c_str(),buffer,1024)) continue;
            if(DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_NO_BROADCAST_SYSTEM,candidate.c_str(),target.c_str())) {
                device=candidate;return true;
            }
        }
        return false;
    }
};

static int fail(const char* message) {
    fprintf(stderr,"FAIL: %s (Win32 %lu)\n",message,GetLastError());return 1;
}
static void record_exception(HANDLE process,DWORD pid,DWORD tid,const EXCEPTION_DEBUG_INFO& event,const std::wstring& root) {
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
    CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
    GetThreadContext(thread,&context);
    const CONTEXT fault_context=context;
    printf("EXCEPTION code=%08lx first=%lu pc=%p rsp=%llx\n",event.ExceptionRecord.ExceptionCode,event.dwFirstChance,event.ExceptionRecord.ExceptionAddress,context.Rsp);
    SymInitialize(process,nullptr,TRUE);
    STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrPC.Mode=AddrModeFlat;
    frame.AddrFrame.Offset=context.Rbp;frame.AddrFrame.Mode=AddrModeFlat;
    frame.AddrStack.Offset=context.Rsp;frame.AddrStack.Mode=AddrModeFlat;
    for(int i=0;i<40;++i) {
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)) break;
        DWORD64 base=SymGetModuleBase64(process,frame.AddrPC.Offset);
        IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);
        SymGetModuleInfo64(process,frame.AddrPC.Offset,&module);
        printf("FRAME %d pc=%llx module=%s rva=%llx\n",i,frame.AddrPC.Offset,module.ModuleName,frame.AddrPC.Offset-base);
    }
    SymCleanup(process);
    wchar_t path[32768];swprintf(path,32768,L"%ls\\crash-%lu.dmp",root.c_str(),pid);
    HANDLE out=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    EXCEPTION_RECORD exception=event.ExceptionRecord;
    CONTEXT dump_context=fault_context;
    EXCEPTION_POINTERS pointers{&exception,&dump_context};
    MINIDUMP_EXCEPTION_INFORMATION info{tid,&pointers,FALSE};
    if(out!=INVALID_HANDLE_VALUE) {MiniDumpWriteDump(process,pid,out,MiniDumpWithIndirectlyReferencedMemory,&info,nullptr,nullptr);CloseHandle(out);}
    CloseHandle(thread);fflush(stdout);
}
enum class DisplayMode { Saved, Fullscreen, Windowed };
struct LaunchOptions { unsigned seconds=90; bool debugging=false,silent=false; DisplayMode display=DisplayMode::Saved; };
static bool launch_options(int argc,wchar_t** argv,LaunchOptions& options) {
    int first=4;
    if(argc>4) {
        wchar_t* end=nullptr;errno=0;
        const auto value=wcstoul(argv[4],&end,10);
        if(end!=argv[4] && *end==0) {
            if(errno==ERANGE || argv[4][0]==L'-' || value>UINT_MAX)return false;
            options.seconds=static_cast<unsigned>(value);first=5;
        }
    }
    for(int i=first;i<argc;++i) {
        if(!_wcsicmp(argv[i],L"debug"))options.debugging=true;
        else if(!_wcsicmp(argv[i],L"silent"))options.silent=true;
        else if(!_wcsicmp(argv[i],L"fullscreen") || !_wcsicmp(argv[i],L"windowed")) {
            const auto requested=!_wcsicmp(argv[i],L"fullscreen")?DisplayMode::Fullscreen:DisplayMode::Windowed;
            if(options.display!=DisplayMode::Saved && options.display!=requested)return false;
            options.display=requested;
        }else return false;
    }
    return true;
}
static std::wstring game_command(const std::wstring& exe,const std::wstring& root,const LaunchOptions& options) {
    const bool windowed=options.display==DisplayMode::Windowed ||
        (options.display==DisplayMode::Saved && options.seconds!=0);
    return L"\""+exe+L"\""+(windowed?L" -inawindow":L"")+L" -nosteamrestart"+
        (options.silent?L" -noaudio":L"")+std::wstring(L" -debugoutput \"")+root+L"\\runner.log\"";
}
static std::string trimmed(std::string value) {
    const auto nonspace=[](unsigned char c){return !std::isspace(c);};
    const auto start=std::find_if(value.begin(),value.end(),nonspace);
    const auto end=std::find_if(value.rbegin(),value.rend(),nonspace).base();
    return start<end?std::string(start,end):std::string();
}
static std::string lower_ascii(std::string value) {
    for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;
}
// Native save.ini uses lowercase [settings] and quoted decimal values. Preserve
// all unrelated bytes, including comments, save records, newline style and BOM.
static bool display_preferences(const std::string& original,DisplayMode mode,std::string& updated) {
    updated=original;if(mode==DisplayMode::Saved)return true;
    if(original.find('\0')!=std::string::npos)return false; // unsupported encoding; never rewrite it
    struct Line {std::string body,ending;};std::vector<Line> lines;
    const std::string newline=original.find("\r\n")!=std::string::npos?"\r\n":"\n";
    for(size_t at=0;at<original.size();) {
        auto end=original.find('\n',at);const bool last=end==std::string::npos;
        if(last)end=original.size();
        const bool cr=!last&&end>at&&original[end-1]=='\r';
        lines.push_back({original.substr(at,end-at-(cr?1:0)),last?"":cr?"\r\n":"\n"});
        at=last?original.size():end+1;
    }
    size_t section=lines.size(),section_end=lines.size();
    for(size_t i=0;i<lines.size();++i) {
        auto text=trimmed(lines[i].body);
        if(i==0&&text.rfind("\xef\xbb\xbf",0)==0)text=text.substr(3);
        if(text.empty()||text[0]!='[')continue;
        const auto close=text.find(']');if(close==std::string::npos)continue;
        if(section!=lines.size()){section_end=i;break;}
        if(lower_ascii(trimmed(text.substr(1,close-1)))=="settings")section=i;
    }
    const std::string keys[]={"windowedmode","borderlesswindow"};
    const std::string spelling[]={"windowedMode","borderlessWindow"};
    const std::string values[]={mode==DisplayMode::Windowed?"\"1.000000\"":"\"0.000000\"","\"0.000000\""};
    bool found[2]{};
    if(section!=lines.size())for(size_t i=section+1;i<section_end;++i) {
        auto& body=lines[i].body;const auto equal=body.find('=');if(equal==std::string::npos)continue;
        auto key=lower_ascii(trimmed(body.substr(0,equal)));
        for(int which=0;which<2;++which)if(key==keys[which]) {
            found[which]=true;size_t first=equal+1;
            while(first<body.size()&&std::isspace(static_cast<unsigned char>(body[first])))++first;
            size_t last=first;
            if(first<body.size()&&body[first]=='"') {
                const auto quote=body.find('"',first+1);if(quote==std::string::npos)return false;last=quote+1;
            }else {
                last=body.find_first_of(";#",first);if(last==std::string::npos)last=body.size();
                while(last>first&&std::isspace(static_cast<unsigned char>(body[last-1])))--last;
            }
            body.replace(first,last-first,values[which]);
        }
    }
    if(section==lines.size()) {
        if(!lines.empty()&&lines.back().ending.empty())lines.back().ending=newline;
        lines.push_back({"[settings]",newline});section_end=lines.size();
    }
    std::vector<Line> added;
    for(int which=0;which<2;++which)if(!found[which])added.push_back({spelling[which]+"="+values[which],newline});
    if(!added.empty()) {
        if(section_end>0&&lines[section_end-1].ending.empty())lines[section_end-1].ending=newline;
        lines.insert(lines.begin()+section_end,added.begin(),added.end());
    }
    updated.clear();for(const auto& line:lines)updated+=line.body+line.ending;return true;
}
static bool apply_display_preferences(const std::wstring& profile,DisplayMode mode) {
    if(mode==DisplayMode::Saved)return true;
    const std::wstring directory=profile+L"\\Local\\NovaDrift",path=directory+L"\\save.ini";
    std::string original;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    const bool existed=file!=INVALID_HANDLE_VALUE;
    if(existed) {
        LARGE_INTEGER size{};
        if(!GetFileSizeEx(file,&size)){CloseHandle(file);return false;}
        if(size.QuadPart<0||size.QuadPart>16*1024*1024){CloseHandle(file);SetLastError(ERROR_FILE_TOO_LARGE);return false;}
        original.resize(static_cast<size_t>(size.QuadPart));DWORD read=0;
        const bool read_ok=original.empty() || (ReadFile(file,original.data(),static_cast<DWORD>(original.size()),&read,nullptr)&&read==original.size());
        CloseHandle(file);if(!read_ok)return false;
    }else if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)return false;
    std::string updated;if(!display_preferences(original,mode,updated)) {SetLastError(ERROR_INVALID_DATA);return false;}
    if(updated==original)return true;
    for(const auto& folder:{profile,profile+L"\\Local",directory})
        if(!CreateDirectoryW(folder.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    const std::wstring temporary=path+L".display-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,updated.data(),static_cast<DWORD>(updated.size()),&written,nullptr)&&written==updated.size();
    if(ok)ok=FlushFileBuffers(file)!=FALSE;CloseHandle(file);
    if(ok&&existed)ok=CopyFileW(path.c_str(),(path+L".display-mode.bak").c_str(),FALSE)!=FALSE;
    if(ok)ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok){const auto error=GetLastError();DeleteFileW(temporary.c_str());SetLastError(error);}
    return ok;
}
#ifndef NOVACOOP_LAUNCH_TEST
int wmain(int argc,wchar_t** argv) {
    if(argc<4) {fwprintf(stderr,L"Usage: launch.exe <game.exe> <sandbox.dll> <runtime-dir> [seconds=90; 0=interactive] [debug] [silent] [fullscreen|windowed]\n");return 2;}
    LaunchOptions options;if(!launch_options(argc,argv,options)){fwprintf(stderr,L"Invalid launcher options or conflicting display modes.\n");return 2;}
    std::wstring root=argv[3],exe=argv[1],dll=argv[2];
    std::vector<wchar_t> full_root(32768);
    DWORD root_length=GetFullPathNameW(root.c_str(),(DWORD)full_root.size(),full_root.data(),nullptr);
    if(!root_length || root_length>=full_root.size()) return fail("Runtime root path");
    root=full_root.data();
    while(root.size()>2 && (root.back()==L'\\' || root.back()==L'/')) root.pop_back();
    DriveMap mapping;
    if(root.rfind(L"\\\\",0)==0) {
        const auto split=root.find_last_of(L"\\/");
        const auto leaf=root.substr(split+1);
        if(!mapping.create(root.substr(0,split))) return fail("Temporary drive mapping");
        root=mapping.device+L"\\"+leaf;
        exe=root+L"\\"+exe.substr(exe.find_last_of(L"\\/")+1);
        dll=root+L"\\"+dll.substr(dll.find_last_of(L"\\/")+1);
        fwprintf(stdout,L"MAPPED workspace runtime to %ls\n",root.c_str());
    }
    const std::wstring profile=root.substr(0,root.find_last_of(L"\\/"))+L"\\test-profile";
    SetEnvironmentVariableW(L"NOVACOOP_ROOT",root.c_str());
    SetEnvironmentVariableW(L"NOVACOOP_PROFILE",profile.c_str());
    SetEnvironmentVariableW(L"LOCALAPPDATA",(profile+L"\\Local").c_str());
    SetEnvironmentVariableW(L"APPDATA",(profile+L"\\Roaming").c_str());
    SetEnvironmentVariableW(L"TEMP",(profile+L"\\Temp").c_str());
    SetEnvironmentVariableW(L"TMP",(profile+L"\\Temp").c_str());
    if(!apply_display_preferences(profile,options.display))return fail("Isolated display preferences");
    const bool debugging=options.debugging;
    std::wstring command=game_command(exe,root,options);
    std::vector<wchar_t> line(command.begin(),command.end());line.push_back(0);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    const std::wstring working=root+L"\\";
    if(!CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,working.c_str(),&si,&pi)) return fail("CreateProcess");
    auto stop=[&](){TerminateProcess(pi.hProcess,80);WaitForSingleObject(pi.hProcess,10000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);};
    SIZE_T bytes=(dll.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(pi.hProcess,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote || !WriteProcessMemory(pi.hProcess,remote,dll.c_str(),bytes,nullptr)) {stop();return fail("WriteProcessMemory");}
    auto load=(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
    HANDLE thread=CreateRemoteThread(pi.hProcess,nullptr,0,load,remote,0,nullptr);
    if(!thread) {stop();return fail("CreateRemoteThread");}
    if(WaitForSingleObject(thread,20000)!=WAIT_OBJECT_0) {CloseHandle(thread);stop();return fail("Bootstrap timeout");}
    DWORD loaded=0;GetExitCodeThread(thread,&loaded);CloseHandle(thread);
    VirtualFreeEx(pi.hProcess,remote,0,MEM_RELEASE);
    if(!loaded) {stop();return fail("Bootstrap refused startup");}
    if(debugging && !DebugActiveProcess(pi.dwProcessId)) {stop();return fail("DebugActiveProcess");}
    ResumeThread(pi.hThread);
    printf("STARTED pid=%lu\n",pi.dwProcessId);fflush(stdout);
    const unsigned seconds=options.seconds;
    DWORD wait=WAIT_TIMEOUT;
    std::wstring stop_file=root+L"\\stop-"+std::to_wstring(pi.dwProcessId)+L".request";
    auto stop_requested=[&](){return GetFileAttributesW(stop_file.c_str())!=INVALID_FILE_ATTRIBUTES;};
    if(debugging) {
        ULONGLONG end=seconds?GetTickCount64()+seconds*1000:~0ULL;
        DEBUG_EVENT event{};
        while(GetTickCount64()<end && !stop_requested()) {
            if(!WaitForDebugEvent(&event,100)) continue;
            DWORD disposition=DBG_CONTINUE;
            if(event.dwDebugEventCode==EXCEPTION_DEBUG_EVENT) {
                auto& ex=event.u.Exception;
                auto code=ex.ExceptionRecord.ExceptionCode;
                if(code!=EXCEPTION_BREAKPOINT && code!=EXCEPTION_SINGLE_STEP) {
                    disposition=DBG_EXCEPTION_NOT_HANDLED;
                    if(code==EXCEPTION_STACK_OVERFLOW || !ex.dwFirstChance) record_exception(pi.hProcess,pi.dwProcessId,event.dwThreadId,ex,root);
                }
            }
            if(event.dwDebugEventCode==LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
            if(event.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT && event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
            bool exited=event.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT;
            ContinueDebugEvent(event.dwProcessId,event.dwThreadId,disposition);
            if(exited) {wait=WAIT_OBJECT_0;break;}
        }
        if(wait==WAIT_TIMEOUT) DebugActiveProcessStop(pi.dwProcessId);
    } else {
        ULONGLONG end=seconds?GetTickCount64()+seconds*1000:~0ULL;
        while(GetTickCount64()<end && !stop_requested()) {
            wait=WaitForSingleObject(pi.hProcess,100);
            if(wait==WAIT_OBJECT_0) break;
        }
    }
    if(wait==WAIT_TIMEOUT) {
        bool requested=stop_requested();
        if(requested) DeleteFileW(stop_file.c_str());
        printf("%s pid=%lu; stopping owned test process\n",requested?"STOP REQUEST":"TIMEOUT",pi.dwProcessId);
        stop();return 3;
    }
    DWORD exit=0;GetExitCodeProcess(pi.hProcess,&exit);
    printf("EXIT pid=%lu code=%lu\n",pi.dwProcessId,exit);
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    return exit?1:0;
}

#endif // NOVACOOP_LAUNCH_TEST
