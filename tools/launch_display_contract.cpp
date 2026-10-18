#define NOVACOOP_LAUNCH_TEST
#include "../src/launch.cpp"
#include <fstream>
#include <filesystem>
static unsigned checks=0;
static void check(bool ok,const char* name){++checks;if(!ok){printf("FAIL %s\n",name);exit(1);}printf("PASS %s\n",name);}
static LaunchOptions options(std::initializer_list<const wchar_t*> args,bool accepted=true){
 std::vector<wchar_t*> words;for(auto p:args)words.push_back(const_cast<wchar_t*>(p));LaunchOptions result;
 check(launch_options(int(words.size()),words.data(),result)==accepted,"parse options");return result;
}
static std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int wmain(int argc,wchar_t** argv){
 check(argc==2,"synthetic profile root supplied");
 auto normal=options({L"launch",L"game",L"dll",L"root",L"0"});
 check(normal.seconds==0&&normal.display==DisplayMode::Saved,"bare interactive preserves native display preference");
 check(game_command(L"C:\\game.exe",L"C:\\runtime",normal).find(L"-inawindow")==std::wstring::npos,"interactive does not force a window");
 auto qa=options({L"launch",L"game",L"dll",L"root",L"180",L"debug",L"silent"});
 auto command=game_command(L"C:\\Space Game\\game.exe",L"C:\\test runtime",qa);
 check(qa.seconds==180&&qa.debugging&&qa.silent&&command.find(L"-inawindow")!=std::wstring::npos&&command.find(L"-noaudio")!=std::wstring::npos,"bounded QA retains window/debug/silent");
 check(command.find(L"\"C:\\Space Game\\game.exe\"")==0&&command.find(L"-debugoutput \"C:\\test runtime\\runner.log\"")!=std::wstring::npos,"command preserves quoted executable and log paths");
 auto fullscreen=options({L"launch",L"game",L"dll",L"root",L"0",L"fullscreen"});
 check(fullscreen.display==DisplayMode::Fullscreen&&game_command(L"game",L"root",fullscreen).find(L"-inawindow")==std::wstring::npos,"explicit fullscreen omits window flag");
 auto windowed=options({L"launch",L"game",L"dll",L"root",L"0",L"windowed"});
 check(windowed.display==DisplayMode::Windowed&&game_command(L"game",L"root",windowed).find(L"-inawindow")!=std::wstring::npos,"explicit windowed forces window flag");
 options({L"launch",L"game",L"dll",L"root",L"0",L"fullscreen",L"windowed"},false);
 options({L"launch",L"game",L"dll",L"root",L"-1"},false);
 options({L"launch",L"game",L"dll",L"root",L"99999999999999999"},false);
 options({L"launch",L"game",L"dll",L"root",L"0",L"unrecognized"},false);
 const std::string before=";head\r\n[score]\r\nwindowedMode=77\r\nopaque=unchanged\r\n[settings]\r\n  windowedMode = \"1.000000\" ;note\r\nborderlessWindow=\"1.000000\"\r\nresolution=1280\r\n[audio]\r\nvolume=0.8";
 const std::string after=";head\r\n[score]\r\nwindowedMode=77\r\nopaque=unchanged\r\n[settings]\r\n  windowedMode = \"0.000000\" ;note\r\nborderlessWindow=\"0.000000\"\r\nresolution=1280\r\n[audio]\r\nvolume=0.8";
 std::string updated;check(display_preferences(before,DisplayMode::Fullscreen,updated)&&updated==after,"fullscreen changes only two settings values byte-for-byte");
 check(display_preferences(before,DisplayMode::Saved,updated)&&updated==before,"implicit mode leaves bytes untouched");
 check(display_preferences(after,DisplayMode::Windowed,updated)&&updated.find("windowedMode = \"1.000000\"")!=std::string::npos&&updated.find("borderlessWindow=\"0.000000\"")!=std::string::npos,"windowed remains bordered instead of borderless");
 check(display_preferences("[settings]\nresolution=1280\n[next]\nkeep=yes\n",DisplayMode::Fullscreen,updated)&&updated=="[settings]\nresolution=1280\nwindowedMode=\"0.000000\"\nborderlessWindow=\"0.000000\"\n[next]\nkeep=yes\n","missing mode keys inserted only within native settings section");
 check(display_preferences("[score]\nkeep=yes",DisplayMode::Fullscreen,updated)&&updated=="[score]\nkeep=yes\n[settings]\nwindowedMode=\"0.000000\"\nborderlessWindow=\"0.000000\"\n","missing settings section appended safely");
 check(display_preferences("\xef\xbb\xbf[SeTTings]\r\nwindowedMode=1  ;keep\r\nborderlessWindow=0",DisplayMode::Fullscreen,updated)&&updated=="\xef\xbb\xbf[SeTTings]\r\nwindowedMode=\"0.000000\"  ;keep\r\nborderlessWindow=\"0.000000\"","BOM/mixed-case section/comments and missing final newline preserved");
 check(!display_preferences(std::string("x\0y",3),DisplayMode::Fullscreen,updated),"unsupported encoding refuses rewrite");
 check(!display_preferences("[settings]\nwindowedMode=\"broken\n",DisplayMode::Fullscreen,updated),"malformed target value refuses rewrite");
 const auto profile=std::filesystem::path(argv[1]);
 check(profile.filename()==L"synthetic-profile","file tests restricted to supplied synthetic profile");
 const auto save=profile/L"Local"/L"NovaDrift"/L"save.ini";
 std::filesystem::create_directories(save.parent_path());{std::ofstream f(save,std::ios::binary);f<<before;}
 check(apply_display_preferences(profile.wstring(),DisplayMode::Fullscreen),"atomic write succeeds on synthetic profile");
 check(read(save)==after&&read(save.wstring()+L".display-mode.bak")==before,"atomic file result and original backup verified");
 auto stamp=std::filesystem::last_write_time(save);
 check(apply_display_preferences(profile.wstring(),DisplayMode::Fullscreen)&&std::filesystem::last_write_time(save)==stamp,"unchanged explicit preference does not rewrite file");
 check(apply_display_preferences(profile.wstring(),DisplayMode::Saved)&&read(save)==after,"saved mode never writes profile");
 check(apply_display_preferences(profile.wstring(),DisplayMode::Windowed)&&read(save).find("windowedMode = \"1.000000\"")!=std::string::npos,"explicit windowed profile applies");
 for(auto& p:std::filesystem::directory_iterator(save.parent_path()))check(p.path().extension()!=L".tmp","no unfinished temporary preference file");
 printf("PASS %u launcher command/preference checks; no game process launched\n",checks);return 0;
}
