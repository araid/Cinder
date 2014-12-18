#include "cinder/msw/video/DirectShowPlayer.h"
#include "cinder/msw/video/VideoRenderer.h"
#include "cinder/Log.h"

#if defined(CINDER_MSW)

#include <dvdmedia.h> // for VIDEOINFOHEADER2
#include <shobjidl.h> 
#include <shlwapi.h>

namespace cinder {
namespace msw {
namespace video {


DirectShowPlayer::DirectShowPlayer( HRESULT &hr, HWND hwnd )
	: mRefCount( 0 ), m_state( STATE_NO_GRAPH ), m_hwnd( hwnd ), m_Width( 0 ), m_Height( 0 )
	, m_pGraph( NULL ), m_pControl( NULL ), m_pEvent( NULL ), m_pVideo( NULL )
{
	hr = S_OK;

	cinder::msw::initializeCom( COINIT_APARTMENTTHREADED );

	CI_LOG_V( "Created DirectShowPlayer." );
}

DirectShowPlayer::~DirectShowPlayer()
{
	TearDownGraph();

	CI_LOG_V( "Destroyed DirectShowPlayer." );
}

HRESULT DirectShowPlayer::QueryInterface( REFIID riid, void** ppv )
{
	AddRef();

	static const QITAB qit[] = {
		QITABENT( DirectShowPlayer, IUnknown ),
		{ 0 }
	};
	return QISearch( this, qit, riid, ppv );
}

ULONG DirectShowPlayer::AddRef()
{
	//CI_LOG_V( "DirectShowPlayer::AddRef():" << mRefCount + 1 );
	return InterlockedIncrement( &mRefCount );
}

ULONG DirectShowPlayer::Release()
{
	assert( mRefCount > 0 );
	//CI_LOG_V( "DirectShowPlayer::Release():" << mRefCount - 1 );

	ULONG uCount = InterlockedDecrement( &mRefCount );
	if( uCount == 0 ) {
		mRefCount = DESTRUCTOR_REF_COUNT;
		delete this;
	}
	return uCount;
}

HRESULT DirectShowPlayer::SetVideoRenderer( VideoRenderer *pVideo )
{
	if( pVideo == NULL )
		return E_POINTER;

	m_pVideo = pVideo;

	return S_OK;
}

// Open a media file for playback.
HRESULT DirectShowPlayer::OpenFile( PCWSTR pszFileName )
{
	HRESULT hr = S_OK;

	do {
		// Create a new filter graph. (This also closes the old one, if any.)
		hr = InitializeGraph();
		BREAK_ON_FAIL( hr );

		// Add the source filter to the graph.
		ScopedComPtr<IBaseFilter> pSource;
		hr = m_pGraph->AddSourceFilter( pszFileName, NULL, &pSource );
		BREAK_ON_FAIL( hr );

		// Try to render the streams.
		hr = RenderStreams( pSource );
		BREAK_ON_FAIL( hr );

		// Obtain width and height of the video.
		hr = m_pVideo->GetNativeVideoSize( &m_Width, &m_Height );
		BREAK_ON_FAIL( hr );
	} while( false );

	if( FAILED( hr ) )
		TearDownGraph();

	return hr;
}

HRESULT DirectShowPlayer::Close()
{
	HRESULT hr = S_OK;

	Stop();
	TearDownGraph();

	return hr;
}

HRESULT DirectShowPlayer::HandleEvent( UINT_PTR pEventPtr )
{
	HRESULT hr = S_OK;

	do {
		// Get the event type.
		long evCode = 0;
		LONG_PTR param1 = 0, param2 = 0;

		// Get the event status. If the operation that triggered the event 
		// did not succeed, the status is a failure code.
		while( SUCCEEDED( m_pEvent->GetEvent( &evCode, &param1, &param2, 0 ) ) ) {
			// Invoke the callback.

			switch( evCode ) {
			case EC_COMPLETE:
				CI_LOG_V( "EC_COMPLETE" );
				Stop();
				break;
			case EC_USERABORT:
				CI_LOG_V( "EC_USERABORT" );
				Stop();
				break;
			case EC_ERRORABORT:
				CI_LOG_E( "Playback error." );
				Stop();
				break;

#if _DEBUG
#define EVENT_CASE(name) case name: CI_LOG_V(#name); break
				EVENT_CASE( EC_ACTIVATE );
				EVENT_CASE( EC_BANDWIDTHCHANGE );
				EVENT_CASE( EC_BUFFERING_DATA );
				EVENT_CASE( EC_BUILT );
				EVENT_CASE( EC_CLOCK_CHANGED );
				EVENT_CASE( EC_CLOCK_UNSET );
				EVENT_CASE( EC_CODECAPI_EVENT );
				//EVENT_CASE( EC_COMPLETE );
				EVENT_CASE( EC_CONTENTPROPERTY_CHANGED );
				EVENT_CASE( EC_DEVICE_LOST );
				EVENT_CASE( EC_DISPLAY_CHANGED );
				EVENT_CASE( EC_END_OF_SEGMENT );
				EVENT_CASE( EC_EOS_SOON );
				EVENT_CASE( EC_ERROR_STILLPLAYING );
				//EVENT_CASE( EC_ERRORABORT );
				EVENT_CASE( EC_ERRORABORTEX );
				EVENT_CASE( EC_EXTDEVICE_MODE_CHANGE );
				EVENT_CASE( EC_FILE_CLOSED );
				EVENT_CASE( EC_FULLSCREEN_LOST );
				EVENT_CASE( EC_GRAPH_CHANGED );
				EVENT_CASE( EC_LENGTH_CHANGED );
				EVENT_CASE( EC_LOADSTATUS );
				EVENT_CASE( EC_MARKER_HIT );
				EVENT_CASE( EC_NEED_RESTART );
				EVENT_CASE( EC_NEW_PIN );
				EVENT_CASE( EC_NOTIFY_WINDOW );
				EVENT_CASE( EC_OLE_EVENT );
				EVENT_CASE( EC_OPENING_FILE );
				EVENT_CASE( EC_PALETTE_CHANGED );
				EVENT_CASE( EC_PAUSED );
				EVENT_CASE( EC_PLEASE_REOPEN );
				EVENT_CASE( EC_PREPROCESS_COMPLETE );
				EVENT_CASE( EC_PROCESSING_LATENCY );
				EVENT_CASE( EC_QUALITY_CHANGE );
				//EVENT_CASE( EC_RENDER_FINISHED );
				EVENT_CASE( EC_REPAINT );
				EVENT_CASE( EC_SAMPLE_LATENCY );
				//EVENT_CASE( EC_SAMPLE_NEEDED );
				EVENT_CASE( EC_SCRUB_TIME );
				EVENT_CASE( EC_SEGMENT_STARTED );
				EVENT_CASE( EC_SHUTTING_DOWN );
				EVENT_CASE( EC_SKIP_FRAMES );
				EVENT_CASE( EC_SNDDEV_IN_ERROR );
				EVENT_CASE( EC_SNDDEV_OUT_ERROR );
				EVENT_CASE( EC_STARVATION );
				EVENT_CASE( EC_STATE_CHANGE );
				EVENT_CASE( EC_STATUS );
				EVENT_CASE( EC_STEP_COMPLETE );
				EVENT_CASE( EC_STREAM_CONTROL_STARTED );
				EVENT_CASE( EC_STREAM_CONTROL_STOPPED );
				EVENT_CASE( EC_STREAM_ERROR_STILLPLAYING );
				EVENT_CASE( EC_STREAM_ERROR_STOPPED );
				EVENT_CASE( EC_TIMECODE_AVAILABLE );
				EVENT_CASE( EC_UNBUILT );
				//EVENT_CASE( EC_USERABORT );
				EVENT_CASE( EC_VIDEO_SIZE_CHANGED );
				EVENT_CASE( EC_VIDEOFRAMEREADY );
				EVENT_CASE( EC_VMR_RENDERDEVICE_SET );
				EVENT_CASE( EC_VMR_SURFACE_FLIPPED );
				EVENT_CASE( EC_VMR_RECONNECTION_FAILED );
				EVENT_CASE( EC_WINDOW_DESTROYED );
				EVENT_CASE( EC_WMT_EVENT );
				EVENT_CASE( EC_WMT_INDEX_EVENT );
#undef EVENT_CASE
#endif
			}

			// Free the event data.
			hr = m_pEvent->FreeEventParams( evCode, param1, param2 );
			BREAK_ON_FAIL( hr );
		}
	} while( false );

	return hr;
}

HRESULT DirectShowPlayer::Play()
{
	if( m_state != STATE_PAUSED && m_state != STATE_STOPPED ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Run();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_RUNNING;
	}
	return hr;
}

HRESULT DirectShowPlayer::Pause()
{
	if( m_state != STATE_RUNNING ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Pause();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_PAUSED;
	}
	return hr;
}

HRESULT DirectShowPlayer::Stop()
{
	if( m_state != STATE_RUNNING && m_state != STATE_PAUSED ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Stop();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_STOPPED;
	}
	return hr;
}

// EVR/VMR functionality

BOOL DirectShowPlayer::HasVideo() const
{
	return ( m_pVideo && m_pVideo->HasVideo() );
}

// Sets the destination rectangle for the video.
HRESULT DirectShowPlayer::UpdateVideoWindow( const LPRECT prc )
{
	if( m_pVideo ) {
		CI_LOG_V( "UpdateVideoWindow" );
		return m_pVideo->UpdateVideoWindow( m_hwnd, prc );
	}
	else {
		return S_OK;
	}
}

// Repaints the video. Call this method when the application receives WM_PAINT.
HRESULT DirectShowPlayer::Repaint( HDC hdc )
{
	if( m_pVideo ) {
		CI_LOG_V( "Repaint" );
		return m_pVideo->Repaint( m_hwnd, hdc );
	}
	else {
		return S_OK;
	}
}

// Notifies the video renderer that the display mode changed.
//
// Call this method when the application receives WM_DISPLAYCHANGE.

HRESULT DirectShowPlayer::DisplayModeChanged()
{
	if( m_pVideo ) {
		CI_LOG_V( "DisplayModeChanged" );
		return m_pVideo->DisplayModeChanged();
	}
	else {
		return S_OK;
	}
}


// Graph building

// Create a new filter graph. 
HRESULT DirectShowPlayer::InitializeGraph()
{
	HRESULT hr = S_OK;

	TearDownGraph();

	do {
		// Create the Filter Graph Manager.
		hr = CoCreateInstance( CLSID_FilterGraph, NULL,
							   CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &m_pGraph ) );
		BREAK_ON_FAIL( hr );

		hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &m_pControl ) );
		BREAK_ON_FAIL( hr );

		hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &m_pEvent ) );
		BREAK_ON_FAIL( hr );

		// Set up event notification.
		hr = m_pEvent->SetNotifyWindow( (OAHWND) m_hwnd, WM_PLAYER_EVENT, NULL );
		BREAK_ON_FAIL( hr );

		m_state = STATE_STOPPED;
	} while( false );

	return hr;
}

void DirectShowPlayer::TearDownGraph()
{
	// Stop sending event messages
	if( m_pEvent ) {
		m_pEvent->SetNotifyWindow( (OAHWND) NULL, NULL, NULL );
	}

	// Tear down renderer first.
	if( m_pVideo ) {
	}

	SafeRelease( m_pGraph );
	SafeRelease( m_pControl );
	SafeRelease( m_pEvent );

	//SafeDelete( m_pVideo ); // We no longer own this pointer, so leave it alone.

	m_state = STATE_NO_GRAPH;
}

/*
HRESULT DirectShowPlayer::CreateVideoRenderer()
{
HRESULT hr = E_FAIL;

enum { Try_EVR, Try_VMR9, Try_VMR7 };

for( DWORD i = Try_EVR; i <= Try_VMR7; i++ ) {
switch( i ) {
case Try_EVR:
CI_LOG_V( "Trying EVR..." );
m_pVideo = new ( std::nothrow ) RendererEVR();
break;

case Try_VMR9:
CI_LOG_V( "Trying VMR9..." );
m_pVideo = new ( std::nothrow ) RendererVMR9();
break;

case Try_VMR7:
CI_LOG_V( "Trying VMR7..." );
m_pVideo = new ( std::nothrow ) RendererVMR7();
break;
}

BREAK_ON_NULL( m_pVideo, E_OUTOFMEMORY );

hr = m_pVideo->AddToGraph( m_pGraph, m_hwnd );
if( SUCCEEDED( hr ) ) {
CI_LOG_V( "Success!" );
break;
}

SafeDelete( m_pVideo );
}
return hr;
}
*/

// Render the streams from a source filter. 

HRESULT DirectShowPlayer::RenderStreams( IBaseFilter *pSource )
{
	HRESULT hr = S_OK;

	BOOL bRenderedAnyPin = FALSE;

	do {
		ScopedComPtr<IFilterGraph2> pGraph2;
		hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &pGraph2 ) );
		BREAK_ON_FAIL( hr );

		// Add the video renderer to the graph
		assert( m_pVideo != NULL );
		hr = m_pVideo->AddToGraph( m_pGraph, m_hwnd );
		BREAK_ON_FAIL( hr );

		// Add the DSound Renderer to the graph.
		ScopedComPtr<IBaseFilter> pAudioRenderer;
		hr = AddFilterByCLSID( m_pGraph, CLSID_DSoundRender,
							   &pAudioRenderer, L"Audio Renderer" );
		BREAK_ON_FAIL( hr );

		// Enumerate the pins on the source filter.
		ScopedComPtr<IEnumPins> pEnum;
		hr = pSource->EnumPins( &pEnum );
		BREAK_ON_FAIL( hr );

		// Loop through all the pins
		IPin *pPin;
		while( S_OK == pEnum->Next( 1, &pPin, NULL ) ) {
			// First allow the video to connect this pin. It's OK if we fail.
			HRESULT hr2 = m_pVideo->ConnectFilters( m_pGraph, pPin );

			// Next, allow the graph to connect this pin. It's OK if we fail.
			if( FAILED( hr2 ) )
				hr2 = pGraph2->RenderEx( pPin, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL );

			pPin->Release();

			// It's OK if we fail some pins, if at least one pin renders.
			if( SUCCEEDED( hr2 ) ) {
				bRenderedAnyPin = TRUE;
			}
		}

		hr = m_pVideo->FinalizeGraph( m_pGraph );
		BREAK_ON_FAIL( hr );

		// Remove the audio renderer, if not used.
		BOOL bRemoved;
		hr = RemoveUnconnectedRenderer( m_pGraph, pAudioRenderer, &bRemoved );
		BREAK_ON_FAIL( hr );
	} while( false );

	// If we succeeded to this point, make sure we rendered at least one stream.
	if( SUCCEEDED( hr ) ) {
		if( !bRenderedAnyPin ) {
			hr = VFW_E_CANNOT_RENDER;
		}
	}

	return hr;
}

}
}
}

#endif // CINDER_MSW
