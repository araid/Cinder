#pragma once

#include "cinder/Cinder.h"
#include "cinder/Log.h"

#if defined( CINDER_MSW )

#include <dshow.h>

// Due to a missing qedit.h in recent Platform SDKs, we've replicated the relevant contents here
// #include <qedit.h>
MIDL_INTERFACE( "0579154A-2B53-4994-B0D0-E773148EFF85" )
ISampleGrabberCB : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SampleCB(
		double SampleTime,
		IMediaSample *pSample ) = 0;

	virtual HRESULT STDMETHODCALLTYPE BufferCB(
		double SampleTime,
		BYTE *pBuffer,
		long BufferLen ) = 0;

};

MIDL_INTERFACE( "6B652FFF-11FE-4fce-92AD-0266B5D7C78F" )
ISampleGrabber : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetOneShot(
		BOOL OneShot ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetMediaType(
		const AM_MEDIA_TYPE *pType ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
		AM_MEDIA_TYPE *pType ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
		BOOL BufferThem ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
		/* [out][in] */ long *pBufferSize,
		/* [out] */ long *pBuffer ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
		/* [retval][out] */ IMediaSample **ppSample ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetCallback(
		ISampleGrabberCB *pCallback,
		long WhichMethodToCallback ) = 0;

};

EXTERN_C const CLSID CLSID_SampleGrabber;
EXTERN_C const IID IID_ISampleGrabber;
EXTERN_C const CLSID CLSID_NullRenderer;

//////////////////////////////  CALLBACK  ////////////////////////////////

//Callback class
class SampleGrabberCallback : public ISampleGrabberCB {
public:

	//------------------------------------------------
	SampleGrabberCallback()
	{
		InitializeCriticalSection( &critSection );
		freezeCheck = 0;


		bufferSetup = false;
		newFrame = false;
		latestBufferLength = 0;

		hEvent = CreateEvent( NULL, true, false, NULL );
	}


	//------------------------------------------------
	~SampleGrabberCallback()
	{
		ptrBuffer = NULL;
		DeleteCriticalSection( &critSection );
		CloseHandle( hEvent );
		if( bufferSetup ) {
			delete pixels;
		}
	}


	//------------------------------------------------
	bool setupBuffer( int numBytesIn )
	{
		if( bufferSetup ) {
			return false;
		}
		else {
			numBytes = numBytesIn;
			pixels = new unsigned char[numBytes];
			bufferSetup = true;
			newFrame = false;
			latestBufferLength = 0;
		}
		return true;
	}


	//------------------------------------------------
	STDMETHODIMP_( ULONG ) AddRef() { return 1; }
	STDMETHODIMP_( ULONG ) Release() { return 2; }


	//------------------------------------------------
	STDMETHODIMP QueryInterface( REFIID riid, void **ppvObject )
	{
		*ppvObject = static_cast<ISampleGrabberCB*>( this );
		return S_OK;
	}


	//This method is meant to have less overhead
	//------------------------------------------------
	STDMETHODIMP SampleCB( double Time, IMediaSample *pSample )
	{
		if( WaitForSingleObject( hEvent, 0 ) == WAIT_OBJECT_0 ) return S_OK;

		CI_LOG_V( "New sample arrived" );

		HRESULT hr = pSample->GetPointer( &ptrBuffer );

		if( hr == S_OK ) {
			latestBufferLength = pSample->GetActualDataLength();
			if( latestBufferLength == numBytes ) {
				EnterCriticalSection( &critSection );
				memcpy( pixels, ptrBuffer, latestBufferLength );
				newFrame = true;
				freezeCheck = 1;
				LeaveCriticalSection( &critSection );
				SetEvent( hEvent );
			}
			else {
				CI_LOG_E( "Buffer sizes do not match" );
			}
		}

		return S_OK;
	}


	//This method is meant to have more overhead
	STDMETHODIMP BufferCB( double Time, BYTE *pBuffer, long BufferLen )
	{
		return E_NOTIMPL;
	}

	int freezeCheck;

	int latestBufferLength;
	int numBytes;
	bool newFrame;
	bool bufferSetup;
	unsigned char * pixels;
	unsigned char * ptrBuffer;
	CRITICAL_SECTION critSection;
	HANDLE hEvent;
};

#endif // CINDER_MSW