// Steam launch-option wrapper. No Steam API, account or Steam configuration access.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
namespace fs=std::filesystem;
static fs::path runtime;
static HWND launch_edit;
static std::wstring Quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slash=0;
    for(wchar_t ch:value) {if(ch==L'\\') {++slash;continue;} if(ch==L'\"') out.append(slash*2+1,L'\\'); else out.append(slash,L'\\'); slash=0;out+=ch;}
    out.append(slash*2,L'\\');return out+L"\"";
}
static std::wstring LaunchOption() {return Quote((runtime/L"SteamCoop.exe").wstring())+L" --host %command%";}
static std::wstring HostCommand() {
    return Quote((runtime/L"novacoop_launch.exe").wstring())+L" "+Quote((runtime/L"NovaDrift.exe").wstring())+L" "+
        Quote((runtime/L"novacoop_sandbox.dll").wstring())+L" "+Quote(runtime.wstring())+L" 0 fullscreen";
}
static std::wstring Validate() {
    if(!fs::is_regular_file(runtime.parent_path()/L".novadriftcoop-owned")) return L"The isolated installation ownership marker is missing.";
    for(const auto* file:{L"NovaDrift.exe",L"novacoop_launch.exe",L"novacoop_sandbox.dll",L"mods/aurie/NovaDriftShips.dll"})
        if(!fs::is_regular_file(runtime/file)) return std::wstring(L"Missing isolated co-op file: ")+file;
    return {};
}
static void Error(const std::wstring& message) {MessageBoxW(nullptr,message.c_str(),L"Nova Drift Steam co-op",MB_ICONERROR|MB_OK);}
static bool DonorIdentity() {
    wchar_t id[64]{};GetEnvironmentVariableW(L"SteamAppId",id,64);
    if(std::wstring(id)==L"480") return true;
    GetEnvironmentVariableW(L"SteamGameId",id,64);return std::wstring(id)==L"480";
}
static bool AlreadyRunning() {
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE) return true;
    PROCESSENTRY32W e{};e.dwSize=sizeof(e);bool found=false;
    if(Process32FirstW(snap,&e)) do {if(_wcsicmp(e.szExeFile,L"NovaDrift.exe")) continue;
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(!p) continue;
        wchar_t name[32768]{};DWORD n=32768;
        if(QueryFullProcessImageNameW(p,0,name,&n)&&!_wcsicmp(name,(runtime/L"NovaDrift.exe").c_str())) found=true;
        CloseHandle(p);
    }while(!found&&Process32NextW(snap,&e));CloseHandle(snap);return found;
}
static bool ResetCommands() {
    const auto temp=runtime/L"ship-control.remoteplay.tmp";
    std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f) return false;
    f<<"counter="<<GetTickCount64()<<"\nqa_focus=0\nmutate=0\n";f.close();
    return !f.fail()&&MoveFileExW(temp.c_str(),(runtime/L"ship-control.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
static int Host() {
    const auto problem=Validate();if(!problem.empty()) {Error(problem);return 2;}
    if(!DonorIdentity()) {Error(L"Launch Spacewar from Steam after applying the displayed launch option. This shortcut only opens setup; Steam supplies the Remote Play session.");return 3;}
    HANDLE lock=CreateFileW((runtime/L"steam-coop.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE) {Error(L"A Steam co-op wrapper is already running.");return 4;}
    if(AlreadyRunning()) {CloseHandle(lock);Error(L"Close the existing isolated Nova Drift session before starting Steam co-op.");return 5;}
    if(!ResetCommands()) {CloseHandle(lock);Error(L"Could not reset the isolated QA command file.");return 6;}
    auto command=HostCommand();STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
    const bool started=CreateProcessW((runtime/L"novacoop_launch.exe").c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,runtime.c_str(),&si,&pi)!=FALSE;
    if(!started) {CloseHandle(lock);Error(L"The isolated launcher could not start. Windows error "+std::to_wstring(GetLastError()));return 7;}
    CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);DWORD result=1;GetExitCodeProcess(pi.hProcess,&result);CloseHandle(pi.hProcess);CloseHandle(lock);
    if(result) Error(L"The isolated launcher exited with code "+std::to_wstring(result)+L". See the co-op runtime logs.");
    return static_cast<int>(result);
}
static bool Copy(HWND hwnd) {
    const auto line=LaunchOption();const SIZE_T bytes=(line.size()+1)*sizeof(wchar_t);
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!data) return false;
    void* out=GlobalLock(data);if(!out) {GlobalFree(data);return false;}CopyMemory(out,line.c_str(),bytes);GlobalUnlock(data);
    if(!OpenClipboard(hwnd)) {GlobalFree(data);return false;}EmptyClipboard();
    bool ok=SetClipboardData(CF_UNICODETEXT,data)!=nullptr;CloseClipboard();if(!ok) GlobalFree(data);return ok;
}
static LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_CREATE) {
        const std::wstring body=L"Experimental Steam Remote Play Together setup\r\n\r\n"
            L"Use the installed Spacewar as the Steam launch entry.\r\n"
            L"1. Steam Library > Spacewar > Properties > General.\r\n"
            L"2. Preserve any existing Launch Options, then paste this entire line:\r\n";
        CreateWindowW(L"STATIC",body.c_str(),WS_CHILD|WS_VISIBLE,20,18,730,127,hwnd,nullptr,nullptr,nullptr);
        launch_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",LaunchOption().c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_READONLY|ES_AUTOHSCROLL,20,145,730,30,hwnd,nullptr,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"Copy launch option",WS_CHILD|WS_VISIBLE|WS_TABSTOP,20,190,185,32,hwnd,(HMENU)1,nullptr,nullptr);
        const std::wstring help=L"3. Enable Steam Overlay and Remote Play in Steam Settings.\r\n"
            L"4. Start Spacewar from Steam. The isolated Nova Drift copy should open.\r\n"
            L"5. Shift+Tab > Friends > Remote Play Together; choose your friend.\r\n"
            L"6. Give the guest keyboard access; turn their mouse access OFF.\r\n\r\n"
            L"Host: mouse aims, LEFT click thrusts, RIGHT click fires.\r\n"
            L"Guest: I/J/K/L steer, N fires; F6 opens the P2 control editor.\r\n\r\n"
            L"Steam will show Spacewar as the played game.\r\n"
            L"This window changes no Steam settings.\r\n"
            L"To undo: restore Spacewar's previous Launch Options (blank if none).";
        CreateWindowW(L"STATIC",help.c_str(),WS_CHILD|WS_VISIBLE,20,240,730,250,hwnd,nullptr,nullptr,nullptr);
        CreateWindowW(L"BUTTON",L"Close",WS_CHILD|WS_VISIBLE|WS_TABSTOP,635,490,115,32,hwnd,(HMENU)2,nullptr,nullptr);
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
        return 0;
    }
    if(message==WM_COMMAND) {if(LOWORD(wp)==1) {if(Copy(hwnd)) SetWindowTextW(hwnd,L"Nova Drift Steam co-op — launch option copied");else Error(L"Clipboard unavailable; select and copy the displayed line manually.");}if(LOWORD(wp)==2) DestroyWindow(hwnd);return 0;}
    if(message==WM_DESTROY) {PostQuitMessage(0);return 0;}return DefWindowProcW(hwnd,message,wp,lp);
}
static HWND Setup() {
    WNDCLASSW c{};c.lpfnWndProc=Proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"NovaDriftSteamSetup";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&c);
    HWND hwnd=CreateWindowW(c.lpszClassName,L"Nova Drift Steam co-op — setup",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,790,575,nullptr,nullptr,c.hInstance,nullptr);
    if(hwnd) {ShowWindow(hwnd,SW_SHOWNOACTIVATE);UpdateWindow(hwnd);}return hwnd;
}
static bool SaveOwnWindow(HWND hwnd,const fs::path& output) {
    RECT r{};GetClientRect(hwnd,&r);HDC dc=GetDC(hwnd),memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);HGDIOBJ old=SelectObject(memory,bitmap);
    const bool rendered=PrintWindow(hwnd,memory,PW_CLIENTONLY)!=FALSE;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    std::vector<char> pixels(static_cast<size_t>(r.right)*r.bottom*4);SelectObject(memory,old);
    bool copied=GetDIBits(dc,bitmap,0,r.bottom,pixels.data(),&info,DIB_RGB_COLORS)!=0;
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
    std::ofstream f(output,std::ios::binary);f.write(reinterpret_cast<char*>(&header),sizeof(header));f.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));f.write(pixels.data(),pixels.size());
    DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(hwnd,dc);return rendered&&copied&&f.good();
}
static int SelfTest(const fs::path& dir) {
    fs::create_directories(dir);const auto line=HostCommand();int argc=0;LPWSTR* argv=CommandLineToArgvW(line.c_str(),&argc);
    bool arguments=argv&&argc==6&&std::wstring(argv[1])==(runtime/L"NovaDrift.exe").wstring()&&std::wstring(argv[4])==L"0"&&std::wstring(argv[5])==L"fullscreen";if(argv) LocalFree(argv);
    bool quote=Quote(L"a\\\"b\\")==L"\"a\\\\\\\"b\\\\\"";
    DWORD clipboard=GetClipboardSequenceNumber();HWND hwnd=Setup();bool render=hwnd&&SaveOwnWindow(hwnd,dir/L"steam-coop-setup.bmp");
    bool controls=hwnd&&GetDlgItem(hwnd,1)&&GetDlgItem(hwnd,2)&&GetWindowTextLengthW(launch_edit)==static_cast<int>(LaunchOption().size());
    bool unchanged=GetClipboardSequenceNumber()==clipboard;if(hwnd) DestroyWindow(hwnd);
    std::ofstream report(dir/L"self-test.json");report<<"{\n  \"argument_roundtrip\": "<<(arguments?"true":"false")<<",\n  \"quote_edge_case\": "<<(quote?"true":"false")<<",\n  \"setup_controls\": "<<(controls?"true":"false")<<",\n  \"own_window_rendered\": "<<(render?"true":"false")<<",\n  \"clipboard_unchanged\": "<<(unchanged?"true":"false")<<",\n  \"game_launched\": false,\n  \"steam_configuration_written\": false\n}\n";
    return arguments&&quote&&controls&&render&&unchanged?0:10;
}
int wmain(int argc,wchar_t** argv) {
    wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);runtime=fs::path(self).parent_path();
    std::wstring mode=L"--setup";fs::path output;
    // --host terminates parsing: every trailing token belongs to Steam's %command% and is ignored.
    for(int i=1;i<argc;++i) {std::wstring arg=argv[i];if(arg==L"--host") {mode=arg;break;}else if(arg==L"--runtime"&&i+1<argc) runtime=argv[++i];else if(arg==L"--self-test"&&i+1<argc) {mode=arg;output=argv[++i];}else if(arg==L"--setup") mode=arg;else return 2;}
    runtime=fs::absolute(runtime).lexically_normal();
    if(mode==L"--self-test") return SelfTest(output);
    if(mode==L"--host") return Host();
    HWND hwnd=Setup();if(!hwnd) return 2;MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0) {if(!IsDialogMessageW(hwnd,&msg)) {TranslateMessage(&msg);DispatchMessageW(&msg);}}return 0;
}
