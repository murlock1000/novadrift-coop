#pragma once
// Native GUI capture/composition. Call only from the game's Draw GUI thread.
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>
#include <YYToolkit/YYTK_Shared.hpp>

namespace ndship::surface_hud {
struct Rect { double x=0,y=0,w=0,h=0; };
struct Result {
    bool ok=false;
    std::array<Rect,3> dest{},source{},stock_source{},stock_dest{}; // owner indices 1 and 2
    std::array<int,3> surface{{-1,-1,-1}};
};
class NativeSurfaceRenderer {
    using RV=YYTK::RValue;
    enum Fn : unsigned { Exists,Create,Free,Target,Reset,Clear,Part,SaveSurface,
        Push,Pop,Blend,GetScissor,SetScissor,MatrixGet,MatrixSet,GetColor,SetColor,GetAlpha,SetAlpha,
        GetFont,SetFont,GetHalign,SetHalign,GetValign,SetValign,Count };
    static constexpr const char* Names[Count]={
        "surface_exists","surface_create","surface_free","surface_set_target",
        "surface_reset_target","draw_clear_alpha","draw_surface_part_ext","surface_save",
        "gpu_push_state","gpu_pop_state","gpu_set_blendmode_ext_sepalpha",
        "gpu_get_scissor","gpu_set_scissor",
        "matrix_get","matrix_set","draw_get_color","draw_set_color",
        "draw_get_alpha","draw_set_alpha","draw_get_font","draw_set_font",
        "draw_get_halign","draw_set_halign","draw_get_valign","draw_set_valign"};
    // Pinned runner gpu_set_blendmode_ext_sepalpha RVA110a940 passes factors
    // unchanged through render-state setter106fe80; these are D3D blend factors.
    // gpu_set_blendmode RVA110a570 normal branch sets state6=5/state7=6;
    // additive branch uses destination2. Thus bm_one=2, src_alpha=5, inv_src_alpha=6.
    static constexpr int One=2,SrcAlpha=5,InvSrcAlpha=6;
    YYTK::YYTKInterface* api_=nullptr;
    YYTK::CInstance* self_=nullptr;
    std::array<YYTK::TRoutine,Count> functions_{};
    std::array<int,3> surfaces_{{-1,-1,-1}};
    int width_=0,height_=0;
    bool available_=false,attempted_=false;
    template<class... Args> RV Call(Fn fn,Args&&... args) {
        std::array<RV,sizeof...(Args)+1> values{{RV(std::forward<Args>(args))...}};
        RV result;functions_[fn](result,self_,self_,int(sizeof...(Args)),values.data());return result;
    }
    bool Initialize(YYTK::YYTKInterface* api) {
        if(api_!=api){api_=api;functions_.fill(nullptr);attempted_=false;available_=false;}
        if(attempted_)return available_;
        attempted_=true;if(!api_)return false;
        for(unsigned i=0;i<Count;++i){void* pointer=nullptr;
            if(!Aurie::AurieSuccess(api_->GetNamedRoutinePointer(Names[i],&pointer))||!pointer)return false;
            functions_[i]=reinterpret_cast<YYTK::TRoutine>(pointer);
        }
        return available_=true;
    }
    bool ExistsNow(int id){return id>=0&&Call(Exists,id).ToBoolean();}
    void FreeAll(){for(int owner=1;owner<=2;++owner){if(ExistsNow(surfaces_[owner]))Call(Free,surfaces_[owner]);surfaces_[owner]=-1;}width_=height_=0;}
    struct State {
        NativeSurfaceRenderer& r;
        std::array<RV,3> matrices;
        RV color,alpha,font,halign,valign,scissor;
        explicit State(NativeSurfaceRenderer& owner):r(owner),
            matrices{{r.Call(MatrixGet,0),r.Call(MatrixGet,1),r.Call(MatrixGet,2)}},
            color(r.Call(GetColor)),alpha(r.Call(GetAlpha)),font(r.Call(GetFont)),
            halign(r.Call(GetHalign)),valign(r.Call(GetValign)),scissor(r.Call(GetScissor)) {r.Call(Push);}
        void Matrices(){for(int i=0;i<3;++i)r.Call(MatrixSet,i,matrices[i]);}
        void Drawing(){r.Call(SetColor,color);r.Call(SetAlpha,alpha);r.Call(SetFont,font);r.Call(SetHalign,halign);r.Call(SetValign,valign);}
        ~State(){Matrices();Drawing();r.Call(Pop);r.Call(SetScissor,scissor);}
    };
    struct SurfaceTarget {
        NativeSurfaceRenderer& r;bool active;
        SurfaceTarget(NativeSurfaceRenderer& renderer,int id):r(renderer),active(r.Call(Target,id).ToBoolean()){}
        ~SurfaceTarget(){if(active)r.Call(Reset);}
    };
    static bool Finite(Rect v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.w)&&std::isfinite(v.h)&&v.w>0&&v.h>0;}
    static Rect Clipped(Rect v,double w,double h){
        if(!Finite(v))return {};
        const double x=std::clamp(std::floor(v.x),0.0,w),y=std::clamp(std::floor(v.y),0.0,h);
        return {x,y,(std::max)(0.0,(std::min)(w,std::ceil(v.x+v.w))-x),(std::max)(0.0,(std::min)(h,std::ceil(v.y+v.h))-y)};
    }
    void Piece(int surface,Rect source,Rect dest){
        if(source.w<=0||source.h<=0||dest.w<=0||dest.h<=0)return;
        Call(Part,surface,source.x,source.y,source.w,source.h,dest.x,dest.y,
             dest.w/source.w,dest.h/source.h,0xffffff,1.0);
    }
public:
    // No destructor engine calls: destruction can happen after the runner/GPU
    // shuts down. Release explicitly during a Draw GUI callback if desired.
    NativeSurfaceRenderer()=default;
    NativeSurfaceRenderer(const NativeSurfaceRenderer&)=delete;
    NativeSurfaceRenderer& operator=(const NativeSurfaceRenderer&)=delete;
    void Release(){if(available_)FreeAll();}
    bool Save(int owner,const char* path){
        if(!available_||owner<1||owner>2||!path||!ExistsNow(surfaces_[owner]))return false;
        Call(SaveSurface,surfaces_[owner],path);return true; // runner returns undefined
    }
    // RenderOwner(owner) executes the original native GUI once. RegionRead(owner)
    // runs immediately afterwards and returns the native health/status rectangle
    // in GUI coordinates (cache it before restoring any Controller overlay).
    // StockRead returns the native weapon/construct strip bounds. Caller owns
    // all game-state snapshots and ownership scopes in RenderOwner.
    template<class RenderOwner,class RegionRead,class StockRead>
    Result Capture(YYTK::YYTKInterface* api,YYTK::CInstance* self,double w,double h,
                   RenderOwner&& renderOwner,RegionRead&& regionRead,StockRead&& stockRead,int primary,double scale,bool shareStock=false){
        Result result;
        if(!Initialize(api)||!std::isfinite(w)||!std::isfinite(h)||w<1||h<1||w>32768||h>32768||
           !std::isfinite(scale)||scale<=0||primary<1||primary>2)return result;
        self_=self;
        const int width=int(std::ceil(w)),height=int(std::ceil(h));
        if(width_!=width||height_!=height){FreeAll();width_=width;height_=height;}
        for(int owner=1;owner<=2;++owner){
            if(!ExistsNow(surfaces_[owner]))surfaces_[owner]=Call(Create,width,height).ToInt32();
            if(!ExistsNow(surfaces_[owner]))return result;
        }
        // Includes blend factors, write masks and scissor. Matrix/draw state is
        // independent of the GPU state stack and is saved explicitly.
        State entry(*this);
        for(int owner=1;owner<=2;++owner){
            State capture(*this);
            SurfaceTarget target(*this,surfaces_[owner]);
            if(!target.active)return result;
            capture.Matrices();
            Call(Clear,0,0.0);
            // Store premultiplied RGB with proper source-over alpha coverage.
            Call(Blend,SrcAlpha,InvSrcAlpha,One,InvSrcAlpha);
            renderOwner(owner);
            result.source[owner]=Clipped(regionRead(owner),w,h);
            if(!Finite(result.source[owner]))return result;
            result.stock_source[owner]=Clipped(stockRead(owner),w,h);
        }
        entry.Matrices();entry.Drawing();
        // Premultiplied surface pixels must not be multiplied by alpha again.
        Call(Blend,One,InvSrcAlpha,One,InvSrcAlpha);Call(SetColor,0xffffff);Call(SetAlpha,1.0);
        const Rect crop=result.source[primary];
        // Remove both native widget regions from the shared GUI. Subtracting
        // rectangles also handles overlapping source bounds without double draws.
        std::vector<Rect> outside{{0,0,w,h}};
        for(Rect cut:{crop,result.stock_source[primary]}){
            if(!Finite(cut))continue;
            std::vector<Rect> next;
            for(Rect part:outside){
                double l=std::max(part.x,cut.x),t=std::max(part.y,cut.y);
                double r=std::min(part.x+part.w,cut.x+cut.w),b=std::min(part.y+part.h,cut.y+cut.h);
                if(l>=r||t>=b){next.push_back(part);continue;}
                next.push_back({part.x,part.y,part.w,t-part.y});
                next.push_back({part.x,b,part.w,part.y+part.h-b});
                next.push_back({part.x,t,l-part.x,b-t});
                next.push_back({r,t,part.x+part.w-r,b-t});
            }
            outside=std::move(next);
        }
        for(auto part:outside)Piece(surfaces_[primary],part,part);
        const double margin=(std::min)(48*scale,w/4),maxWidth=(std::max)(1.0,(w-3*margin)/2);
        const double bottom=(std::min)(h,crop.y+crop.h);
        for(int owner=1;owner<=2;++owner){
            const Rect source=result.source[owner];const double fit=(std::min)({1.0,maxWidth/source.w,bottom/source.h});
            const double dw=source.w*fit,dh=source.h*fit;
            result.dest[owner]={owner==1?margin:w-margin-dw,bottom-dh,dw,dh};
            Piece(surfaces_[owner],source,result.dest[owner]);result.surface[owner]=surfaces_[owner];
            const Rect stock=result.stock_source[owner];
            if(Finite(stock)){
                const double stockFit=std::min(1.0,maxWidth/stock.w);
                const double sw=stock.w*stockFit,sh=stock.h*stockFit;
                result.stock_dest[owner]={owner==1?margin:w-margin-sw,result.dest[owner].y-10*scale-sh,sw,sh};
                Piece(surfaces_[shareStock?primary:owner],stock,result.stock_dest[owner]);
            }
        }
        result.ok=true;return result;
    }
};
} // namespace ndship::surface_hud
