/*
* Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/
#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dxgi1_6.h>
THIRD_PARTY_INCLUDES_END
#include "Microsoft/HideMicrosoftPlatformTypes.h"





DECLARE_LOG_CATEGORY_EXTERN(LogStreamlineDXGIRHI, Log, All);

class FStreamlineRHI;



/**
 * DXGI SwapChain proxy that wraps an IDXGISwapChain and implements up to IDXGISwapChain4.
 * Inspired by the Streamline SDK interposer, adapted for Unreal Engine types.
 */

#if __has_include(<dxgi1_7.h>)
#error "New DXGI, who dis? If DXGI 1.7 adds IDXGISwapChain5, extend "FStreamlineDXGISwapChainProxy" to implement this interface and then adjust this code to watch out for the next DGXI header"
#endif


class DECLSPEC_UUID("0f9f6ae5-097d-4b49-a019-1ee4efab8f69") STREAMLINEDXGIRHI_API FStreamlineDXGISwapChainProxy : public IDXGISwapChain4
{
public:
	FStreamlineDXGISwapChainProxy(IDXGISwapChain* InOriginal, const FStreamlineRHI* InStreamlineRHI = nullptr, bool bInIsStreamlineProxy = false);
	virtual ~FStreamlineDXGISwapChainProxy() = default;

	FStreamlineDXGISwapChainProxy(const FStreamlineDXGISwapChainProxy&) = delete;
	FStreamlineDXGISwapChainProxy& operator=(const FStreamlineDXGISwapChainProxy&) = delete;

	/** Get the underlying wrapped swapchain (which may itself be an SL-interposed swapchain) */
	IDXGISwapChain* GetWrappedSwapchain() const { return WrappedSwapchain; }

	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override final;
	ULONG   STDMETHODCALLTYPE AddRef() override final;
	ULONG   STDMETHODCALLTYPE Release() override final;

#pragma region IDXGIObject
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) override final;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown) override final;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData) override final;
	HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override final;
#pragma endregion
#pragma region IDXGIDeviceSubObject
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override final;
#pragma endregion
#pragma region IDXGISwapChain
	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override final;
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) override final;
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) override final;
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) override final;
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) override final;
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override final;
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) override final;
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput) override final;
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) override final;
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount) override final;
#pragma endregion
#pragma region IDXGISwapChain1
	HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) override final;
	HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) override final;
	HRESULT STDMETHODCALLTYPE GetHwnd(HWND* pHwnd) override final;
	HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void** ppUnk) override final;
	HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) override final;
	BOOL    STDMETHODCALLTYPE IsTemporaryMonoSupported() override final;
	HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) override final;
	HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor) override final;
	HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* pColor) override final;
	HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override final;
	HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* pRotation) override final;
#pragma endregion
#pragma region IDXGISwapChain2
	HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override final;
	HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* pWidth, UINT* pHeight) override final;
	HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override final;
	HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) override final;
	HANDLE  STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override final;
	HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix) override final;
	HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix) override final;
#pragma endregion
#pragma region IDXGISwapChain3
	UINT    STDMETHODCALLTYPE GetCurrentBackBufferIndex() override final;
	HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport) override final;
	HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override final;
	HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue) override final;
#pragma endregion
#pragma region IDXGISwapChain4
	HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData) override final;
#pragma endregion

	/** Returns true if the swap chain proxy is enabled (i.e., Slate callbacks for swapchain tracking are NOT used). */
	static bool IsEnabled();

	/**
	 * Wraps an existing swap chain with a proxy. The proxy takes ownership of a reference to the original.
	 * On output, *ppSwapChain is replaced with the proxy (and the original's ref from the caller is released).
	 *
	 * @param ppSwapChain       Pointer to the swap chain pointer to wrap. Updated in-place.
	 * @param InStreamlineRHI   Optional FStreamlineRHI to route DXGI errors through APIErrorHandler.
	 * @return true if wrapping succeeded, false if the swap chain was left unchanged.
	 */
	static bool WrapSwapChain(IDXGISwapChain** ppSwapChain, const FStreamlineRHI* InStreamlineRHI = nullptr, bool bIsStreamlineProxy = false);

	/** Typed overload for IDXGISwapChain1 */
	static bool WrapSwapChain(IDXGISwapChain1** ppSwapChain, const FStreamlineRHI* InStreamlineRHI = nullptr, bool bIsStreamlineProxy = false);

private:
	static bool IsSwapChainInterface(REFIID riid);

	IDXGISwapChain* WrappedSwapchain;
	TAtomic<LONG> RefCount;
	const FStreamlineRHI* StreamlineRHI;
	bool bIsStreamlineProxy;
};

