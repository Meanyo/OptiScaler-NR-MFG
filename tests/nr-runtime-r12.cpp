// Compile twice as a mock model (/DMOCK_NR_MODEL /DMODEL_ID=1 or 2), then as
// a host. Exercises the real forwarders without a GPU or NVIDIA initialization.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

#ifdef MOCK_NR_MODEL
static int handle = MODEL_ID;
static int releases = 0;
extern "C" {
__declspec(dllexport) int NVSDK_NGX_D3D12_Init_Ext(unsigned long long, const wchar_t*, void*, int, const void*) { return 1; }
__declspec(dllexport) int NVSDK_NGX_D3D12_CreateFeature(void*, int, const void*, void** out) { *out = &handle; return 1; }
__declspec(dllexport) int NVSDK_NGX_D3D12_EvaluateFeature(void*, const void*, const void*, void*) { return 1; }
__declspec(dllexport) int NVSDK_NGX_D3D12_ReleaseFeature(void* feature) { if (feature != &handle) return -1; ++releases; return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_Init_Ext(unsigned long long, const wchar_t*, void*, void*, void*, const void*, int) { return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_CreateFeature(void*, int, const void*, void** out) { *out = &handle; return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_EvaluateFeature(void*, const void*, const void*, void*) { return 1; }
__declspec(dllexport) int NVSDK_NGX_VULKAN_ReleaseFeature(void* feature) { return NVSDK_NGX_D3D12_ReleaseFeature(feature); }
__declspec(dllexport) int mock_releases() { return releases; }
}
#else
using Create = void*(*)(const wchar_t*, const wchar_t*, void*, void*, void*, unsigned, unsigned, int, float, int, float, float, float, int, int);
using Release = void(*)(void*);
using VkInit = int(*)(const wchar_t*, const wchar_t*, void*, void*, void*, int);
using VkCreate = void*(*)(void*, void*, unsigned, unsigned, int, float, int, float, float, float, int, int);
static void setFloat(void*, const char*, float) {}
static void setUInt(void*, const char*, unsigned) {}
int wmain(int argc, wchar_t** argv)
{
    if (argc != 5) return 2;
    void* table[8] = {};
    table[1] = (void*)setFloat;
    table[3] = (void*)setUInt;
    void** params = table;
    HMODULE forwarders[2] = {};
    HMODULE models[2] = {};
    void* features[2] = {};
    Release release[2] = {};
    int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d\n", __LINE__); return 1; } } while (0)
    for (int i = 0; i != 2; ++i)
    {
        forwarders[i] = LoadLibraryW(argv[1+i]);
        CHECK(forwarders[i] != nullptr);
        auto create = (Create)GetProcAddress(forwarders[i], "dlssnr_call_create");
        release[i] = (Release)GetProcAddress(forwarders[i], "dlssnr_call_release");
        CHECK(create && release[i]);
        features[i] = create(argv[3+i], L".", nullptr, nullptr, &params, 64, 64, 0, 1, 0, 1, 1, -1, 1, 1);
        CHECK(features[i] && *(int*)features[i] == i+1);
        models[i] = GetModuleHandleW(argv[3+i]);
        CHECK(models[i] != nullptr);
        auto init = (VkInit)GetProcAddress(forwarders[i], "dlssnr_vk_init");
        auto vkCreate = (VkCreate)GetProcAddress(forwarders[i], "dlssnr_vk_create");
        auto vkRelease = (Release)GetProcAddress(forwarders[i], "dlssnr_vk_release");
        CHECK(init && vkCreate && vkRelease);
        CHECK(init(argv[3+i], L".", nullptr, nullptr, nullptr, 0x15) == 1);
        auto vkFeature = vkCreate((void*)1, &params, 64, 64, 0, 1, 0, 1, 1, -1, 1, 1);
        CHECK(vkFeature && *(int*)vkFeature == i+1);
        vkRelease(vkFeature);
    }
    CHECK(forwarders[0] != forwarders[1]);
    // Release an old feature AFTER loading and using the second model, as fence retirement does.
    release[0](features[0]);
    release[1](features[1]);
    for (int i = 0; i != 2; ++i)
    {
        auto count = (int(*)())GetProcAddress(models[i], "mock_releases");
        CHECK(count && count() == 2);
        auto create = (Create)GetProcAddress(forwarders[i], "dlssnr_call_create");
        auto again = create(argv[3+i], L".", nullptr, nullptr, &params, 64, 64, 0, 1, 0, 1, 1, -1, 1, 1);
        CHECK(again && *(int*)again == i+1);
        release[i](again);
        CHECK(count() == 3);
    }
    std::printf("R12 runtime isolation: %d checks passed (D3D12 + Vulkan, switch back, old-feature release)\n", checks);
}
#endif
