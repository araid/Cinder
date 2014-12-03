#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Sketch.h"
#include "cinder/gl/Ubo.h"
#include "cinder/Camera.h"
#include "cinder/Light.h"
#include "cinder/MayaCamUI.h"


using namespace ci;
using namespace ci::app;
using namespace std;

typedef std::shared_ptr<class ShadowMap> ShadowMapRef;

class ShadowMap {
public:
	static ShadowMapRef create( int size ) { return ShadowMapRef( new ShadowMap{ size } ); }
	ShadowMap( int size )
	{
		reset( size );
	}

	void reset( int size )
	{
		gl::Texture2d::Format depthFormat;
		depthFormat.setInternalFormat( GL_DEPTH_COMPONENT32F );
		depthFormat.setMagFilter( GL_LINEAR );
		depthFormat.setMinFilter( GL_LINEAR );
		depthFormat.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );
		depthFormat.setCompareMode( GL_COMPARE_REF_TO_TEXTURE );
		depthFormat.setCompareFunc( GL_LEQUAL );
		mTextureShadowMap = gl::Texture2d::create( size, size, depthFormat );

		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_DEPTH_ATTACHMENT, mTextureShadowMap );
		mShadowMap = gl::Fbo::create( size, size, fboFormat );
	}

	const gl::FboRef&		getFbo() const { return mShadowMap; }
	const gl::Texture2dRef&	getTexture() const { return mTextureShadowMap; }

	float					getAspectRatio() const { return mShadowMap->getAspectRatio(); }
	ivec2					getSize() const { return mShadowMap->getSize(); }
private:
	gl::FboRef				mShadowMap;
	gl::Texture2dRef		mTextureShadowMap;
};

class LightsApp : public AppNative {
public:
	void prepareSettings( Settings *settings ) override;

	void setup() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

	void resize() override;

	void render( bool onlyShadowCasters = false );
private:
	gl::BatchRef           mFloor, mFloorShadow;
	gl::BatchRef           mObject, mObjectShadow;
	gl::SketchRef          mSketch;
	gl::GlslProgRef        mShader, mShaderShadow;
	gl::Texture2dRef       mModulationTexture;
	gl::UboRef             mLightDataBuffer;

	CameraPersp            mCamera;
	MayaCamUI              mMayaCam;

	std::vector<LightRef>  mLights;
	ShadowMapRef           mShadowMap;

	mat4                   mTransform;

	bool                   mAnimated;
};

void LightsApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 1280, 720 );
}

void LightsApp::setup()
{
	//
	disableFrameRate();
	gl::enableVerticalSync( false );

	// Load assets.
	gl::Texture2d::Format tfmt;
	tfmt.enableMipmapping( true );
	tfmt.setMinFilter( GL_LINEAR_MIPMAP_LINEAR );
	tfmt.setWrap( GL_REPEAT, GL_REPEAT );

	try {
		// Shaders.
		mShader = gl::GlslProg::create( loadAsset( "lighting.vert" ), loadAsset( "lighting.frag" ) );
		mShaderShadow = gl::context()->getStockShader( gl::ShaderDef() );

		// Textures.
		mModulationTexture = gl::Texture2d::create( loadImage( loadAsset( "gobo1.png" ) ), tfmt );

		// Buffers.
		mLightDataBuffer = gl::Ubo::create( 32 * sizeof( Light::Data ), nullptr, GL_DYNAMIC_DRAW );
		mLightDataBuffer->bindBufferBase( 0 );
		mShader->uniformBlock( "uLights", 0 );
	}
	catch( const std::exception &e ) {
		console() << e.what() << std::endl;
		quit();
	}

	// Create 3D objects.
	mFloor = gl::Batch::create( geom::Plane().size( vec2( 100 ) ), mShader );
	mObject = gl::Batch::create( geom::Teapot().subdivisions( 60 ), mShader );
	mObjectShadow = gl::Batch::create( geom::Teapot().subdivisions( 20 ), mShaderShadow );

	// Initialize camera.
	mCamera.setEyePoint( vec3( 10, 20, 10 ) );
	mCamera.setCenterOfInterestPoint( vec3( 0, 1, 0 ) );

	// Create a spot light.
	mLights.push_back( Light::createSpot() );
	SpotLightRef spot = dynamic_pointer_cast<SpotLight>( mLights.back() );
	spot->setPosition( vec3( 0, 9, 0 ) );
	spot->setDirection( vec3( 0, -1, 0 ) );
	spot->pointAt( 50.0f * glm::normalize( vec3( 1, 0, 1 ) ) );

	// The color describes the relative intensity of the light for each of the primary colors red, green and blue.
	// If you want the light to be brighter, change its intensity rather than its color.
	spot->setColor( 1, 1, 1 );

	// The spot ratio determines how wide the (outer) cone of the spot light is. A ratio of 1 means that
	// it is as wide as it is tall, which equals a spot angle of 45 degrees and a cone angle of 90 degrees.
	spot->setSpotRatio( 1 );

	// The hotspot defines an 'inner cone'. Within this cone, the light will have its maximum intensity
	// (although still subject to distance attenuation). Outside it, the intensity will gradually fade
	// to zero at the outer cone. The hotspot ratio can never exceed the spot ratio. Set them to be equal
	// if you don't want angular attenuation.
	spot->setHotspotRatio( 0.9f );

	// In real life, light intensity decreases exponentially the further away from the light source you are.
	// To mimic this, you can set distance attenuation parameters. Here, we apply a slight quadratic attenuation
	// (the light will be half as bright for every 5 units = 1 / sqrt(0.04) ), but for artistic purposes 
	// you can also specify linear attenuation, or none at all. 
	spot->setAttenuation( 0, 0.04f );

	// Range and intensity are two birds of the same feather: with increased intensity comes increased range,
	// and by increasing the range you actually would increase the intensity. However, we allow you to specify
	// intensity and range separately for ease of use. Try to keep the range as small as possible,
	// because this will increase shadow quality and performance.
	spot->setRange( 50 );
	spot->setIntensity( 1 );

	// If you want to make sure that the intensity will be zero at the specified range and distance attenuation,
	// you can use the 'calcIntensity' function to calculate it for you. You can optionally supply a threshold,
	// which is the intensity at full range. Larger threshold values will yield a higher intensity, but may produce
	// visible artefacts. In general, it is best to use the default threshold and simply adjust your distance attenuation.
	//mLight->calcIntensity();

	// Alternatively, you can adjust the range based on the current intensity and distance attenuation, so that
	// the intensity will be zero at full range. You can optionally supply a threshold, which is the intensity 
	// at full range. Larger threshold values will yield a shorter range, but may produce visible artefacts.
	// In general, it is best to use the default threshold and simply adjust your distance attenuation.
	//spot->calcRange();

	// The modulation map can be animated using the modulation parameters translateX, translateY, rotateZ and scale.
	// Each parameter is defined by an offset (or start value), a linear animation and an oscillating one, of which 
	// the latter has both an amplitude and a frequency. In this sample, we define a constant rotation.
	spot->getModulationParams().rotateZ = Light::AnimParam( 0, 0.25f, 0, 0 );

	// Enable modulation and shadow.
	//spot->enableModulation();
	spot->enableShadows();

	//
	mLights.push_back( Light::createPoint() );
	PointLightRef point = dynamic_pointer_cast<PointLight>( mLights.back() );
	point->setPosition( vec3( -2.5f, 1, -2.5f ) );
	point->setRange( 5 );
	point->setVisible( false );
	//*/

	//
	mLights.push_back( Light::createCapsule() );
	CapsuleLightRef capsule = dynamic_pointer_cast<CapsuleLight>( mLights.back() );
	capsule->setLengthAndAxis( vec3( 5, 1, -5 ), vec3( -5, 1, 5 ) );
	capsule->setRange( 5 );
	capsule->setVisible( false );
	//*/

	mLights.push_back( Light::createWedge() );
	WedgeLightRef wedge = dynamic_pointer_cast<WedgeLight>( mLights.back() );
	wedge->setLengthAndAxis( vec3( -5, 9, 15 ), vec3( 5, 9, 15 ) );
	wedge->pointAt( vec3( 0, 0, 10 ) );
	wedge->setAttenuation( 0, 0.1f );
	wedge->setSpotRatio( 0.25f );
	wedge->calcRange();
	wedge->setVisible( false );

	// Create shadow map.
	mShadowMap = ShadowMap::create( 2048 );

	// Create debug sketch.
	mSketch = gl::Sketch::create( false );

	mAnimated = false;
}

void LightsApp::update()
{
	if( mAnimated ) {
		// Animate light sources.
		float t = 0.25f * float( getElapsedSeconds() );
		{
			float x = 10.0f * math<float>::cos( 3.5f * t );
			float z = 10.0f * math<float>::sin( t );
			dynamic_pointer_cast<SpotLight>( mLights[0] )->pointAt( vec3( x, 0, z ) );
			dynamic_pointer_cast<WedgeLight>( mLights[3] )->pointAt( vec3( x, 0, z ) );
		}
	{
		float x = 5.0f * math<float>::cos( t );
		float z = 5.0f * math<float>::sin( t );
		dynamic_pointer_cast<CapsuleLight>( mLights[2] )->setLengthAndAxis( vec3( 5.0f + x, 1, z ), vec3( 5.0f - x, 1, -z ) );
	}
	}

	// Animate object.
	mTransform = glm::translate( vec3( 0, 1.5f, 0 ) );
	mTransform *= glm::rotate( float( getElapsedSeconds() ), glm::normalize( vec3( 0.1f, 0.5f, 0.2f ) ) );
	mTransform *= glm::scale( vec3( 3.0f ) );

	// Update shader uniforms.
	int numVisible = 0;
	for( size_t i = 0; i < mLights.size(); ++i ) {
		if( mLights[i]->isVisible() ) {
			Light::Data light = mLights[i]->getData( getElapsedSeconds(), mCamera.getViewMatrix() );
			mLightDataBuffer->bufferSubData( numVisible * sizeof( Light::Data ), sizeof( Light::Data ), &light );
			numVisible++;
		}
	}

	gl::ScopedGlslProg shader( mShader );
	mShader->uniform( "uLightCount", numVisible );
	mShader->uniform( "uModulationMap[0]", 1 );
	mShader->uniform( "uShadowMap[0]", 2 );
	mShader->uniform( "uSkyDirection", mCamera.getViewMatrix() * vec4( 0, 1, 0, 0 ) );

	// Update debug sketch.
	mSketch->clear();

	gl::color( 1, 1, 1 );
	mSketch->grid( 100, 10 );
	for( size_t i = 0; i < mLights.size(); ++i ) {
		if( mLights[i]->isVisible() )
			mSketch->light( *mLights[i].get() );
	}
}

void LightsApp::draw()
{
	gl::clear();
	gl::color( 1, 1, 1 );

	gl::enableDepthRead();
	gl::enableDepthWrite();

	// Render shadow map.
	if( mLights[0]->isVisible() ) {
		gl::ScopedFramebuffer shadowmap( mShadowMap->getFbo() );
		gl::ScopedViewport viewport( ivec2( 0 ), mShadowMap->getSize() );

		gl::clear();

		gl::pushMatrices();

		// TODO: pfffff....
		const SpotLightRef spot = static_pointer_cast<SpotLight>( mLights[0] );
		gl::setMatrices( *spot.get() );

		render( true );
	}

	// Render scene.
	{
		gl::pushMatrices();
		gl::setMatrices( mCamera );

		mSketch->draw();

		gl::ScopedTextureBind gobo( mModulationTexture, (uint8_t) 1 );
		gl::ScopedTextureBind shadowmap( mShadowMap->getTexture(), (uint8_t) 2 );
		render( false );

		gl::popMatrices();
	}

	gl::disableDepthWrite();
	gl::disableDepthRead();
}

void LightsApp::mouseDown( MouseEvent event )
{
	mMayaCam.setCurrentCam( mCamera );
	mMayaCam.mouseDown( event.getPos() );
}

void LightsApp::mouseDrag( MouseEvent event )
{
	mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
	mCamera = mMayaCam.getCamera();
}

void LightsApp::keyDown( KeyEvent event )
{
	SpotLightRef spot = static_pointer_cast<SpotLight>( mLights[0] );

	switch( event.getCode() ) {
	case KeyEvent::KEY_1:
		mLights[0]->setVisible( !mLights[0]->isVisible() );
		break;
	case KeyEvent::KEY_2:
		mLights[1]->setVisible( !mLights[1]->isVisible() );
		break;
	case KeyEvent::KEY_3:
		mLights[2]->setVisible( !mLights[2]->isVisible() );
		break;
	case KeyEvent::KEY_4:
		mLights[3]->setVisible( !mLights[3]->isVisible() );
		break;
	case KeyEvent::KEY_a:
		mAnimated = !mAnimated;
		break;
	case KeyEvent::KEY_c:
		mLights[0]->setColor( 1, 0, 0 );
		mLights[1]->setColor( 0, 1, 0 );
		mLights[2]->setColor( 0, 0, 1 );
		mLights[3]->setColor( 1, 1, 0 );
		break;
	case KeyEvent::KEY_m:
		spot->enableModulation( !spot->hasModulation() );
		break;
	case KeyEvent::KEY_s:
		spot->enableShadows( !spot->hasShadows() );
		break;
	case KeyEvent::KEY_w:
		mLights[0]->setColor( 1, 1, 1 );
		mLights[1]->setColor( 1, 1, 1 );
		mLights[2]->setColor( 1, 1, 1 );
		mLights[3]->setColor( 1, 1, 1 );
		break;
	case KeyEvent::KEY_h:
		if( spot->getHotspotRatio() > 0 )
			spot->setHotspotRatio( 0 );
		else
			spot->setHotspotRatio( spot->getSpotRatio() );
		break;
	case KeyEvent::KEY_d:
		if( spot->getAttenuation().y > 0 ) {
			spot->setAttenuation( 0, 0 );
			spot->setRange( 50 );
		}
		else {
			spot->setAttenuation( 0, 0.04f );
			spot->calcRange();
		}
		break;
	}
}

void LightsApp::resize()
{
	mCamera.setAspectRatio( getWindowAspectRatio() );
}

void LightsApp::render( bool onlyShadowCasters )
{
	if( onlyShadowCasters ) {
		gl::pushModelMatrix();
		gl::setModelMatrix( mTransform );
		mObjectShadow->draw();
		gl::popModelMatrix();
	}
	else {
		mFloor->draw();

		gl::pushModelMatrix();
		gl::setModelMatrix( mTransform );
		mObject->draw();
		gl::popModelMatrix();
	}
}

CINDER_APP_NATIVE( LightsApp, RendererGl )
