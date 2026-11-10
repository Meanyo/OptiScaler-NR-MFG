#pragma once
#include <nvsdk_ngx.h>
#include <limits>

namespace DlssNr
{
// Post-SR must cover the actual output allocation. OutWidth/OutHeight can contain
// a recommended render size left by an optimal-settings query and are not authoritative.
inline bool ReadOutputSize(NVSDK_NGX_Parameter* params, unsigned long long allocationWidth,
                           unsigned int allocationHeight, unsigned int& width, unsigned int& height)
{
    width = height = 0;
    if (!params || !allocationWidth || !allocationHeight ||
        allocationWidth > (std::numeric_limits<unsigned int>::max)())
        return false;
    unsigned int x = 0, y = 0;
    params->Get(NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X, &x);
    params->Get(NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y, &y);
    if (x || y)
        return false;
    width = static_cast<unsigned int>(allocationWidth);
    height = allocationHeight;
    return true;
}

inline bool ReadRenderInputSize(NVSDK_NGX_Parameter* params, unsigned long long allocationWidth,
                                unsigned int allocationHeight, unsigned int& width, unsigned int& height)
{
    width = height = 0;
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, &width);
    params->Get(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, &height);
    if (!width || !height)
    {
        params->Get(NVSDK_NGX_Parameter_Width, &width);
        params->Get(NVSDK_NGX_Parameter_Height, &height);
    }
    if (!width || !height)
    {
        if (allocationWidth > (std::numeric_limits<unsigned int>::max)())
            return false;
        width = static_cast<unsigned int>(allocationWidth);
        height = allocationHeight;
    }
    const char* offsets[] = { NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X,
                              NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y,
                              NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X,
                              NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y,
                              NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X,
                              NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y };
    for (const char* key : offsets)
    {
        unsigned int offset = 0;
        params->Get(key, &offset);
        if (offset)
            return false;
    }
    return width && height && width <= allocationWidth && height <= allocationHeight;
}
}
