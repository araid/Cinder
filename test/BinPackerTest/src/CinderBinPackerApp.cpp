#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/BinPacker.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class CinderBinPackerApp : public AppNative {
public:
	enum Mode { SINGLE_COPY, SINGLE_IN_PLACE, MULTI_COPY };

	void prepareSettings( Settings *settings );
	void setup();
	void keyDown( KeyEvent event );	
	void update();
	void draw();

	BinPacker								mBinPackerSingle;
	MultiBinPacker							mBinPackerMulti;

	std::vector<Area>						mUnpacked;

	std::vector<Area>						mPackedSingle;
	std::vector< std::map<unsigned, Area> >	mPackedMulti;

	Mode									mMode;
};

void CinderBinPackerApp::prepareSettings(Settings *settings)
{
	settings->setWindowSize(512, 512);
}

void CinderBinPackerApp::setup()
{
	mMode = SINGLE_COPY;

	mBinPackerSingle.setSize( 128, 128 );
	mBinPackerMulti.setSize( 128, 128 );
}

void CinderBinPackerApp::keyDown( KeyEvent event )
{
	switch( event.getCode() )
	{
	case KeyEvent::KEY_1:
		// enable single bin, copy mode
		mMode = SINGLE_COPY;
		break;
	case KeyEvent::KEY_2:
		// enable single bin, in-place mode
		mMode = SINGLE_IN_PLACE;
		break;
	case KeyEvent::KEY_3:
		// enable multi bin, copy mode
		mMode = MULTI_COPY;
		break;
	default:
		// add an Area of random size to mUnpacked
		int size = math<int>::pow(2, Rand::randInt(4, 7));
		mUnpacked.push_back( Area(0, 0, size, size) );
		break;
	}

	switch( mMode )
	{
	case SINGLE_COPY:
		// show the total number of Area's in the window title bar
		getWindow()->setTitle( "CinderBinPackerApp | Single Bin, Copy Mode " + ci::toString( mUnpacked.size() ) );

		try
		{ 
			// mPackedSingle will contain all Area's of mUnpacked in the exact same order,
			// but moved to a different spot in the bin. Unpacked will not be altered.
			// If rotated, (x1,y1) will be the lower left corner and (x2,y2) will be
			// the upper right corner of the Area.
			mPackedSingle = mBinPackerSingle.pack( mUnpacked ); 
		}
		catch(...) 
		{  
			// the bin is not large enough to contain all Area's, so let's
			// double the size...
			int size = mBinPackerSingle.getWidth();
			mBinPackerSingle.setSize( size << 1, size << 1 );

			/// ...and try again
			mPackedSingle = mBinPackerSingle.pack( mUnpacked ); 
		}
		break;
	case SINGLE_IN_PLACE:
		// show the total number of Area's in the window title bar
		getWindow()->setTitle( "CinderBinPackerApp | Single Bin, In-place Mode " + ci::toString( mUnpacked.size() ) );
		{
			// create a list of references
			std::vector<Area*>	refs;
			for(unsigned i=0;i<mUnpacked.size();++i)
				refs.push_back( &mUnpacked[i] );

			try
			{ 
				// This will apply packing to all Area's in mUnpacked
				mBinPackerSingle.pack( refs ); 
			}
			catch(...) 
			{  
				// the bin is not large enough to contain all Area's, so let's
				// double the size...
				int size = mBinPackerSingle.getWidth();
				mBinPackerSingle.setSize( size << 1, size << 1 );

				/// ...and try again
				mBinPackerSingle.pack( refs ); 
			}
		}
		break;
	case MULTI_COPY:
		// show the total number of Area's in the window title bar
		getWindow()->setTitle( "CinderBinPackerApp | Multi Bin, Copy Mode " + ci::toString( mUnpacked.size() ) );

		try
		{ 
			// mPackedMulti will contain a std::map for each generated bin. The map
			// contains key-value pairs. The key is the index of the original Area,
			// the value is the Area moved to a spot in the bin. Unpacked will not be altered.
			// If rotated, (x1,y1) will be the lower left corner and (x2,y2) will be
			// the upper right corner of the Area.
			mPackedMulti = mBinPackerMulti.pack( mUnpacked ); 
		}
		catch(...) 
		{  
			// will only throw if any of the input rects is too big to fit a single bin, 
			// which is not the case in this demo
		}
		break;
	}
}

void CinderBinPackerApp::update()
{
}

void CinderBinPackerApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 

	// draw all packed Area's
	Rand rnd;	

	switch( mMode )
	{
	case SINGLE_COPY:
		// draw the borders of the bin
		gl::color( Color( 1, 1, 0 ) );
		gl::drawStrokedRect( Rectf( Vec2f::zero(), mBinPackerSingle.getSize() ) );

		// packing has been done by copy, so use the mPackedSingle array
		for(unsigned i=0;i<mPackedSingle.size();++i) {
			rnd.seed(i+12345);
			gl::color( Color( (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f ) );
			gl::drawSolidRect( Rectf( mPackedSingle[i] ) );
		}
		break;
	case SINGLE_IN_PLACE:
		// draw the borders of the bin
		gl::color( Color( 1, 1, 0 ) );
		gl::drawStrokedRect( Rectf( Vec2f::zero(), mBinPackerSingle.getSize() ) );

		// packing has been done in-place, so use the mUnpacked array
		for(unsigned i=0;i<mUnpacked.size();++i) {
			rnd.seed(i+12345);
			gl::color( Color( (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f ) );
			gl::drawSolidRect( Rectf( mUnpacked[i] ) );
		}
		break;
	case MULTI_COPY:
		for(unsigned j=0;j<mPackedMulti.size();++j) {
			unsigned n = floor( getWindowWidth() / (float) mBinPackerMulti.getWidth() );

			gl::pushModelView();
			gl::translate( (j % n) * mBinPackerMulti.getWidth(), (j / n) * mBinPackerMulti.getHeight(), 0.0f );

			// draw the borders of the bin
			gl::color( Color( 1, 1, 0 ) );
			gl::drawStrokedRect( Rectf( Vec2f::zero(), mBinPackerMulti.getSize() ) );

			std::map<unsigned, Area>::const_iterator it = mPackedMulti[j].begin();
			while(it != mPackedMulti[j].end()) {
				rnd.seed(it->first+12345);
				gl::color( Color( (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f, (rnd.nextUint() & 0xFF) / 255.0f ) );
				gl::drawSolidRect( Rectf( it->second ) );

				++it;
			}

			gl::popModelView();
		}
		break;
	}
}

CINDER_APP_NATIVE( CinderBinPackerApp, RendererGl )
