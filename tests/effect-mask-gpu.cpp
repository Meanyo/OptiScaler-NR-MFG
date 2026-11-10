// Executes the shipped DXIL resolve on D3D12 WARP; no game or NVIDIA NR model required.
#define NOMINMAX
#include <windows.h>
#undef near
#undef small
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>
#include "../OptiScaler/shaders/dlssnr/DlssNr_Common.h"
using Microsoft::WRL::ComPtr;
static_assert(sizeof(DlssNrConstants) == 256);
static_assert(offsetof(DlssNrConstants, EffectStrength) == 92);
static_assert(offsetof(DlssNrConstants, EffectSensitivity) == 96);
static_assert(offsetof(DlssNrConstants, EffectPreview) == 100);
static_assert(offsetof(DlssNrConstants, AdjustmentPreview) == 104);
static_assert(offsetof(DlssNrConstants, AdjustmentPreviewGain) == 108);
void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D12 HRESULT " + std::to_string(hr)); }
struct Pixel { float r, g, b, a; };
constexpr UINT W = 37, H = 23;
struct Gpu
{
    ComPtr<ID3D12Device> dev;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12DescriptorHeap> heap;
    UINT64 serial = 0;
    Gpu()
    {
        ComPtr<IDXGIFactory4> factory;
        check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter> adapter;
        check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
        check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)));
        D3D12_COMMAND_QUEUE_DESC q{};
        check(dev->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)));
        check(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));
        check(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd)));
        check(cmd->Close());
        check(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0, 0};
        ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, 0};
        D3D12_ROOT_PARAMETER params[3]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
        for (int i = 1; i < 3; ++i) {
            params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable = {1, &ranges[i-1]};
        }
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        D3D12_ROOT_SIGNATURE_DESC rd{3, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, error;
        check(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
        check(dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 7, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        check(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)));
    }
    ComPtr<ID3D12Resource> buffer(UINT64 size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
    {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = type;
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = size;
        d.Height = d.DepthOrArraySize = d.MipLevels = d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> res;
        check(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&res)));
        return res;
    }
    void transition(ID3D12Resource* res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition = {res, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
        cmd->ResourceBarrier(1, &b);
    }
    std::vector<Pixel> run(const char* path, const DlssNrConstants& c, const std::vector<Pixel>& input, bool small = false)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Missing shader");
        std::vector<char> code((std::istreambuf_iterator<char>(f)), {});
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{}; pd.pRootSignature = root.Get();
        pd.CS = {code.data(), code.size()};
        ComPtr<ID3D12PipelineState> pso;
        check(dev->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso)));
        check(alloc->Reset()); check(cmd->Reset(alloc.Get(), pso.Get()));
        auto cb = buffer(sizeof(c), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        void* ptr; check(cb->Map(0, nullptr, &ptr)); std::memcpy(ptr, &c, sizeof(c)); cb->Unmap(0, nullptr);
        std::array<ComPtr<ID3D12Resource>, 5> textures;
        std::vector<ComPtr<ID3D12Resource>> uploads;
        const UINT step = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT outputFoot{}; UINT64 total = 0;
        for (int i = 0; i < 5; ++i) {
            UINT width = small && i < 2 ? 19 : W, height = small && i < 2 ? 12 : H;
            D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            d.Width = width; d.Height = height; d.DepthOrArraySize = d.MipLevels = d.SampleDesc.Count = 1;
            d.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            d.Flags = i >= 3 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
            D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
            auto state = i >= 3 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COPY_DEST;
            check(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&textures[i])));
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot{};
            dev->GetCopyableFootprints(&d, 0, 1, 0, &foot, nullptr, nullptr, &total);
            if (i < 3) {
                auto upload = buffer(total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
                check(upload->Map(0, nullptr, &ptr));
                for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
                    Pixel p = i == 2 ? input[y * W + x] : (i == 0 ? Pixel{0.2f,0.2f,0.2f,1} : Pixel{0.4f,0.3f,0.2f,1});
                    std::memcpy(static_cast<char*>(ptr) + foot.Offset + y * foot.Footprint.RowPitch + x * sizeof(Pixel), &p, sizeof(p));
                }
                upload->Unmap(0, nullptr);
                D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = upload.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = foot;
                D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = textures[i].Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                transition(textures[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                uploads.push_back(upload);
            } else if (i == 3) outputFoot = foot;
        }
        for (UINT i = 0; i < 5; ++i) {
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Texture2D.MipLevels = 1;
            dev->CreateShaderResourceView(textures[std::min(i, 2u)].Get(), &sd, {cpu.ptr + step * i});
        }
        for (UINT i = 0; i < 2; ++i) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            dev->CreateUnorderedAccessView(textures[3+i].Get(), nullptr, &ud, {cpu.ptr + step * (5+i)});
        }
        ID3D12DescriptorHeap* heaps[]{heap.Get()}; cmd->SetDescriptorHeaps(1, heaps);
        cmd->SetComputeRootSignature(root.Get());
        cmd->SetComputeRootConstantBufferView(0, cb->GetGPUVirtualAddress());
        auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
        cmd->SetComputeRootDescriptorTable(1, gpu);
        cmd->SetComputeRootDescriptorTable(2, {gpu.ptr + step * 5});
        cmd->Dispatch((W+7)/8, (H+7)/8, 1);
        transition(textures[3].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        auto readback = buffer(total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource = textures[3].Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = outputFoot;
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        check(cmd->Close()); ID3D12CommandList* lists[]{cmd.Get()}; queue->ExecuteCommandLists(1, lists);
        check(queue->Signal(fence.Get(), ++serial));
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        check(fence->SetEventOnCompletion(serial, event));
        DWORD waited = WaitForSingleObject(event, 30000); CloseHandle(event);
        if (waited != WAIT_OBJECT_0) throw std::runtime_error("GPU timeout");
        std::vector<Pixel> output(W * H);
        check(readback->Map(0, nullptr, &ptr));
        for (UINT y = 0; y < H; ++y)
            std::memcpy(output.data() + y * W, static_cast<char*>(ptr) + outputFoot.Offset + y * outputFoot.Footprint.RowPitch, W * sizeof(Pixel));
        readback->Unmap(0, nullptr);
        return output;
    }
};
int tests = 0;
void expect(bool pass, const char* name) {
    if (!pass) throw std::runtime_error(name);
    ++tests; std::cout << "PASS " << name << '\n';
}
bool near(float a, float b) { return std::isfinite(a) && std::abs(a-b) < 0.00005f; }
int main(int argc, char** argv) try
{
    if (argc != 3) throw std::runtime_error("Usage: effect-mask-gpu NEW_DXIL R3_DXIL");
    Gpu gpu;
    DlssNrConstants c{};
    c.Mode = DlssNrMode_Resolve; c.Width = W; c.Height = H; c.WhitePoint = 1;
    c.ApplyModel = 1; c.TransferStrength = c.ColourStrength = 1; c.MaxRatio = 2;
    c.CompareZoom = 1; c.DebugScale = 1; c.EffectSensitivity = 0.5f;
    std::vector<Pixel> input(W * H, Pixel{0.02f,0.02f,0.02f,0.37f});
    for (UINT y=5; y<18; ++y) for (UINT x=10; x<28; ++x) input[y*W+x] = {4,2,1,0.37f};
    input[0] = {5,5,5,0.37f};
    const UINT bright = 11*W+18, dark = 20*W+34;
    for (UINT mode=0; mode<5; ++mode) for (UINT passthrough=0; passthrough<2; ++passthrough) {
        for (UINT y=5; y<18; ++y) for (UINT x=10; x<28; ++x)
            input[y*W+x] = passthrough ? Pixel{1,0.6f,0.2f,0.37f} : Pixel{4,2,1,0.37f};
        input[0] = passthrough ? Pixel{1,1,1,0.37f} : Pixel{5,5,5,0.37f};
        c.ReversibleMode = mode; c.Passthrough = passthrough; c.EffectStrength = 0;
        auto baseline = gpu.run(argv[2], c, input);
        auto off = gpu.run(argv[1], c, input);
        expect(std::memcmp(baseline.data(), off.data(), W*H*sizeof(Pixel)) == 0, "disabled output equals r3 (all modes/HDR-SDR)");
        c.EffectStrength = 1;
        auto on = gpu.run(argv[1], c, input);
        expect(near(on[bright].r, input[bright].r) && near(on[bright].g, input[bright].g), "bright effect preserved");
        expect(near(on[dark].r, off[dark].r) && near(on[dark].g, off[dark].g), "dark scene detail unchanged");
        expect(near(on[0].r, input[0].r), "corner particle protected without out-of-bounds reads");
        bool finite = true;
        for (auto p : on) finite &= std::isfinite(p.r) && std::isfinite(p.g) && std::isfinite(p.b) && p.a == 0.37f;
        expect(finite, "all pixels finite and original alpha preserved");
        c.EffectStrength = 0.5f;
        auto half = gpu.run(argv[1], c, input);
        expect(near(half[bright].r, (off[bright].r + input[bright].r)*0.5f), "strength midpoint blends toward input");
    }
    for (UINT y=5; y<18; ++y) for (UINT x=10; x<28; ++x) input[y*W+x] = {4,2,1,0.37f};
    c.ReversibleMode=3; c.Passthrough=0; c.EffectStrength=1; c.Transfer=1;
    auto small = gpu.run(argv[1], c, input, true);
    expect(near(small[bright].r, input[bright].r), "reduced NR raster still protects full frame");
    c.EffectPreview=1;
    auto preview = gpu.run(argv[1], c, input);
    expect(preview[bright].r > preview[bright].g && near(preview[dark].r,input[dark].r), "preview marks effects red and leaves unmasked input");
    c.EffectStrength=0;
    auto previewZero = gpu.run(argv[1], c, input);
    expect(std::memcmp(preview.data(),previewZero.data(),W*H*sizeof(Pixel))==0, "preview independent of protection strength");
    c.EffectPreview=0; c.EffectStrength=1; c.ApplyModel=0;
    auto clean = gpu.run(argv[1], c, input);
    expect(std::memcmp(clean.data(),input.data(),W*H*sizeof(Pixel))==0, "apply-model off preserves clean input");
    c.ApplyModel=1; c.EffectStrength=0; c.EffectPreview=0; c.Transfer=0; c.AdjustmentPreviewGain=4;
    for (UINT mode : {0u,1u,3u}) {
        c.ReversibleMode=mode; c.AdjustmentPreview=2; c.TransferStrength=0;
        auto noDetail = gpu.run(argv[1],c,input);
        expect(std::memcmp(noDetail.data(),input.data(),W*H*sizeof(Pixel))==0, "detail zero has no red luminance footprint");
        c.TransferStrength=1; c.AdjustmentPreview=3; c.ColourStrength=0;
        auto noColour = gpu.run(argv[1],c,input);
        expect(std::memcmp(noColour.data(),input.data(),W*H*sizeof(Pixel))==0, "colour zero has no red colour footprint");
        c.ColourStrength=1; c.AdjustmentPreview=4; c.MaxRatio=1000;
        auto noGuard = gpu.run(argv[1],c,input);
        expect(std::memcmp(noGuard.data(),input.data(),W*H*sizeof(Pixel))==0, "non-binding guard has no red footprint");
        c.MaxRatio=1;
        auto guardPreview = gpu.run(argv[1],c,input);
        expect(guardPreview[dark].r > input[dark].r && guardPreview[dark].g < input[dark].g, "binding guard highlights constrained pixels");
    }
    for (UINT mode : {2u,4u}) for (UINT previewMode : {2u,3u,4u}) {
        c.ReversibleMode=mode; c.AdjustmentPreview=previewMode;
        auto replaced = gpu.run(argv[1],c,input);
        expect(std::memcmp(replaced.data(),input.data(),W*H*sizeof(Pixel))==0, "pure replacement reports no skipped-composition footprint");
    }
    c.ReversibleMode=3; c.AdjustmentPreview=1; c.MaxRatio=2;
    auto total = gpu.run(argv[1],c,input);
    expect(total[dark].r > input[dark].r && total[dark].g < input[dark].g, "total preview highlights final NR changes");
    c.EffectStrength=1;
    auto protectedPreview = gpu.run(argv[1],c,input);
    expect(near(protectedPreview[bright].r,input[bright].r) && near(protectedPreview[bright].g,input[bright].g), "total preview excludes fully protected pixels");
    c.CompareMode=2; c.CompareSplit=0.5f;
    auto wipePreview = gpu.run(argv[1],c,input);
    expect(near(wipePreview[3*W+3].r,input[3*W+3].r), "comparison original side remains untouched by preview");
    c.CompareMode=0; c.ApplyModel=0;
    auto hiddenPreview = gpu.run(argv[1],c,input);
    expect(std::memcmp(hiddenPreview.data(),input.data(),W*H*sizeof(Pixel))==0, "apply-model off bypasses adjustment preview");
    std::cout << "Completed " << tests << " checks on D3D12 WARP using production DXIL.\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
