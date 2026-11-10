#include <limits>
#include <string>
#include <unordered_map>
#include <iostream>
#include "dlssnr/NrRenderInput.h"
struct Params : NVSDK_NGX_Parameter {
    std::unordered_map<std::string,unsigned int> values;
    void Set(const char* k, unsigned int v) override { values[k]=v; }
    NVSDK_NGX_Result Get(const char* k, unsigned int* v) const override {
        auto it=values.find(k); if(it==values.end())return NVSDK_NGX_Result_FAIL_InvalidParameter;
        *v=it->second; return NVSDK_NGX_Result_Success;
    }
#define UNUSED_TYPE(T) void Set(const char*, T) override {} NVSDK_NGX_Result Get(const char*, T*) const override {return NVSDK_NGX_Result_FAIL_InvalidParameter;}
    UNUSED_TYPE(unsigned long long)
    UNUSED_TYPE(float)
    UNUSED_TYPE(double)
    UNUSED_TYPE(int)
    UNUSED_TYPE(ID3D11Resource*)
    UNUSED_TYPE(ID3D12Resource*)
    UNUSED_TYPE(void*)
#undef UNUSED_TYPE
    void Reset() override {values.clear();}
};
int main() {
    unsigned int checks=0, failures=0;
    auto check = [&](bool ok, const char* name) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    Params p; unsigned int w{}, h{};
    check(DlssNr::ReadRenderInputSize(&p,1920,1080,w,h) && w==1920 && h==1080, "render allocation fallback");
    p.Set(NVSDK_NGX_Parameter_Width,1280u); p.Set(NVSDK_NGX_Parameter_Height,720u);
    check(DlssNr::ReadRenderInputSize(&p,3840,2160,w,h) && w==1280 && h==720, "render creation dimensions");
    p.Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,1920u);
    p.Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,1080u);
    check(DlssNr::ReadRenderInputSize(&p,3840,2160,w,h) && w==1920 && h==1080, "render active subrect");
    check(!DlssNr::ReadRenderInputSize(&p,1280,720,w,h), "oversized render rejected");
    const char* offsets[]={NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y,
        NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y,
        NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X,NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y};
    for(auto key:offsets) { p.Set(key,1u); check(!DlssNr::ReadRenderInputSize(&p,3840,2160,w,h), key); p.Set(key,0u); }
    p.Reset(); check(!DlssNr::ReadRenderInputSize(&p,0,0,w,h), "empty render rejected");

    // Query results may be render sizes. Post-SR must cover the bottom-right pixel too.
    for (const auto& size : {std::pair<unsigned int,unsigned int>{1920,1080}, {2258,1270}, {2560,1440}, {3840,2160}, {7680,4320}, {0,0}}) {
        p.Reset(); p.Set(NVSDK_NGX_Parameter_OutWidth,size.first); p.Set(NVSDK_NGX_Parameter_OutHeight,size.second);
        const bool valid = DlssNr::ReadOutputSize(&p,3840,2160,w,h);
        check(valid && w==3840 && h==2160, "post-SR uses actual output, not query dimensions");
        check(valid && 3839u<w && 2159u<h, "post-SR bottom-right inside dispatch bounds");
    }
    p.Reset(); check(DlssNr::ReadOutputSize(&p,3840,2160,w,h) && w==3840 && h==2160, "missing output parameters");
    p.Set(NVSDK_NGX_Parameter_OutWidth,1920u);
    check(DlssNr::ReadOutputSize(&p,3840,2160,w,h) && w==3840 && h==2160, "partial query dimensions");
    p.Set(NVSDK_NGX_Parameter_Width,1280u); p.Set(NVSDK_NGX_Parameter_Height,720u);
    for (unsigned int renderWidth : {1920u,1280u,1600u}) {
        p.Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,renderWidth);
        p.Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,720u);
        check(DlssNr::ReadRenderInputSize(&p,3840,2160,w,h) && w==renderWidth && h==720, "NR-before-SR retains dynamic render size");
        check(DlssNr::ReadOutputSize(&p,3840,2160,w,h) && w==3840 && h==2160, "switch to SR-before-NR retains full output");
    }
    check(DlssNr::ReadOutputSize(&p,1920,1080,w,h) && w==1920 && h==1080, "output resolution change");
    for (auto key : {NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y}) {
        p.Set(key,1u); check(!DlssNr::ReadOutputSize(&p,3840,2160,w,h), "output atlas offset rejected"); p.Set(key,0u);
    }
    p.Reset();
    check(!DlssNr::ReadOutputSize(&p,0,2160,w,h), "zero output width rejected");
    check(!DlssNr::ReadOutputSize(&p,3840,0,w,h), "zero output height rejected");
    check(!DlssNr::ReadOutputSize(&p,0,0,w,h), "empty output rejected");
    check(!DlssNr::ReadOutputSize(&p,static_cast<unsigned long long>((std::numeric_limits<unsigned int>::max)())+2,2160,w,h), "output width overflow rejected");
    std::cout << (failures ? "FAIL: " : "PASS: ") << checks << " size checks, " << failures << " failures.\n";
    return failures ? 1 : 0;
}

