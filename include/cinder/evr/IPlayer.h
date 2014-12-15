#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMswCom.h"

namespace cinder {
namespace msw {
namespace video {

//! Definitions.
const UINT WM_PLAYER_EVENT = WM_APP + 1;

// Forward declarations.
class IRenderer;

class IPlayer : public IUnknown {
public:
	IPlayer() {}
	virtual ~IPlayer() {}

	virtual HRESULT SetVideoRenderer( IRenderer *pVideo ) =0;

	virtual HRESULT OpenFile( PCWSTR pszFileName ) = 0;
	virtual HRESULT Close() = 0;

	virtual HRESULT Play() = 0;
	virtual HRESULT Pause() = 0;
	virtual HRESULT Stop() = 0;

	virtual HRESULT HandleEvent( UINT_PTR pEventPtr ) = 0;

	virtual UINT32  GetWidth() const = 0;
	virtual UINT32  GetHeight() const = 0;

	virtual BOOL    CheckNewFrame() const = 0;

	virtual bool CreateSharedTexture( int w, int h, int textureID ) = 0;
	virtual void ReleaseSharedTexture( int textureID ) = 0;
	virtual bool LockSharedTexture( int *pTextureID ) = 0;
	virtual bool UnlockSharedTexture( int textureID ) = 0;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif