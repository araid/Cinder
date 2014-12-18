#include "vld.h"

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "cinder/msw/video/RendererGlImpl.h"
#include "cinder/gl/Query.h"

using namespace ci;
using namespace ci::app;
using namespace ci::msw;
using namespace std;

class WindowsEnhancedVideoApp : public AppNative {
public:
	void prepareSettings( Settings* settings ) override;

	void setup() override;
	void shutdown() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

	void resize() override;
	void fileDrop( FileDropEvent event ) override;

	bool playVideo( const fs::path &path );
private:
	bool                     mUseMovieGl;

	video::MovieSurfaceRef   mMovieSurface;
	video::MovieGlRef        mMovieGl;

	glm::mat4                mTransform;
	gl::QueryTimeSwappedRef  mQuery;

	fs::path                 mPath;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1920, 1080 );

	mUseMovieGl = true;
}

void WindowsEnhancedVideoApp::setup()
{
	fs::path path = getOpenFilePath();
	playVideo( path );

	gl::enableVerticalSync( true );
	gl::clear();
	gl::color( 1, 1, 1 );

	mQuery = gl::QueryTimeSwapped::create();
}

void WindowsEnhancedVideoApp::shutdown()
{
	// Do we still need to explicitely destroy these?
	mMovieSurface.reset();
	mMovieGl.reset();
}

void WindowsEnhancedVideoApp::update()
{
}

void WindowsEnhancedVideoApp::draw()
{
	if( mMovieGl && mMovieGl->checkNewFrame() ) {
		gl::clear();

		mQuery->begin();
		mMovieGl->draw( 0, 0 );
		mQuery->end();
	}
	else if( mMovieSurface && mMovieSurface->checkNewFrame() ) {
		gl::clear();

		mQuery->begin();
		mMovieSurface->draw( 0, 0 );
		mQuery->end();
	}
}

void WindowsEnhancedVideoApp::mouseDown( MouseEvent event )
{
}

void WindowsEnhancedVideoApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_DELETE:
		mMovieGl.reset();
		mMovieSurface.reset();
		break;
	case KeyEvent::KEY_SPACE:
		if( mMovieGl ) {
			if( mMovieGl->isPlaying() )
				mMovieGl->stop();
			else mMovieGl->play();
		}
		if( mMovieSurface ) {
			if( mMovieSurface->isPlaying() )
				mMovieSurface->stop();
			else mMovieSurface->play();
		}
		break;
	case KeyEvent::KEY_F1:
		if( !mUseMovieGl && !mPath.empty() ) {
			mUseMovieGl = true;
			mMovieSurface.reset();
			playVideo( mPath );
		}
		break;
	case KeyEvent::KEY_F2:
		if( mUseMovieGl && !mPath.empty() ) {
			mUseMovieGl = false;
			mMovieGl.reset();
			playVideo( mPath );
		}
		break;
	}
}

void WindowsEnhancedVideoApp::resize()
{
	Area bounds = mMovieGl ? mMovieGl->getBounds() : mMovieSurface ? mMovieSurface->getBounds() : getWindowBounds();
	Area scaled = Area::proportionalFit( bounds, getWindowBounds(), true, true );
	mTransform = glm::translate( vec3( vec2( scaled.getUL() - bounds.getUL() ) + vec2( 0.5 ), 0 ) ) * glm::scale( vec3( vec2( scaled.getSize() ) / vec2( bounds.getSize() ), 1 ) );
	gl::setModelMatrix( mTransform );
}

void WindowsEnhancedVideoApp::fileDrop( FileDropEvent event )
{
	const fs::path& path = event.getFile( 0 );
	playVideo( path );
}

bool WindowsEnhancedVideoApp::playVideo( const fs::path &path )
{
	if( !path.empty() && fs::exists( path ) ) {
		// TODO: make sure the movie can play
		if( mUseMovieGl ) {
			mMovieGl = video::MovieGl::create( path );
			mMovieGl->play();
		}
		else {
			mMovieSurface = video::MovieSurface::create( path );
			mMovieSurface->play();
		}

		mPath = path;

		Area bounds = mMovieGl ? mMovieGl->getBounds() : mMovieSurface ? mMovieSurface->getBounds() : getWindowBounds();
		Area proportional = Area::proportionalFit( bounds, getDisplay()->getBounds(), true, false );
		getWindow()->setSize( proportional.getSize() );
		getWindow()->setPos( proportional.getUL() );

		std::string title = "WindowsEnhancedVideo";
		if( mUseMovieGl )
			title += " (MovieGl)";
		else
			title += " (MovieSurface)";

		if( ( mMovieGl && mMovieGl->isUsingDirectShow() ) || ( mMovieSurface && mMovieSurface->isUsingDirectShow() ) )
			title += " (DirectShow)";
		else
			title += " (Media Foundation)";

		getWindow()->setTitle( title );

		return true;
	}

	return false;
}

CINDER_APP_NATIVE( WindowsEnhancedVideoApp, RendererGl )
