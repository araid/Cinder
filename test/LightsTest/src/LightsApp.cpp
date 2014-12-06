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
	~ShadowMap() {}

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
	gl::BatchRef           mRoom, mRoomShadow;
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
	bool                   mDebugDraw;
	bool                   mHardLights;
};

void LightsApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 1280, 720 );
	settings->disableFrameRate();
}

void LightsApp::setup()
{
	// Disable vertical sync, so we can determine the performance.
	gl::enableVerticalSync( false );

	// Load assets.
	gl::Texture2d::Format tfmt;
	tfmt.enableMipmapping( true );
	tfmt.setMinFilter( GL_LINEAR_MIPMAP_LINEAR );
	tfmt.setWrap( GL_REPEAT, GL_REPEAT );

	try {
		// Textures.
		mModulationTexture = gl::Texture2d::create( loadImage( loadAsset( "gobo1.png" ) ), tfmt );

		// Buffers.
		mLightDataBuffer = gl::Ubo::create( 32 * sizeof( Light::Data ), nullptr, GL_DYNAMIC_DRAW );
		mLightDataBuffer->bindBufferBase( 0 );

		// Shaders.
		mShader = gl::GlslProg::create( loadAsset( "lighting.vert" ), loadAsset( "lighting.frag" ) );
		mShaderShadow = gl::context()->getStockShader( gl::ShaderDef() );

		// Uniforms.
		mShader->uniformBlock( "uLights", 0 );
		mShader->uniform( "uModulationMap[0]", 1 );
		mShader->uniform( "uShadowMap[0]", 2 );
	}
	catch( const std::exception &e ) {
		console() << e.what() << std::endl;
		quit();
	}

	// Create 3D room using a box and then flipping the normals. This is only required if the shader is actually using normals.
	auto flipNormals = []( const vec3& normal ) { return -normal; };
	mRoom = gl::Batch::create( geom::AttribFn<vec3, vec3>( geom::Cube().size( vec3( 50 ) ), geom::NORMAL, geom::NORMAL, flipNormals ), mShader );
	mRoomShadow = gl::Batch::create( geom::Cube().size( vec3( 50 ) ), mShaderShadow );

	// Create an object in the room.
	mObject = gl::Batch::create( geom::Teapot().subdivisions( 60 ), mShader );
	mObjectShadow = gl::Batch::create( geom::Teapot().subdivisions( 20 ), mShaderShadow );

	// Initialize camera.
	mCamera.setEyePoint( vec3( 6, 11, -8 ) );
	mCamera.setCenterOfInterestPoint( vec3( 0, 1, 0 ) );

	// Create a spot light.
	SpotLightRef spot = Light::createSpot();
	mLights.push_back( spot );

	spot->setPosition( vec3( 0, 9, 0 ) );
	spot->setDirection( vec3( 0, -1, 0 ) );
	spot->pointAt( vec3( 0, 1, 5 ) );

	// The color describes the relative intensity of the light for each of the primary colors red, green and blue.
	// If you want the light to be brighter, change its intensity rather than its color.
	spot->setColor( Color::hex( 0xE68800 ) );

	// The spot ratio determines how wide the (outer) cone of the spot light is. A ratio of 1 means that
	// it is as wide as it is tall, which equals a spot angle of 45 degrees and a cone angle of 90 degrees.
	spot->setSpotRatio( 1 );

	// The hotspot defines an 'inner cone'. Within this cone, the light will have its maximum intensity
	// (although still subject to distance attenuation). Outside it, the intensity will gradually fade
	// to zero at the outer cone. The hotspot ratio can never exceed the spot ratio. Set them to be equal
	// if you don't want angular attenuation.
	spot->setHotspotRatio( 0 );

	// In real life, light intensity decreases exponentially the further away from the light source you are.
	// To mimic this, you can set distance attenuation parameters. Here, we apply a slight quadratic attenuation
	// (the light will be half as bright for every 5 units = 1 / sqrt(0.04) ), but for artistic purposes 
	// you can also specify linear attenuation, or none at all. 
	spot->setAttenuation( 0, 0.04f );

	// Range and intensity are two birds of the same feather: with increased intensity comes increased range,
	// and by increasing the range you actually would increase the intensity. However, we allow you to specify
	// intensity and range separately for ease of use. Try to keep the range as small as possible,
	// because this will increase shadow quality and performance.
	spot->setRange( 100 );
	spot->setIntensity( 2 );

	// If you want to make sure that the intensity will be zero at the specified range and distance attenuation,
	// you can use the 'calcIntensity' function to calculate it for you. You can optionally supply a threshold,
	// which is the intensity at full range. Larger threshold values will yield a higher intensity, but may produce
	// visible artefacts. In general, it is best to use the default threshold and simply adjust your distance attenuation.
	// spot->calcIntensity();

	// Alternatively, you can adjust the range based on the current intensity and distance attenuation, so that
	// the intensity will be zero at full range. You can optionally supply a threshold, which is the intensity 
	// at full range. Larger threshold values will yield a shorter range, but may produce visible artefacts.
	// In general, it is best to use the default threshold and simply adjust your distance attenuation.
	spot->calcRange();

	// The modulation map can be animated using the modulation parameters translateX, translateY, rotateZ and scale.
	// Each parameter is defined by an offset (or start value), a linear animation and an oscillating one, of which 
	// the latter has both an amplitude and a frequency. In this sample, we define a constant rotation.
	spot->getModulationParams().rotateZ = Light::AnimParam( 0, 0.25f, 0, 0 );

	// Enable (modulation and) shadows.
	//spot->enableModulation();
	spot->enableShadows();

	// Create point light.
	PointLightRef point = Light::createPoint();
	mLights.push_back( point );

	point->setPosition( vec3( -2.5f, 1, -2.5f ) );
	point->setRange( 10 );
	point->setAttenuation( 0, 0.5f );
	point->setColor( Color::hex( 0x7800CE ) );

	// Create capsule light.
	CapsuleLightRef capsule = Light::createCapsule();
	mLights.push_back( capsule );

	capsule->setLengthAndAxis( vec3( 5, 2.5f, -5 ), vec3( -5, 2.5f, -5 ) );
	capsule->setRange( 10 );
	capsule->setAttenuation( 0, 1 );
	capsule->setColor( Color::hex( 0xFF004F ) );

	// Create wedge light.
	WedgeLightRef wedge = Light::createWedge();
	mLights.push_back( wedge );

	wedge->setLengthAndAxis( vec3( -5, 9, 15 ), vec3( 5, 9, 15 ) );
	wedge->pointAt( vec3( 0, 1, 0 ) );
	wedge->setAttenuation( 0, 0.1f );
	wedge->setSpotRatio( 0.25f );
	wedge->setHotspotRatio( 0 );
	wedge->calcRange();
	wedge->setColor( Color::hex( 0x00AC6B ) );

	// Create directional light.
	DirectionalLightRef directional = Light::createDirectional();
	mLights.push_back( directional );

	directional->setDirection( vec3( 3, -2, -1 ) );
	directional->setIntensity( 0.1f );
	directional->setColor( Color::hex( 0x004D95 ) );

	// Create shadow map.
	mShadowMap = ShadowMap::create( 2048 );

	// Create debug sketch.
	mSketch = gl::Sketch::create( false );

	mAnimated = false;
	mDebugDraw = false;
	mHardLights = false;
}

void LightsApp::update()
{
	// Animate light sources.
	if( mAnimated ) {
		float t = 0.25f * float( getElapsedSeconds() );

		float x = 20.0f * math<float>::cos( 3.5f * t );
		float y = 1.0f + math<float>::sin( 0.3f * t );
		float z = 20.0f * math<float>::sin( t );
		dynamic_pointer_cast<SpotLight>( mLights[0] )->pointAt( vec3( x, y, z ) );
		dynamic_pointer_cast<WedgeLight>( mLights[3] )->pointAt( vec3( x, y, z ) );

		x = 5.0f * math<float>::cos( t );
		z = 5.0f * math<float>::sin( t );
		dynamic_pointer_cast<CapsuleLight>( mLights[2] )->setLengthAndAxis( vec3( 5.0f + x, 2.5f, z ), vec3( 5.0f - x, 2.5f, -z ) );
	}

	// Animate object.
	mTransform = glm::translate( vec3( 0, 1.5f, 0 ) );
	mTransform *= glm::rotate( float( getElapsedSeconds() ), glm::normalize( vec3( 0.1f, 0.5f, 0.2f ) ) );
	mTransform *= glm::scale( vec3( 3.0f ) );

	// Update debug sketch.
	mSketch->clear();

	if( mDebugDraw ) {
		for( size_t i = 0; i < mLights.size(); ++i ) {
			if( mLights[i]->isVisible() )
				mSketch->light( mLights[i] );
		}
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

	// Update uniform buffer object containing light data.
	int numVisible = 0;
	for( size_t i = 0; i < mLights.size(); ++i ) {
		if( mLights[i]->isVisible() ) {
			Light::Data light = mLights[i]->getData( getElapsedSeconds(), mCamera.getViewMatrix() );
			mLightDataBuffer->bufferSubData( numVisible * sizeof( Light::Data ), sizeof( Light::Data ), &light );
			numVisible++;
		}
	}

	// Render scene.
	{
		gl::pushMatrices();
		gl::setMatrices( mCamera );

		mSketch->draw();

		// Update shader uniforms.
		gl::ScopedGlslProg shader( mShader );
		mShader->uniform( "uLightCount", numVisible );
		mShader->uniform( "uSkyDirection", mCamera.getViewMatrix() * vec4( 0, 1, 0, 0 ) );

		// Bind textures and render.
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
	// Start user input.
	mMayaCam.setCurrentCam( mCamera );
	mMayaCam.mouseDown( event.getPos() );
}

void LightsApp::mouseDrag( MouseEvent event )
{
	// Handle user input (with support for trackpad).
	bool isZooming = event.isRightDown() || ( event.isShiftDown() && event.isLeftDown() );
	bool isPanning = !isZooming && event.isLeftDown();

	mMayaCam.mouseDrag( event.getPos(), isPanning, false, isZooming );
	mCamera = mMayaCam.getCamera();

	// Restrict the camera a bit.
	vec3 lookat = mCamera.getCenterOfInterestPoint();
	vec3 eye = mCamera.getEyePoint();
	eye.y = math<float>::max( eye.y, 1.0f );

	float d = math<float>::min( 200.0f, glm::distance( eye, lookat ) );
	eye = lookat + d * glm::normalize( eye - lookat );

	mCamera.setEyePoint( eye );
	mCamera.setCenterOfInterestPoint( lookat );
}

void LightsApp::keyDown( KeyEvent event )
{
	// Let's get some references for easy access.
	SpotLightRef spot = static_pointer_cast<SpotLight>( mLights[0] );
	PointLightRef point = static_pointer_cast<PointLight>( mLights[1] );
	CapsuleLightRef capsule = static_pointer_cast<CapsuleLight>( mLights[2] );
	WedgeLightRef wedge = static_pointer_cast<WedgeLight>( mLights[3] );
	DirectionalLightRef directional = static_pointer_cast<DirectionalLight>( mLights[4] );

	// Handle keyboard input.
	switch( event.getCode() ) {
	case KeyEvent::KEY_1:
		// Toggle spot light.
		spot->setVisible( !spot->isVisible() );
		break;
	case KeyEvent::KEY_2:
		// Toggle point light.
		point->setVisible( !point->isVisible() );
		break;
	case KeyEvent::KEY_3:
		// Toggle capsule light.
		capsule->setVisible( !capsule->isVisible() );
		break;
	case KeyEvent::KEY_4:
		// Toggle wedge light.
		wedge->setVisible( !wedge->isVisible() );
		break;
	case KeyEvent::KEY_5:
		// Toggle directional light.
		directional->setVisible( !directional->isVisible() );
		break;
	case KeyEvent::KEY_a:
		// Toggle light animation.
		mAnimated = !mAnimated;
		break;
	case KeyEvent::KEY_c:
		// Colorize lights.
		spot->setColor( Color::hex( 0xE68800 ) );
		point->setColor( Color::hex( 0x7800CE ) );
		capsule->setColor( Color::hex( 0xFF004F ) );
		wedge->setColor( Color::hex( 0x00AC6B ) );
		directional->setColor( Color::hex( 0x004D95 ) );
		break;
	case KeyEvent::KEY_m:
		// Toggle modulation map.
		spot->enableModulation( !spot->hasModulation() );
		break;
	case KeyEvent::KEY_s:
		// Toggle shadows.
		spot->enableShadows( !spot->hasShadows() );
		break;
	case KeyEvent::KEY_w:
		// White lights.
		for( size_t i = 0; i < mLights.size(); ++i )
			mLights[i]->setColor( 1, 1, 1 );
		break;
	case KeyEvent::KEY_h:
		// Toggle hotspot for spot and wedge lights.
		if( spot->getHotspotRatio() > 0 )
			spot->setHotspotRatio( 0 );
		else
			spot->setHotspotRatio( spot->getSpotRatio() );
		if( wedge->getHotspotRatio() > 0 )
			wedge->setHotspotRatio( 0 );
		else
			wedge->setHotspotRatio( wedge->getSpotRatio() );
		break;
	case KeyEvent::KEY_d:
		// Toggle distance attenuation for spot and wedge lights.
		mHardLights = !mHardLights;
		if( mHardLights ) {
			point->setAttenuation( 0.5f, 0 );
			capsule->setAttenuation( 0.5f, 0 );
			spot->setAttenuation( 0, 0 );
			wedge->setAttenuation( 0, 0 );
			spot->setRange( 100 );
			wedge->setRange( 100 );
		}
		else {
			point->setAttenuation( 0, 0.5f );
			capsule->setAttenuation( 0, 0.5f );
			spot->setAttenuation( 0, 0.04f );
			wedge->setAttenuation( 0, 0.04f );
			spot->calcRange();
			wedge->calcRange();
		}
		break;
	case KeyEvent::KEY_RETURN:
		// Toggle wireframes.
		mDebugDraw = !mDebugDraw;
		break;
	case KeyEvent::KEY_SPACE:
		// Reload shader.
		try {
			mShader = gl::GlslProg::create( loadAsset( "lighting.vert" ), loadAsset( "lighting.frag" ) );

			// Uniforms.
			mShader->uniformBlock( "uLights", 0 );
			mShader->uniform( "uModulationMap[0]", 1 );
			mShader->uniform( "uShadowMap[0]", 2 );

			// Batches.
			mRoom->replaceGlslProg( mShader );
			mObject->replaceGlslProg( mShader );
		}
		catch( const std::exception &e ) {
			console() << e.what() << std::endl;
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
	gl::pushModelMatrix();

	if( onlyShadowCasters ) {
		gl::translate( vec3( 0, 50, 0 ) );
		mRoomShadow->draw();
		gl::setModelMatrix( mTransform );
		mObjectShadow->draw();
	}
	else {
		gl::enableFaceCulling();

		gl::cullFace( GL_FRONT );
		gl::translate( vec3( 0, 50, 0 ) );
		mRoom->draw();

		gl::cullFace( GL_BACK );
		gl::setModelMatrix( mTransform );
		mObject->draw();

		gl::enableFaceCulling( false );
	}

	gl::popModelMatrix();
}

CINDER_APP_NATIVE( LightsApp, RendererGl( RendererGl::Options().msaa( 4 ) ) )
