#pragma once
// Two-player GUI for the pinned Nova Drift co-op build. No hooks or game state
// mutations: caller supplies live per-player state and invokes Draw in DrawGUI.
// Native health/shield/status artwork is composed separately by the caller.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>
#include <YYToolkit/YYTK_Shared.hpp>

namespace ndship::hud {
inline constexpr int Color(unsigned r,unsigned g,unsigned b) noexcept {
    return static_cast<int>(r|(g<<8)|(b<<16)); // GameMaker stores BGR integers
}
inline constexpr int PlayerRed=Color(246,91,105),PlayerGold=Color(255,214,64);
inline constexpr int PlayerColor(int owner) noexcept { return owner==2?PlayerGold:PlayerRed; }
struct PlayerState {
    bool joined=false,alive=false;
    double hull=0,hull_max_effective=0,shield=0,shield_max_effective=0;
    double choices=0;
};
struct State {
    bool enabled=false;
    std::array<PlayerState,2> players{};
    bool upgrade_menu=false;
    int upgrade_owner=1;
    std::string menu_hint; // actual configured profile controls
    std::string gameplay_hint; // optional actual profile help, below top-right gear
    // Zero queries current GUI dimensions. Layout is designed for2400x1350.
    double width=0,height=0;
    std::array<double,2> life_x{},life_y{},life_width{},stock_y{};
    std::array<double,2> hint_alpha{}; // brief join/redeploy identity, supplied by caller
    double controls_alpha=0,upgrade_hint_alpha=0; // caller owns timing/fades
    int font=-1; // built-in default font; caller may supply a suitable font asset
};
struct Layout {
    double scale,margin,panel_width,panel_height,panel_y,right_x;
};
inline Layout ComputeLayout(double width,double height) noexcept {
    const double scale=std::clamp(std::min(width/2400.0,height/1350.0),0.35,3.0);
    const double margin=48*scale,panel_width=std::min(650*scale,(width-3*margin)/2);
    return {scale,margin,panel_width,194*scale,height-42*scale-194*scale,width-margin-panel_width};
}
inline double Ratio(double value,double maximum) noexcept {
    if(!std::isfinite(value)||!std::isfinite(maximum)||maximum<=0)return 0;
    return std::clamp(value/maximum,0.0,1.0);
}
inline double Opacity(double alpha) noexcept {
    return std::isfinite(alpha)?std::clamp(alpha,0.0,1.0):0.0;
}
namespace detail {
using YYTK::RValue;
class Painter {
    YYTK::YYTKInterface* api_;
    YYTK::CInstance* self_;
    bool ok_=true;
    int layout_font_=-1;
public:
    Painter(YYTK::YYTKInterface* api,YYTK::CInstance* self):api_(api),self_(self){}
    RValue Call(const char* name,std::initializer_list<RValue> args={}) {
        RValue out;
        if(!Aurie::AurieSuccess(api_->CallBuiltinEx(out,name,self_,self_,std::vector<RValue>(args))))ok_=false;
        return out;
    }
    bool Ok()const noexcept{return ok_;}
    void SetLayoutFont(int font)noexcept{layout_font_=font;}
    double TextWidth(const std::string& text) {
        struct Entry{bool valid=false;int font=-1;std::string text;double width=0;};
        static std::array<Entry,128> cache{};static std::size_t next=0;
        for(const auto& item:cache)
            if(item.valid&&item.font==layout_font_&&item.text==text)return item.width;
        const double width=Call("string_width",{RValue(text)}).ToDouble();
        auto& item=cache[next++%cache.size()];item.valid=true;item.font=layout_font_;item.text=text;item.width=width;
        return width;
    }
    const std::vector<std::string>& Wrap(const std::string& text,double width,double scale) {
        struct Entry{bool valid=false;int font=-1;std::string text;double width=0,scale=0;std::vector<std::string> lines;};
        static std::array<Entry,8> cache{};static std::size_t next=0;
        for(const auto& item:cache)
            if(item.valid&&item.font==layout_font_&&item.text==text&&item.width==width&&item.scale==scale)return item.lines;
        auto& result=cache[next++%cache.size()];result.valid=true;result.font=layout_font_;
        result.text=text;result.width=width;result.scale=scale;result.lines.clear();
        std::istringstream words(text);std::string word,line;
        while(words>>word) {
            const auto candidate=line.empty()?word:line+" "+word;
            if(!line.empty() && TextWidth(candidate)*scale>width) {
                result.lines.push_back(line);line=word;
            } else line=candidate;
        }
        if(!line.empty())result.lines.push_back(line);
        // Pre-cache the final lines too, so FittedText performs no engine
        // measurement on steady frames, even after candidate-cache rotation.
        for(const auto& value:result.lines)TextWidth(value);
        return result.lines;
    }
    void Rect(double x,double y,double w,double h,int color,double alpha=1,bool outline=false) {
        Call("draw_set_color",{RValue(color)});Call("draw_set_alpha",{RValue(alpha)});
        Call("draw_rectangle",{RValue(x),RValue(y),RValue(x+w),RValue(y+h),RValue(outline)});
    }
    void Text(double x,double y,const char* text,int color,double scale,int align=0,double alpha=1) {
        Call("draw_set_color",{RValue(color)});Call("draw_set_alpha",{RValue(Opacity(alpha))});
        Call("draw_set_halign",{RValue(align)});
        Call("draw_text_transformed",{RValue(x),RValue(y),RValue(text),RValue(scale),RValue(scale),RValue(0.0)});
    }
    void FittedText(double x,double y,const std::string& text,int color,double scale,double width,int align=0,double alpha=1) {
        const double raw=TextWidth(text);
        if(raw>0)scale=std::min(scale,width/raw);
        Text(x,y,text.c_str(),color,scale,align,alpha);
    }
};
inline double FontScale(const State& state,const Layout& layout) noexcept {
    // Native scaled fonts already incorporate guiScale. Only the built-in
    // fallback font needs explicit scaling with the GUI dimensions.
    return state.font>=0?1.0:2.0*layout.scale;
}
inline void Identity(Painter& p,const State& state,int owner,double x,double y,
                     double width,const Layout& layout) {
    const auto& player=state.players[owner-1];const double s=layout.scale,t=FontScale(state,layout);
    const int accent=PlayerColor(owner);
    if(player.joined) {
        // Persistent color identity sits immediately above the native bars.
        p.Call("draw_set_color",{RValue(accent)});p.Call("draw_set_alpha",{RValue(0.55)});
        p.Call("draw_line",{RValue(x),RValue(y+46*s),RValue(x+width),RValue(y+46*s)});
    }
    const double alpha=Opacity(state.hint_alpha[owner-1]);if(alpha<=0)return;
    const char* title=owner==2?"P2":"P1";
    if(!player.joined)title=owner==2?"P2  -  F8 TO JOIN":"P1";
    else if(!player.alive)title=owner==2?"P2  -  DOWN":"P1  -  DOWN";
    p.FittedText(x,state.stock_y[owner-1]>0?state.stock_y[owner-1]-110*s:y,title,accent,t,width,0,alpha);
}
inline void UpgradeBanner(Painter& p,const State& state,const Layout& layout,double width) {
    const int owner=state.upgrade_owner==2?2:1,accent=PlayerColor(owner);
    const double s=layout.scale,t=FontScale(state,layout),w=1000*s,x=(width-w)/2,y=36*s;
    // Color and native-style rules/chevrons persist; explanatory text fades.
    p.Call("draw_set_color",{RValue(accent)});p.Call("draw_set_alpha",{RValue(0.75)});
    p.Call("draw_line",{RValue(x+16*s),RValue(y+38*s),RValue(x+w-16*s),RValue(y+38*s)});
    for(int side=0;side<2;++side) {
        const double edge=side?x+w:x,tip=edge+(side?-12:12)*s;
        p.Call("draw_line",{RValue(edge),RValue(y+22*s),RValue(tip),RValue(y+38*s)});
        p.Call("draw_line",{RValue(tip),RValue(y+38*s),RValue(edge),RValue(y+54*s)});
    }
    const double alpha=Opacity(state.upgrade_hint_alpha);if(alpha<=0)return;
    p.FittedText(width/2,y,owner==2?"P2":"P1",accent,1.25*t,w,1,alpha);
    if(state.menu_hint.empty())return;
    const double hintWidth=650*s,hintX=width-48*s-hintWidth;
    const auto& lines=p.Wrap(state.menu_hint,hintWidth,0.85*t);
    for(std::size_t i=0;i<lines.size();++i)
        p.FittedText(hintX,(180+28*i)*s,lines[i],Color(235,235,235),0.85*t,hintWidth,0,alpha);
}
inline void GameplayHint(Painter& p,const State& state,const Layout& layout,double height) {
    const double alpha=Opacity(state.controls_alpha);if(alpha<=0)return;
    const double s=layout.scale,t=FontScale(state,layout),x=layout.right_x,w=layout.panel_width;
    const double hintx=state.life_width[1]>0?state.life_x[1]+state.life_width[1]:x+w;
    const double hinty=state.life_width[1]>0?state.stock_y[1]>0?state.stock_y[1]-110*s:state.life_y[1]-50*s:height-95*s;
    p.Text(hintx,hinty,"F6 CONTROLS",Color(225,225,225),0.65*t,2,alpha);
    if(state.upgrade_menu||state.gameplay_hint.empty())return;
    const auto& lines=p.Wrap(state.gameplay_hint,w,0.85*t);
    for(std::size_t i=0;i<lines.size();++i)
        p.FittedText(x,180*s+26*i*s,lines[i],Color(225,225,225),0.85*t,w,0,alpha);
}
} // namespace detail

inline bool Draw(YYTK::YYTKInterface* api,const State& state,YYTK::CInstance* self=nullptr) {
    if(!api||!state.enabled)return true;
    detail::Painter p(api,self);
    double width=state.width,height=state.height;
    if(width<=0)width=p.Call("display_get_gui_width").ToDouble();
    if(height<=0)height=p.Call("display_get_gui_height").ToDouble();
    if(!std::isfinite(width)||!std::isfinite(height)||width<320||height<200)return false;
    const auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha"),font=p.Call("draw_get_font"),
        horizontal=p.Call("draw_get_halign"),vertical=p.Call("draw_get_valign");
    // Abort before changing draw state if a required state query failed.
    if(!p.Ok())return false;
    p.Call("draw_set_font",{detail::RValue(state.font)});
    p.SetLayoutFont(state.font);
    p.Call("draw_set_valign",{detail::RValue(0)});
    const auto layout=ComputeLayout(width,height);
    for(int i=0;i<2;++i){
        const bool native=state.life_width[i]>0;
        detail::Identity(p,state,i+1,native?state.life_x[i]:(i?layout.right_x:layout.margin),native?state.life_y[i]-50*layout.scale:height-95*layout.scale,native?state.life_width[i]:layout.panel_width,layout);
    }
    detail::GameplayHint(p,state,layout,height);
    if(state.upgrade_menu)detail::UpgradeBanner(p,state,layout,width);
    p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});p.Call("draw_set_font",{font});
    p.Call("draw_set_halign",{horizontal});p.Call("draw_set_valign",{vertical});
    return p.Ok();
}

// These visual instances all draw their own sprite. Glow/Visibility Step2
// follow the owning player but do not copy its image_blend, so tinting603 alone
// leaves independent white layers covering the colored hull.
inline bool IsShipVisualAsset(int asset) noexcept {
    return asset==603||asset==611||asset==613||asset==616||asset==617||asset==618||asset==619;
}
inline bool TintShipVisual(YYTK::YYTKInterface* api,YYTK::CInstance* instance,int owner) {
    if(!api||!instance||(owner!=1&&owner!=2))return false;
    YYTK::RValue color(PlayerColor(owner));
    return Aurie::AurieSuccess(api->SetBuiltin("image_blend",instance,NULL_INDEX,color));
}
// Invoke once per ship in a WORLD Draw event, after its native draw. Do not
// call this from DrawGUI without applying the game's world-to-GUI transform.
inline bool DrawMarker(YYTK::YYTKInterface* api,int owner,double x,double y,
                       double radius=36,YYTK::CInstance* self=nullptr,double text_alpha=0) {
    if(!api||(owner!=1&&owner!=2)||!std::isfinite(x)||!std::isfinite(y)||
       !std::isfinite(radius)||radius<=0)return false;
    detail::Painter p(api,self);
    const auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha"),font=p.Call("draw_get_font"),
        horizontal=p.Call("draw_get_halign"),vertical=p.Call("draw_get_valign");
    if(!p.Ok())return false;
    p.Call("draw_set_color",{detail::RValue(PlayerColor(owner))});
    p.Call("draw_set_alpha",{detail::RValue(0.92)});
    p.Call("draw_circle",{detail::RValue(x),detail::RValue(y),detail::RValue(radius),detail::RValue(true)});
    p.Call("draw_set_alpha",{detail::RValue(0.3)});
    p.Call("draw_circle",{detail::RValue(x),detail::RValue(y),detail::RValue(radius+3),detail::RValue(true)});
    p.Call("draw_set_font",{detail::RValue(-1)});p.Call("draw_set_valign",{detail::RValue(2)});
    if(Opacity(text_alpha)>0)p.Text(x,y-radius-10,owner==2?"P2":"P1",PlayerColor(owner),1.6,1,text_alpha);
    p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});p.Call("draw_set_font",{font});
    p.Call("draw_set_halign",{horizontal});p.Call("draw_set_valign",{vertical});
    return p.Ok();
}

// Six native health/shield rectangle call sites in Controller Draw64, used
// to observe actual rendered bar widths during QA. The hook forwards every
// draw unchanged, including Bastion assembly (return98b68c). Native ABI:
// void(float x1,float y1,float x2,float y2,bool outline).
inline constexpr std::uintptr_t NativeDrawRectangleRva=0xf35810;
inline constexpr std::array<std::uintptr_t,6> NativeHealthRectReturns{{
    0x98a93b,0x98aa9d,0x98abcb,0x98af23,0x98b085,0x98b1b3
}};
inline bool IsNativeHealthRectReturnRva(std::uintptr_t rva) noexcept {
    for(auto address:NativeHealthRectReturns)if(address==rva)return true;
    return false;
}
} // namespace ndship::hud
