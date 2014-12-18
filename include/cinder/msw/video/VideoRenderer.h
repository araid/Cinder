/*
Copyright (c) 2014, The Cinder Project, All rights reserved.

This code is intended for use with the Cinder C++ library: http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"
#include "cinder/msw/video/EVRCustomPresenter.h"
#include "cinder/msw/video/SampleGrabber.h"

#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>
#include <evr.h>

namespace cinder {
namespace msw {
namespace video {

// Abstract class to manage the video renderer filter.
// Specific implementations handle the VMR-7, VMR-9, or EVR filter.
class VideoRenderer {
public:
	virtual ~VideoRenderer() {};
	virtual BOOL    HasVideo() const = 0;
	virtual HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) = 0;
	virtual HRESULT Repaint( HWND hwnd, HDC hdc ) = 0;
	virtual HRESULT DisplayModeChanged() = 0;
	virtual HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const = 0;
	virtual BOOL    CheckNewFrame() const = 0;

	// DirectShow support.
	virtual HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) = 0;
	virtual HRESULT FinalizeGraph( IGraphBuilder *pGraph ) = 0;
	virtual HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pPin ) = 0;

	virtual bool CreateSharedTexture( int w, int h, int textureID ) = 0;
	virtual void ReleaseSharedTexture( int textureID ) = 0;
	virtual bool LockSharedTexture( int *pTextureID ) = 0;
	virtual bool UnlockSharedTexture( int textureID ) = 0;
};

// Manages the VMR-7 video renderer filter.
class RendererVMR7 : public VideoRenderer {
	IVMRWindowlessControl   *m_pWindowless;

public:
	RendererVMR7();
	~RendererVMR7();
	BOOL    HasVideo() const override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	BOOL    CheckNewFrame() const override { return TRUE; /* TODO */ }

	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pPin ) override { return E_NOTIMPL; }

	bool CreateSharedTexture( int w, int h, int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	void ReleaseSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool LockSharedTexture( int *pTextureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool UnlockSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }
};


// Manages the VMR-9 video renderer filter.
class RendererVMR9 : public VideoRenderer {
	IVMRWindowlessControl9 *m_pWindowless;

public:
	RendererVMR9();
	~RendererVMR9();
	BOOL    HasVideo() const override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	BOOL    CheckNewFrame() const override { return TRUE; /* TODO */ }

	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pPin ) override { return E_NOTIMPL; }

	bool CreateSharedTexture( int w, int h, int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	void ReleaseSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool LockSharedTexture( int *pTextureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool UnlockSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }
};


// Manages the EVR video renderer filter.
class RendererEVR : public VideoRenderer {
	IBaseFilter*             m_pEVR;
	IMFVideoDisplayControl*  m_pVideoDisplay;
	EVRCustomPresenter*      m_pPresenter;

public:
	RendererEVR();
	~RendererEVR();
	BOOL    HasVideo() const override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	BOOL    CheckNewFrame() const override { assert( m_pPresenter != NULL ); return m_pPresenter->CheckNewFrame(); }

	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pPin ) override { return E_NOTIMPL; }

	bool CreateSharedTexture( int w, int h, int textureID ) override { assert( m_pPresenter != NULL ); return m_pPresenter->CreateSharedTexture( w, h, textureID ); }
	void ReleaseSharedTexture( int textureID ) override { assert( m_pPresenter != NULL ); m_pPresenter->ReleaseSharedTexture( textureID ); }
	bool LockSharedTexture( int *pTextureID ) override { assert( m_pPresenter != NULL ); return m_pPresenter->LockSharedTexture( pTextureID ); }
	bool UnlockSharedTexture( int textureID ) override { assert( m_pPresenter != NULL ); return m_pPresenter->UnlockSharedTexture( textureID ); }

	EVRCustomPresenter* GetPresenter() { return m_pPresenter; }
};


// Manages the SampleGrabber filter.
class RendererSampleGrabber : public VideoRenderer {
	IBaseFilter*            m_pNullRenderer;
	IBaseFilter*            m_pGrabberFilter;
	ISampleGrabber*         m_pGrabber;
	SampleGrabberCallback*  m_pCallBack;

public:
	RendererSampleGrabber();
	~RendererSampleGrabber();
	BOOL    HasVideo() const override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	BOOL    CheckNewFrame() const override { return m_pCallBack->HasNewFrame(); }

	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pPin ) override;

	bool CreateSharedTexture( int w, int h, int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	void ReleaseSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool LockSharedTexture( int *pTextureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool UnlockSharedTexture( int textureID ) override { throw std::runtime_error( "Not implemented" ); }

	SampleGrabberCallback* GetCallback() { return m_pCallBack; }
};

HRESULT RemoveUnconnectedRenderer( IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved );
HRESULT AddFilterByCLSID( IGraphBuilder *pGraph, REFGUID clsid, IBaseFilter **ppF, LPCWSTR wszName );
HRESULT InitializeEVR( IBaseFilter *pEVR, HWND hwnd, IMFVideoPresenter *pPresenter, IMFVideoDisplayControl **ppWc );
HRESULT InitWindowlessVMR9( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl9 **ppWc );
HRESULT InitWindowlessVMR( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl **ppWc );
HRESULT FindUnconnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin );
HRESULT FindConnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin );
HRESULT IsPinConnected( IPin *pPin, BOOL *pResult );
HRESULT IsPinDirection( IPin *pPin, PIN_DIRECTION dir, BOOL *pResult );
HRESULT MatchPin( IPin *pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL *pResult );
HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pOut, IBaseFilter *pDest );
HRESULT ConnectFilters( IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn );
HRESULT ConnectFilters( IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest );

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW
