#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include "ship_config.hpp"
namespace cfg=ndship::config;
namespace {
constexpr int Mode=100,Device=101,Slot=102,Deadzone=103,Player=104,MouseAim=105,KeyBase=1000,PadBase=2000,Defaults=3001,Save=3002,Cancel=3003;
struct WindowState {
    cfg::Profile profile;
    std::array<cfg::Profile,3> profiles;std::array<std::filesystem::path,3> paths;int player=2;
    std::filesystem::path path;
    HWND window=nullptr,mode=nullptr,device=nullptr,slot=nullptr,deadzone=nullptr,status=nullptr,description=nullptr,player_select=nullptr,mouse_aim=nullptr;
    std::array<HWND,cfg::ActionCount> keys{},pads{};
    HFONT font=nullptr; int capturing=-1; bool saved=false;
} ui;
std::wstring Wide(const std::string& text) { return std::wstring(text.begin(),text.end()); }
std::wstring KeyName(int key) {
    if(key<0)return L"Unbound";
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return std::wstring(1,static_cast<wchar_t>(key));
    switch(key) {
        case VK_LBUTTON:return L"Mouse left";case VK_RBUTTON:return L"Mouse right";case VK_MBUTTON:return L"Mouse middle";
        case VK_XBUTTON1:return L"Mouse back";case VK_XBUTTON2:return L"Mouse forward";
        case VK_RCONTROL:return L"Right Ctrl";case VK_LCONTROL:return L"Left Ctrl";
        case VK_RSHIFT:return L"Right Shift";case VK_LSHIFT:return L"Left Shift";
        case VK_RMENU:return L"Right Alt";case VK_LMENU:return L"Left Alt";
    }
    UINT scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);LONG flags=static_cast<LONG>(scan<<16);
    if(key==VK_LEFT||key==VK_RIGHT||key==VK_UP||key==VK_DOWN||key==VK_DELETE||key==VK_INSERT||key==VK_HOME||key==VK_END||key==VK_PRIOR||key==VK_NEXT||key==VK_DIVIDE)flags|=1<<24;
    wchar_t name[96]{};if(GetKeyNameTextW(flags,name,96)>0)return name;
    return L"Key "+std::to_wstring(key);
}
HWND Add(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int width,int height,int id=0) {
    auto h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,ui.window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(ui.font),TRUE);return h;
}
void Item(HWND combo,const wchar_t* text) { SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text)); }
void SetStatus(const std::wstring& text) { SetWindowTextW(ui.status,text.c_str()); }
void Description() {
    const int mode=static_cast<int>(SendMessageW(ui.mode,CB_GETCURSEL,0,0));
    SetWindowTextW(ui.description,mode==0?L"Directional: movement keys / left stick choose a heading. The ship turns and accelerates using its normal physics.":L"Tank: Up thrusts, Left / Right turn, Down brakes. Your build determines which special actions are available.");
}
void Populate() {
    SendMessageW(ui.mode,CB_SETCURSEL,static_cast<int>(ui.profile.steering),0);
    SendMessageW(ui.device,CB_SETCURSEL,static_cast<int>(ui.profile.device),0);
    SendMessageW(ui.slot,CB_SETCURSEL,ui.profile.controller_slot,0);
    wchar_t zone[32];std::swprintf(zone,32,L"%.2f",ui.profile.deadzone);SetWindowTextW(ui.deadzone,zone);
    for(std::size_t i=0;i<cfg::ActionCount;++i) {
        SetWindowTextW(ui.keys[i],KeyName(ui.profile.keys[i]).c_str());
        int selected=0;
        for(std::size_t j=0;j<sizeof(cfg::PadChoices)/sizeof(cfg::PadChoices[0]);++j)if(cfg::PadChoices[j].mask==ui.profile.pad[i])selected=static_cast<int>(j);
        SendMessageW(ui.pads[i],CB_SETCURSEL,selected,0);
    }
    SendMessageW(ui.mouse_aim,BM_SETCHECK,ui.profile.mouse_aim?BST_CHECKED:BST_UNCHECKED,0);
    EnableWindow(ui.mouse_aim,ui.player==1);
    ui.capturing=-1;Description();
}
bool ReadForm(std::string& error) {
    cfg::Profile next=ui.profile;
    next.mouse_aim=ui.player==1&&SendMessageW(ui.mouse_aim,BM_GETCHECK,0,0)==BST_CHECKED;
    next.steering=static_cast<cfg::Steering>(SendMessageW(ui.mode,CB_GETCURSEL,0,0));
    next.device=static_cast<cfg::Device>(SendMessageW(ui.device,CB_GETCURSEL,0,0));
    next.controller_slot=static_cast<int>(SendMessageW(ui.slot,CB_GETCURSEL,0,0));
    wchar_t text[64];GetWindowTextW(ui.deadzone,text,64);
    try {std::size_t used=0;const std::wstring value=text;next.deadzone=std::stod(value,&used);if(used!=value.size())throw 0;}
    catch(...){error="Enter a deadzone between 0.05 and 0.80.";return false;}
    for(std::size_t i=0;i<cfg::ActionCount;++i) {
        const auto choice=SendMessageW(ui.pads[i],CB_GETCURSEL,0,0);
        if(choice<0||static_cast<std::size_t>(choice)>=sizeof(cfg::PadChoices)/sizeof(cfg::PadChoices[0])){error="Select a controller binding.";return false;}
        next.pad[i]=cfg::PadChoices[choice].mask;
    }
    if(!cfg::Validate(next,error))return false;
    ui.profile=next;return true;
}
void FinishCapture(int key) {
    if(ui.capturing<0)return;
    const int action=ui.capturing;
    if(key!=VK_ESCAPE)ui.profile.keys[action]=key==VK_BACK?-1:key;
    ui.capturing=-1;SetWindowTextW(ui.keys[action],KeyName(ui.profile.keys[action]).c_str());
    SetStatus(L"Bindings changed. Save to apply them to this player.");
}
int PhysicalKey(WPARAM w,LPARAM l) {
    int key=static_cast<int>(w);
    if(key==VK_CONTROL)return (l&(1<<24))?VK_RCONTROL:VK_LCONTROL;
    if(key==VK_MENU)return (l&(1<<24))?VK_RMENU:VK_LMENU;
    if(key==VK_SHIFT)return static_cast<int>(MapVirtualKeyW((static_cast<UINT>(l)>>16)&255,MAPVK_VSC_TO_VK_EX));
    return key;
}
LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_CREATE) {
        ui.window=window;ui.font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        Add(L"STATIC",L"Controls for",0,20,16,100,26);
        ui.player_select=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,125,12,225,150,Player);
        Item(ui.player_select,L"Player 1");Item(ui.player_select,L"Player 2");SendMessageW(ui.player_select,CB_SETCURSEL,1,0);
        ui.mouse_aim=Add(L"BUTTON",L"Aim with mouse (Player 1)",BS_AUTOCHECKBOX|WS_TABSTOP,480,14,300,25,MouseAim);
        Add(L"STATIC",L"Steering",0,20,57,120,22);ui.mode=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,150,53,290,200,Mode);
        Item(ui.mode,L"Directional (keys / stick heading)");Item(ui.mode,L"Tank (thrust and turn)");
        Add(L"STATIC",L"Input device",0,480,57,120,22);ui.device=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,600,53,300,180,Device);
        Item(ui.device,L"Keyboard / mouse");Item(ui.device,L"Controller");Item(ui.device,L"Keyboard + controller");
        Add(L"STATIC",L"Controller",0,20,96,120,22);ui.slot=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,150,91,290,200,Slot);
        Item(ui.slot,L"Controller 1 (XInput slot 0)");Item(ui.slot,L"Controller 2 (XInput slot 1)");Item(ui.slot,L"Controller 3 (XInput slot 2)");Item(ui.slot,L"Controller 4 (XInput slot 3)");
        Add(L"STATIC",L"Stick deadzone",0,480,96,120,22);ui.deadzone=Add(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,600,91,90,26,Deadzone);
        Add(L"STATIC",L"0.05 to 0.80",0,705,96,170,22);
        ui.description=Add(L"STATIC",L"",0,20,135,880,42);
        for(int column=0;column<2;++column) {
            const int x=20+460*column;
            Add(L"STATIC",L"Action",0,x,184,150,24);Add(L"STATIC",L"Keyboard / mouse",0,x+155,184,120,24);Add(L"STATIC",L"Controller",0,x+280,184,145,24);
        }
        for(std::size_t i=0;i<cfg::ActionCount;++i) {
            const int column=static_cast<int>(i/10),row=static_cast<int>(i%10),x=20+460*column,y=215+38*row;
            Add(L"STATIC",cfg::ActionLabels[i],0,x,y+5,153,25);
            ui.keys[i]=Add(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,x+155,y,118,28,KeyBase+static_cast<int>(i));
            ui.pads[i]=Add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,x+280,y,145,300,PadBase+static_cast<int>(i));
            for(const auto& choice:cfg::PadChoices)Item(ui.pads[i],choice.label);
        }
        Add(L"STATIC",L"Click a key to change it. Backspace clears; Escape cancels. Menus: move keys / stick navigate; Fire selects.\nStabilize goes back, Deploy rerolls, Construct 2 banishes. Left stick supplies movement when controller input is enabled.",0,20,603,880,40);
        ui.status=Add(L"STATIC",ui.path.c_str(),SS_PATHELLIPSIS,20,640,880,25);
        Add(L"BUTTON",L"Restore defaults",BS_PUSHBUTTON|WS_TABSTOP,20,680,160,32,Defaults);
        Add(L"BUTTON",L"Save",BS_DEFPUSHBUTTON|WS_TABSTOP,666,680,110,32,Save);
        Add(L"BUTTON",L"Close",BS_PUSHBUTTON|WS_TABSTOP,790,680,110,32,Cancel);
        Populate();return 0;
    }
    if(message==WM_COMMAND) {
        const int id=LOWORD(w);
        if(id>=KeyBase&&id<KeyBase+static_cast<int>(cfg::ActionCount)&&HIWORD(w)==BN_CLICKED) {
            if(ui.capturing>=0)FinishCapture(VK_ESCAPE);
            ui.capturing=id-KeyBase;SetWindowTextW(ui.keys[ui.capturing],L"Press a key...");
            SetStatus(L"Press the new key. Backspace unbinds; Escape cancels.");SetFocus(window);return 0;
        }
        if(id==Player&&HIWORD(w)==CBN_SELCHANGE){
            std::string error;if(!ReadForm(error)){SetStatus(Wide(error));SendMessageW(ui.player_select,CB_SETCURSEL,ui.player-1,0);return 0;}
            ui.profiles[ui.player]=ui.profile;ui.player=(int)SendMessageW(ui.player_select,CB_GETCURSEL,0,0)+1;
            ui.profile=ui.profiles[ui.player];ui.path=ui.paths[ui.player];Populate();SetStatus(L"Save applies the selected player's bindings.");return 0;
        }
        if(id==Device&&HIWORD(w)==CBN_SELCHANGE&&ui.player==1&&SendMessageW(ui.device,CB_GETCURSEL,0,0)==1){SendMessageW(ui.mouse_aim,BM_SETCHECK,BST_UNCHECKED,0);return 0;}
        if(id==Mode&&HIWORD(w)==CBN_SELCHANGE){Description();return 0;}
        if(id==Defaults){ui.profile=ui.player==1?cfg::PlayerOneDefaults():cfg::Profile{};Populate();SetStatus(L"Defaults restored. Save to apply them.");return 0;}
        if(id==Save) {
            if(ui.capturing>=0){SetStatus(L"Finish or cancel key capture before saving.");return 0;}
            std::string error;
            if(!ReadForm(error)||!cfg::Save(ui.path,ui.profile,error)){MessageBoxW(window,Wide(error).c_str(),L"Co-op controls",MB_OK|MB_ICONERROR);return 0;}
            ui.saved=true;SetStatus(L"Saved. The game reloads this player's controls while running.");return 0;
        }
        if(id==Cancel){DestroyWindow(window);return 0;}
    }
    if((message==WM_KEYDOWN||message==WM_SYSKEYDOWN)&&ui.capturing>=0){FinishCapture(PhysicalKey(w,l));return 0;}
    if(ui.capturing>=0) {
        if(message==WM_LBUTTONDOWN){FinishCapture(VK_LBUTTON);return 0;}
        if(message==WM_RBUTTONDOWN){FinishCapture(VK_RBUTTON);return 0;}
        if(message==WM_MBUTTONDOWN){FinishCapture(VK_MBUTTON);return 0;}
        if(message==WM_XBUTTONDOWN){FinishCapture(HIWORD(w)==XBUTTON1?VK_XBUTTON1:VK_XBUTTON2);return TRUE;}
    }
    if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){if(ui.font)DeleteObject(ui.font);PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
int wmain(int argc,wchar_t** argv) {
    SetProcessDPIAware();wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    ui.path=std::filesystem::path(executable).parent_path()/cfg::FileName;
    bool check=false,defaults=false,self_test=false;
    for(int i=1;i<argc;++i) {
        const std::wstring arg=argv[i];
        if((arg==L"--config"||arg==L"--check"||arg==L"--write-defaults"||arg==L"--self-test")&&i+1<argc) {
            check=arg==L"--check";defaults=arg==L"--write-defaults";self_test=arg==L"--self-test";ui.path=argv[++i];
        } else return 2;
    }
    std::string error;
    if(defaults)return cfg::Save(ui.path,cfg::Profile{},error)?0:2;
    if(check)return cfg::Load(ui.path,ui.profile,error)?0:2;
    if(std::filesystem::exists(ui.path)&&!cfg::Load(ui.path,ui.profile,error)&&!self_test)
        MessageBoxW(nullptr,(L"The saved profile could not be read. Defaults are shown.\n\n"+Wide(error)).c_str(),L"Co-op controls",MB_OK|MB_ICONWARNING);
    ui.paths[2]=ui.path;ui.paths[1]=ui.path.parent_path()/cfg::PlayerOneFileName;
    ui.profiles[2]=ui.profile;ui.profiles[1]=cfg::PlayerOneDefaults();
    if(std::filesystem::exists(ui.paths[1])){std::string issue;if(!cfg::Load(ui.paths[1],ui.profiles[1],issue)){MessageBoxW(nullptr,Wide(issue).c_str(),L"Player 1 controls",MB_OK|MB_ICONERROR);return 2;}}
    WNDCLASSW cls{};cls.lpfnWndProc=Procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);cls.lpszClassName=L"NovaDriftP2Controls";
    if(!RegisterClassW(&cls))return 3;
    RECT bounds{0,0,920,735};AdjustWindowRect(&bounds,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
    HWND window=CreateWindowW(cls.lpszClassName,L"Nova Drift - Co-op Controls",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,cls.hInstance,nullptr);
    if(!window)return 3;
    if(self_test) {
        // Exercise the real dialog control path on an explicit test profile.
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(KeyBase+static_cast<int>(cfg::Action::Fire),BN_CLICKED),0);
        SendMessageW(window,WM_KEYDOWN,'P',0);
        SendMessageW(ui.mode,CB_SETCURSEL,1,0);SendMessageW(ui.device,CB_SETCURSEL,2,0);SendMessageW(ui.slot,CB_SETCURSEL,3,0);SetWindowTextW(ui.deadzone,L"0.35");
        SendMessageW(ui.pads[static_cast<std::size_t>(cfg::Action::Fire)],CB_SETCURSEL,2,0); // B
        if(!ReadForm(error)||!cfg::Save(ui.path,ui.profile,error)){DestroyWindow(window);return 4;}
        cfg::Profile loaded;
        bool ok=cfg::Load(ui.path,loaded,error)&&loaded.Key(cfg::Action::Fire)=='P'&&loaded.steering==cfg::Steering::Tank&&loaded.device==cfg::Device::Both&&loaded.controller_slot==3&&loaded.deadzone==0.35&&loaded.Pad(cfg::Action::Fire)==0x2000;
        SendMessageW(ui.player_select,CB_SETCURSEL,0,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Player,CBN_SELCHANGE),0);
        SendMessageW(window,WM_COMMAND,MAKEWPARAM(KeyBase+static_cast<int>(cfg::Action::Fire),BN_CLICKED),0);SendMessageW(window,WM_KEYDOWN,'B',0);
        if(!ReadForm(error)||!cfg::Save(ui.path,ui.profile,error)){DestroyWindow(window);return 4;}
        cfg::Profile first;ok=ok&&cfg::Load(ui.paths[1],first,error)&&first.Key(cfg::Action::Fire)=='B'&&first.mouse_aim&&first.controller_slot==0;
        DestroyWindow(window);return ok?0:4;
    }
    ShowWindow(window,SW_SHOW);UpdateWindow(window);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) {
        if(ui.capturing>=0||!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
    return 0;
}
