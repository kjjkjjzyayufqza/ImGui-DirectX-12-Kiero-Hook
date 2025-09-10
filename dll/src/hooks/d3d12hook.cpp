#include <framework/stdafx.h>
#include "d3d12hook.h"
#include <kiero.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_2.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "ui/UiRenderer.h"


// Debug
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

typedef HRESULT(__stdcall *PresentFunc)(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags);
PresentFunc oPresent = nullptr;

typedef HRESULT(__stdcall *Present1Func)(IDXGISwapChain1 *pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
Present1Func oPresent1 = nullptr;

// Forward declaration for IDXGISwapChain1::Present1 hook
HRESULT __fastcall hkPresent1(IDXGISwapChain1 *pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);

typedef void(__stdcall *ExecuteCommandListsFunc)(ID3D12CommandQueue *pCommandQueue, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);
ExecuteCommandListsFunc oExecuteCommandLists = nullptr;

typedef HRESULT(__stdcall *ResizeBuffers)(IDXGISwapChain3 *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
ResizeBuffers oResizeBuffers;

typedef HRESULT(__stdcall *SignalFunc)(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value);
SignalFunc oSignal = nullptr;

HWND window;
WNDPROC oWndProc;

struct FrameContext
{
    ID3D12CommandAllocator *CommandAllocator;
    UINT64 FenceValue; // In imgui original code // i didn't use it
    ID3D12Resource *g_mainRenderTargetResource = {};
    D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor = {};
};

// Data
static int const NUM_FRAMES_IN_FLIGHT = 3;
// static FrameContext*                g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
//  Modified
FrameContext *g_frameContext;
static UINT g_frameIndex = 0;
static UINT g_fenceValue = 0;

// static int const                    NUM_BACK_BUFFERS = 3; // original
static int NUM_BACK_BUFFERS = -1;
static ID3D12Device *g_pd3dDevice = nullptr;
static ID3D12DescriptorHeap *g_pd3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap *g_pd3dSrvDescHeap = nullptr;
static ID3D12CommandQueue *g_pd3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList *g_pd3dCommandList = nullptr;
static ID3D12Fence *g_fence = nullptr;
static HANDLE g_fenceEvent = nullptr;
static UINT64 g_fenceLastSignaledValue = 0;
static IDXGISwapChain3 *g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static HANDLE g_hSwapChainWaitableObject = nullptr;
// static ID3D12Resource*              g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {}; // Original
// static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {}; // Original
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

static bool g_present1Hooked = false;

static bool g_show_ui = true;
bool bShould_render = true;

static void CreateRenderTarget()
{
    if (!g_pSwapChain || !g_pd3dDevice || !g_pd3dRtvDescHeap || !g_frameContext || NUM_BACK_BUFFERS <= 0)
        return;

    // Получаем размер дескриптора RTV и начальный дескриптор
    SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

    // Сначала инициализируем дескрипторы
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        g_frameContext[i].g_mainRenderTargetDescriptor = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // Затем создаем render target views
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource *pBackBuffer = nullptr;
        if (SUCCEEDED(g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer))))
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, g_frameContext[i].g_mainRenderTargetDescriptor);
            g_frameContext[i].g_mainRenderTargetResource = pBackBuffer;
        }
        else
        {
            LOG_ERROR("Failed to get back buffer %d", i);
            if (pBackBuffer)
            {
                pBackBuffer->Release();
                pBackBuffer = nullptr;
            }
        }
    }
}

static void CleanupRenderTarget()
{
    if (g_frameContext && NUM_BACK_BUFFERS > 0)
    {
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            if (g_frameContext[i].g_mainRenderTargetResource)
            {
                g_frameContext[i].g_mainRenderTargetResource->Release();
                g_frameContext[i].g_mainRenderTargetResource = nullptr;
            }
        }
    }
}

static void WaitForLastSubmittedFrame()
{
    UINT bufferCount = (NUM_BACK_BUFFERS > 0) ? (UINT)NUM_BACK_BUFFERS : NUM_FRAMES_IN_FLIGHT;
    FrameContext *frameCtx = &g_frameContext[g_frameIndex % bufferCount];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT APIENTRY WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hwnd, uMsg, wParam, lParam);
}

void InitImGui()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;  // prevent system cursor flicker on exit
    ImGui::StyleColorsLight();

    // Initialize UI (loads ReShade-like default font) BEFORE backend init so atlas is uploaded once
    ui::Initialize();

    ImGui_ImplWin32_Init(window);

    ImGui_ImplDX12_Init(g_pd3dDevice, NUM_FRAMES_IN_FLIGHT,
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        g_pd3dSrvDescHeap,
                        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
                        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());
}

static bool IsDirectQueue(ID3D12CommandQueue* queue)
{
    if (!queue) return false;
    D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
    return desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
}

static void SignalFrameFence(FrameContext& frameCtx)
{
    if (!g_pd3dCommandQueue || !g_fence)
        return;
    ++g_fenceLastSignaledValue;
    frameCtx.FenceValue = g_fenceLastSignaledValue;
    g_pd3dCommandQueue->Signal(g_fence, g_fenceLastSignaledValue);
}

static void CreateFrameContextsAndAllocators(UINT bufferCount)
{
    if (g_frameContext)
    {
        for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
        {
            // Free previous resources if any
            if (g_frameContext[i].g_mainRenderTargetResource)
            {
                g_frameContext[i].g_mainRenderTargetResource->Release();
                g_frameContext[i].g_mainRenderTargetResource = nullptr;
            }
            if (g_frameContext[i].CommandAllocator)
            {
                g_frameContext[i].CommandAllocator->Release();
                g_frameContext[i].CommandAllocator = nullptr;
            }
        }
        delete[] g_frameContext;
        g_frameContext = nullptr;
    }

    NUM_BACK_BUFFERS = static_cast<int>(bufferCount);
    g_frameContext = new FrameContext[NUM_BACK_BUFFERS];
    for (UINT i = 0; i < bufferCount; ++i)
    {
        g_frameContext[i].FenceValue = 0;
        g_frameContext[i].g_mainRenderTargetResource = nullptr;
        g_frameContext[i].g_mainRenderTargetDescriptor = {};
        g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator));
    }
}

static bool HookPresent1IfAvailable()
{
    if (g_present1Hooked)
        return true;

    // Create a temporary device/queue/swapchain1 to obtain Present1 address
    WNDCLASSEX wc { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"Kiero_Present1_Window";
    RegisterClassEx(&wc);
    HWND tempWnd = CreateWindow(wc.lpszClassName, L"Kiero_Present1", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);

    IDXGIFactory2* factory = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
    {
        DestroyWindow(tempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    ID3D12Device* device = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
    {
        factory->Release();
        DestroyWindow(tempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* cq = nullptr;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&cq))))
    {
        device->Release();
        factory->Release();
        DestroyWindow(tempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = 100;
    scd.Height = 100;
    scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SampleDesc.Count = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* sc1 = nullptr;
    if (FAILED(factory->CreateSwapChainForHwnd(cq, tempWnd, &scd, nullptr, nullptr, &sc1)))
    {
        cq->Release();
        device->Release();
        factory->Release();
        DestroyWindow(tempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtbl = *(void***)sc1;
    // IDXGISwapChain base has 18 entries; Present1 is the 5th new method in IDXGISwapChain1 (index 22 overall)
    const int kPresent1Index = 22;
    void* present1Addr = vtbl[kPresent1Index];

    bool hooked = false;
    if (present1Addr)
    {
        if (MH_CreateHook(present1Addr, (LPVOID)&hkPresent1, reinterpret_cast<void**>(&oPresent1)) == MH_OK &&
            MH_EnableHook(present1Addr) == MH_OK)
        {
            g_present1Hooked = true;
            hooked = true;
        }
    }

    sc1->Release();
    cq->Release();
    device->Release();
    factory->Release();
    DestroyWindow(tempWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return hooked;
}

HRESULT __fastcall hkPresent(IDXGISwapChain3 *pSwapChain, UINT SyncInterval, UINT Flags)
{
    static bool init = false;

    if (!init)
    {
        // Ждем пока не получим CommandQueue через ExecuteCommandLists
        if (!g_pd3dCommandQueue)
            return oPresent(pSwapChain, SyncInterval, Flags);

        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void **)&g_pd3dDevice)))
        {
            DXGI_SWAP_CHAIN_DESC sdesc;
            pSwapChain->GetDesc(&sdesc);
            window = sdesc.OutputWindow;
            NUM_BACK_BUFFERS = sdesc.BufferCount;

            // SRV Heap
            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                desc.NumDescriptors = 1;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap))))
                    return oPresent(pSwapChain, SyncInterval, Flags);
            }

            // RTV Heap
            {
                D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                desc.NumDescriptors = NUM_BACK_BUFFERS;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                desc.NodeMask = 1;
                if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap))))
                    return oPresent(pSwapChain, SyncInterval, Flags);
            }

            // Command Allocators (one per back buffer)
            CreateFrameContextsAndAllocators(sdesc.BufferCount);

            // Command List (independent, will reset per-frame with its allocator)
            if (FAILED(g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&g_pd3dCommandList))))
            {
                return oPresent(pSwapChain, SyncInterval, Flags);
            }
            g_pd3dCommandList->Close();

            // Fence & Events
            if (FAILED(g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))))
            {
                return oPresent(pSwapChain, SyncInterval, Flags);
            }

            g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (g_fenceEvent == nullptr)
            {
                return oPresent(pSwapChain, SyncInterval, Flags);
            }

            g_hSwapChainWaitableObject = pSwapChain->GetFrameLatencyWaitableObject();
            g_pSwapChain = pSwapChain;

            // Create RenderTarget
            CreateRenderTarget();

            // Hook window procedure
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (__int3264)(LONG_PTR)WndProc);

            // Initialize ImGui last, after all DirectX objects are created
            InitImGui();

            init = true;

            // Attempt to hook Present1 for games that use it instead of Present
            HookPresent1IfAvailable();
        }
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    // Проверяем все необходимые объекты
    if (!g_pd3dCommandQueue || !g_pd3dDevice || !g_frameContext || !g_pd3dSrvDescHeap)
        return oPresent(pSwapChain, SyncInterval, Flags);

    // Обработка изменения размера
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
    {
        g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
        WaitForLastSubmittedFrame();
        CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_ResizeWidth = g_ResizeHeight = 0;
        CreateRenderTarget();
    }

    // Toggle UI visibility with Insert
    if (GetAsyncKeyState(VK_INSERT) & 1)
        g_show_ui = !g_show_ui;

    // Начало нового кадра
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // UI draw
    if (g_show_ui)
    {
        ui::Draw();
    }
    ImGui::GetIO().MouseDrawCursor = g_show_ui;

    // Получаем текущий back buffer
    UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
    g_frameIndex = backBufferIdx;
    FrameContext &frameCtx = g_frameContext[backBufferIdx];

    // Сброс command allocator
    frameCtx.CommandAllocator->Reset();

    // Подготовка к рендерингу
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_frameContext[backBufferIdx].g_mainRenderTargetResource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Выполнение команд рендеринга
    g_pd3dCommandList->Reset(frameCtx.CommandAllocator, nullptr);
    g_pd3dCommandList->ResourceBarrier(1, &barrier);
    g_pd3dCommandList->OMSetRenderTargets(1, &g_frameContext[backBufferIdx].g_mainRenderTargetDescriptor, FALSE, nullptr);
    g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);

    // Рендеринг ImGui
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);

    // Возврат ресурса в состояние present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_pd3dCommandList->ResourceBarrier(1, &barrier);
    g_pd3dCommandList->Close();

    // Выполнение command list
    g_pd3dCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&g_pd3dCommandList));
    SignalFrameFence(frameCtx);

    return oPresent(pSwapChain, SyncInterval, Flags);
}

void __fastcall hkExecuteCommandLists(ID3D12CommandQueue *pCommandQueue, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
{
    if (!g_pd3dCommandQueue && IsDirectQueue(pCommandQueue))
    {
        g_pd3dCommandQueue = pCommandQueue;
    }

    oExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
}

HRESULT __fastcall hkResizeBuffers(IDXGISwapChain3 *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    // Если DirectX объекты еще не инициализированы, просто передаем вызов дальше
    if (!g_pd3dDevice || !g_pSwapChain)
    {
        LOG_INFO("ResizeBuffers called before DirectX initialization - passing through");
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    // Проверяем, что SwapChain соответствует нашему
    if (pSwapChain != g_pSwapChain)
    {
        LOG_INFO("ResizeBuffers called on different SwapChain - passing through");
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    LOG_INFO("Handling ResizeBuffers: %dx%d, BufferCount: %d", Width, Height, BufferCount);

    if (g_pd3dDevice)
    {
        ImGui_ImplDX12_InvalidateDeviceObjects();
    }

    // Ensure GPU finished with current resources for this frame
    g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
    WaitForLastSubmittedFrame();

    CleanupRenderTarget();

    // Recreate frame contexts and allocators according to new buffer count
    CreateFrameContextsAndAllocators(BufferCount);

    // Call original function
    HRESULT result = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (SUCCEEDED(result))
    {
        // Пересоздаем наши ресурсы только если успешно изменили размер
        CreateRenderTarget();
        if (g_pd3dDevice)
        {
            ImGui_ImplDX12_CreateDeviceObjects();
        }
        LOG_INFO("ResizeBuffers completed successfully");
    }
    else
    {
        LOG_ERROR("ResizeBuffers failed with error code: 0x%X", result);
    }

    return result;
}

// Hook for IDXGISwapChain1::Present1, mirrors hkPresent logic but preserves Present1 semantics
HRESULT __fastcall hkPresent1(IDXGISwapChain1 *pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
    static bool init1 = false;

    IDXGISwapChain3* sc3 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(&sc3))) && sc3)
    {
        if (!init1)
        {
            if (!g_pd3dCommandQueue)
            {
                sc3->Release();
                return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
            }

            if (SUCCEEDED(sc3->GetDevice(__uuidof(ID3D12Device), (void **)&g_pd3dDevice)))
            {
                DXGI_SWAP_CHAIN_DESC sdesc;
                sc3->GetDesc(&sdesc);
                window = sdesc.OutputWindow;
                NUM_BACK_BUFFERS = sdesc.BufferCount;

                // SRV Heap
                {
                    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    desc.NumDescriptors = 1;
                    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                    if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap))))
                    {
                        sc3->Release();
                        return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
                    }
                }

                // RTV Heap
                {
                    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
                    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                    desc.NumDescriptors = NUM_BACK_BUFFERS;
                    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                    desc.NodeMask = 1;
                    if (FAILED(g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap))))
                    {
                        sc3->Release();
                        return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
                    }
                }

                // Command Allocators (one per back buffer)
                CreateFrameContextsAndAllocators(sdesc.BufferCount);

                // Command List
                if (FAILED(g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&g_pd3dCommandList))))
                {
                    sc3->Release();
                    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
                }
                g_pd3dCommandList->Close();

                // Fence & Events
                if (FAILED(g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))))
                {
                    sc3->Release();
                    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
                }

                g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (g_fenceEvent == nullptr)
                {
                    sc3->Release();
                    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
                }

                g_hSwapChainWaitableObject = sc3->GetFrameLatencyWaitableObject();
                g_pSwapChain = sc3;

                // Create RenderTarget
                CreateRenderTarget();

                // Hook window procedure
                oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (__int3264)(LONG_PTR)WndProc);

                // Initialize ImGui last
                InitImGui();

                init1 = true;
            }
        }

        if (!g_pd3dCommandQueue || !g_pd3dDevice || !g_frameContext || !g_pd3dSrvDescHeap)
        {
            sc3->Release();
            return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
        }

        // Resize handling
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_frameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
            WaitForLastSubmittedFrame();
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
        g_frameIndex = backBufferIdx;
        FrameContext &frameCtx = g_frameContext[backBufferIdx];
        frameCtx.CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = g_frameContext[backBufferIdx].g_mainRenderTargetResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        g_pd3dCommandList->Reset(frameCtx.CommandAllocator, nullptr);
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->OMSetRenderTargets(1, &g_frameContext[backBufferIdx].g_mainRenderTargetDescriptor, FALSE, nullptr);
        g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (g_show_ui)
        {
            ui::Draw();
        }
        ImGui::GetIO().MouseDrawCursor = g_show_ui;
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->Close();

        g_pd3dCommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&g_pd3dCommandList));
        SignalFrameFence(frameCtx);

        sc3->Release();
    }

    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
}

HRESULT __fastcall hkSignal(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value)
{
    if (g_pd3dCommandQueue != nullptr && queue == g_pd3dCommandQueue)
    {
        g_fence = fence;
        g_fenceValue = value;
    }
    return oSignal(queue, fence, value);
    ;
}

bool InitD3D12Hook()
{
    LOG_INFO("Waiting for process initialization...");

    HANDLE d3d12Module = nullptr;
    HANDLE dxgiModule = nullptr;

    while (true)
    {
        d3d12Module = GetModuleHandleA("d3d12.dll");
        dxgiModule = GetModuleHandleA("dxgi.dll");

        if (d3d12Module && dxgiModule)
            break;

        if (WaitForSingleObject(GetCurrentProcess(), 1000) != WAIT_TIMEOUT)
        {
            LOG_ERROR("Process terminated while waiting for DirectX");
            return false;
        }

        LOG_INFO("Waiting for DirectX modules...");
    }

    LOG_INFO("DirectX modules found, initializing hooks...");

    try
    {
        auto kieroStatus = kiero::init(kiero::RenderType::D3D12);
        if (kieroStatus != kiero::Status::Success)
        {
            LOG_ERROR("Failed to initialize kiero");
            return false;
        }

        bool hooks_success = true;

        if (kiero::bind(54, (void **)&oExecuteCommandLists, hkExecuteCommandLists) != kiero::Status::Success)
        {
            LOG_ERROR("Failed to hook ExecuteCommandLists");
            hooks_success = false;
        }

        if (kiero::bind(58, (void **)&oSignal, hkSignal) != kiero::Status::Success)
        {
            LOG_ERROR("Failed to hook Signal");
            hooks_success = false;
        }

        if (kiero::bind(140, (void **)&oPresent, hkPresent) != kiero::Status::Success)
        {
            LOG_ERROR("Failed to hook Present");
            hooks_success = false;
        }

        if (kiero::bind(145, (void **)&oResizeBuffers, hkResizeBuffers) != kiero::Status::Success)
        {
            LOG_ERROR("Failed to hook ResizeBuffers");
            hooks_success = false;
        }

        if (!hooks_success)
        {
            LOG_ERROR("Failed to create one or more hooks");
            kiero::shutdown();
            return false;
        }

        LOG_INFO("D3D12 successfully hooked using kiero");
        return true;
    }
    catch (...)
    {
        LOG_ERROR("Exception during hook initialization");
        kiero::shutdown();
        return false;
    }
}

void ReleaseD3D12Hook()
{
    kiero::shutdown();

    if (g_pd3dDevice)
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (g_pd3dCommandQueue && g_fence && g_fenceEvent)
    {
        WaitForLastSubmittedFrame();
    }

    // Clean up render targets
    CleanupRenderTarget();

    // Release command allocators
    if (g_frameContext)
    {
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            if (g_frameContext[i].CommandAllocator)
            {
                g_frameContext[i].CommandAllocator->Release();
                g_frameContext[i].CommandAllocator = nullptr;
            }
        }
        delete[] g_frameContext;
        g_frameContext = nullptr;
    }

    if (g_pd3dCommandList)
    {
        g_pd3dCommandList->Release();
        g_pd3dCommandList = nullptr;
    }

    if (g_pd3dCommandQueue)
    {
        g_pd3dCommandQueue->Release();
        g_pd3dCommandQueue = nullptr;
    }

    // Close handles before releasing resources
    if (g_fenceEvent)
    {
        CloseHandle(g_fenceEvent);
        g_fenceEvent = nullptr;
    }

    if (g_hSwapChainWaitableObject)
    {
        CloseHandle(g_hSwapChainWaitableObject);
        g_hSwapChainWaitableObject = nullptr;
    }

    if (g_pd3dRtvDescHeap)
    {
        g_pd3dRtvDescHeap->Release();
        g_pd3dRtvDescHeap = nullptr;
    }

    if (g_pd3dSrvDescHeap)
    {
        g_pd3dSrvDescHeap->Release();
        g_pd3dSrvDescHeap = nullptr;
    }

    if (g_fence)
    {
        g_fence->Release();
        g_fence = nullptr;
    }

    if (oWndProc && window)
    {
        SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = nullptr;
    }

    g_pd3dDevice = nullptr;
    g_pSwapChain = nullptr;
    window = nullptr;

    NUM_BACK_BUFFERS = -1;
    g_frameIndex = 0;
    g_fenceValue = 0;
}
