/*
Copyright (c) 2014, Paul Houx - All rights reserved.
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

#include "cinder/Color.h"
#include "cinder/Matrix.h"
#include "cinder/Quaternion.h"
#include "cinder/Vector.h"

namespace cinder {

class DirectionalLight;
class PointLight;
class CapsuleLight;
class SpotLight;
class WedgeLight;

//////////////////////////////////////////////////////////////////////////////////////////////////////

class ILightPosition {
public:
	virtual ~ILightPosition() {}

	virtual const vec3& getPosition() const = 0;
	virtual vec3 getPosition( const mat4& transform ) const = 0;
	virtual void setPosition( const vec3& worldPosition ) = 0;
};

class ILightDirection {
public:
	virtual ~ILightDirection() {}

	virtual const vec3& getDirection() const = 0;
	virtual vec3 getDirection( const mat4& transform ) const = 0;
	virtual void setDirection( const vec3& direction ) = 0;
};

class ILightRange {
public:
	virtual ~ILightRange() {}

	virtual float getRange() const = 0;
	virtual void setRange( float range ) = 0;
	virtual bool calcRange( float threshold = 2.0f / 255.0f ) = 0;
	virtual bool calcIntensity( float threshold = 2.0f / 255.0f ) = 0;
};

class ILightLength {
public:
	virtual ~ILightLength() {}

	virtual float getLength() const = 0;
	virtual void setLength( float length ) = 0;
	virtual const vec3& getAxis() const = 0;
	virtual void setAxis( const vec3 &axis ) = 0;
	virtual void setLengthAndAxis( const vec3 &a, const vec3 &b ) = 0;
};

class ILightAttenuation {
public:
	virtual ~ILightAttenuation() {}

	virtual const vec2& getAttenuation() const = 0;
	virtual void setAttenuation( const vec2 &attenuation ) = 0;
	virtual void setAttenuation( float linear, float quadratic ) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class Light> LightRef;

class Light {
public:
	//! Note: values are used as bitmask in shader and should also be sortable!!
	typedef enum Type { Directional = 0x0, Point = 0x1, Capsule = 0x2, Spot = 0x4, Wedge = 0x8 };

	//! This structure should be tightly packed and conform to the std140 layout as defined in the shader!!
	struct Data {
		Data()
			: intensity( 1 ), direction( 0, -1, 0 ), range( 100 ), length( 0 ), color( 1 ), angle( 1 ) {}

		vec3   position;
		float  intensity;
		vec3   direction;			// normalized
		float  range;
		vec4   length;				// xyz = direction, w = length (capsule and wedge only)
		vec4   color;				// x = red, y = green, z = blue, w = luminance
		vec2   attenuation;			// x = linear coefficient, y = quadratic coefficient
		vec2   angle;				// x = cos(outer angle), y = cos(inner angle)
		mat4   shadowMatrix;		// converts to shadow map space
		mat4   modulationMatrix;	// converts to modulation map space
		int    shadowIndex;			// index into an array of sampler2DShadow samplers
		int    modulationIndex;		// index into an array of sampler2D samplers
		int    flags;				// 0-3 = light type, 4 = modulation enabled, 5 = shadow enabled
		int    reserved;

		enum Flags { ModulationEnabled = 0x10, ShadowEnabled = 0x020 };
	};

	struct AnimParam {
		AnimParam()
			: offset( 0 ), linear( 0 ), amplitude( 0 ), frequency( 0 ) {}
		AnimParam( float offset, float linear, float amplitude, float frequency )
			: offset( offset ), linear( linear ), amplitude( amplitude ), frequency( frequency ) {}

		float offset;
		float linear;
		float amplitude;
		float frequency;

		float evaluate( float time ) const
		{
			return offset + time * linear + amplitude * sinf( time * frequency );
		}
	};

public:
	Light( Type type )
		: mType( type )
		, mIntensity( 1 )
		, mColor( 1, 1, 1 )
		, mFlags( type )
		, mVisible( true )
	{
	}
	virtual ~Light() {}

	static std::shared_ptr<DirectionalLight> createDirectional() { return std::make_shared<DirectionalLight>(); }
	static std::shared_ptr<PointLight> createPoint() { return std::make_shared<PointLight>(); }
	static std::shared_ptr<CapsuleLight> createCapsule() { return std::make_shared<CapsuleLight>(); }
	static std::shared_ptr<SpotLight> createSpot() { return std::make_shared<SpotLight>(); }
	static std::shared_ptr<WedgeLight> createWedge() { return std::make_shared<WedgeLight>(); }

	//! Returns the type of the light (directional, point, capsule, spot or wedge).
	virtual Type getType() const { return mType; }

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	virtual Data getData( double time = 0.0, const mat4 &transform = mat4() ) const = 0;

	bool isVisible() const { return mVisible; }
	void setVisible( bool visible = true ) { mVisible = visible; }

	//! Returns the intensity of the light.
	float getIntensity() const { return mIntensity; }
	//! Adjust the intensity of the spotlight, directly affecting its effective range based on the current distance attenuation parameters.
	void setIntensity( float intensity ) { mIntensity = intensity; }

	//! Returns the relative intensities for red, green and blue, which will always be within the range [0, 1].
	const Color& getColor() const { return mColor; }
	//! Set the relative intensity for red, green and blue. Values will be kept within the range [0, 1] and the light's intensity will be adjusted for brighter lights.
	void setColor( const Color& color );
	//! Set the relative intensity for red, green and blue. Values will be kept within the range [0, 1] and the light's intensity will be adjusted for brighter lights.
	void setColor( float r, float g, float b ) { setColor( Color( r, g, b ) ); }

	//! Returns TRUE if the light is casting shadows.
	bool hasShadows() const { return ( mFlags & Data::ShadowEnabled ) > 0; }
	//! Returns TRUE if the light is modulated by a texture.
	bool hasModulation() const { return ( mFlags & Data::ModulationEnabled ) > 0; }

	//! Comparator for the std::sort function.
	bool operator<( const Light& rhs ) const { return (int) mType < (int) rhs.mType; }

protected:
	static bool calcRange( float intensity, const vec2 &attenuation, float *range, float threshold );
	static bool calcIntensity( float range, const vec2 &attenuation, float *intensity, float threshold );

private:
	void updateMatrices() const;

private:
	Type       mType;

protected:
	Color      mColor;
	float      mIntensity;
	int        mFlags;
	bool       mVisible;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class DirectionalLight> DirectionalLightRef;

class DirectionalLight : public Light, public ILightDirection {
public:
	DirectionalLight()
		: Light( Directional ), mDirection( 0, -1, 0 ) {}
	~DirectionalLight() {}

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	Data getData( double time = 0.0, const mat4 &transform = mat4() ) const override;

	const vec3& getDirection() const override { return mDirection; }
	vec3 getDirection( const mat4& transform ) const override { return vec3( transform * vec4( mDirection, 0 ) ); }
	void setDirection( const vec3& direction ) override { mDirection = glm::normalize( direction ); }
private:
	vec3       mDirection;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class PointLight> PointLightRef;

class PointLight : public Light, public ILightPosition, public ILightRange, public ILightAttenuation {
public:
	PointLight()
		: Light( Point ), mPosition( 0 ), mRange( 100 ), mAttenuation( 0 ) {}
	virtual ~PointLight() {}

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	virtual Data getData( double time = 0.0, const mat4 &transform = mat4() ) const override;

	const vec3& getPosition() const override { return mPosition; }
	vec3 getPosition( const mat4& transform ) const override { return vec3( transform * vec4( mPosition, 1 ) ); }
	void setPosition( const vec3& worldPosition ) override { mPosition = worldPosition; }

	/*! Returns the range of the light, which is a function of the intensity and the distance attenuation.
		Surfaces beyond the range do not receive any light.*/
	float getRange() const override { return mRange; }
	/*! Adjust the intensity of the light, based on the current distance attenuation parameters, so that surfaces beyond the range do not receive any light.*/
	void setRange( float range ) override { mRange = range; }

	//! Calculates the range of the light, based on the intensity and distance attenuation. Returns TRUE on success.
	bool calcRange( float threshold = 2.0f / 255.0f ) override { return Light::calcRange( mIntensity, mAttenuation, &mRange, threshold ); }
	//! Calculates the required intensity of the light for the light's range and attenuation. Returns TRUE on success.
	bool calcIntensity( float threshold = 2.0f / 255.0f ) override { return Light::calcIntensity( mRange, mAttenuation, &mIntensity, threshold ); }

	//! Returns the linear and quadratic distance attenuation.
	const vec2& getAttenuation() const override { return mAttenuation; }
	//! Set constant, linear and quadratic distance \a attenuation. 
	void setAttenuation( const vec2 &attenuation ) override { mAttenuation = attenuation; }
	//! Set \a constant, \a linear and \a quadratic distance attenuation. 
	void setAttenuation( float linear, float quadratic ) override { setAttenuation( vec2( linear, quadratic ) ); }

protected:
	PointLight( Type type )
		: Light( type ) {}

protected:
	vec3       mPosition;
	float      mRange;
	vec2       mAttenuation;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class CapsuleLight> CapsuleLightRef;

class CapsuleLight : public PointLight, public ILightLength {
public:
	CapsuleLight()
		: PointLight( Capsule ), mLength( 0 ), mAxis( 1, 0, 0 ) {}
	~CapsuleLight() {}

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	Data getData( double time = 0.0, const mat4 &transform = mat4() ) const override;

	float getLength() const override { return mLength; }
	void setLength( float length ) override { mLength = math<float>::max( 0, length ); }

	const vec3& getAxis() const override { return mAxis; }
	void setAxis( const vec3 &axis ) override { mAxis = glm::normalize( axis ); }

	void setLengthAndAxis( const vec3 &a, const vec3 &b ) override
	{
		vec3 line = b - a;
		mAxis = glm::normalize( line );
		mLength = glm::length( line );
		setPosition( ( a + b ) * 0.5f );
	}

private:
	float mLength;
	vec3  mAxis;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class SpotLight> SpotLightRef;

class SpotLight : public Light, public ILightPosition, public ILightDirection, public ILightRange, public ILightAttenuation {
public:
	struct ModulationParams {
		ModulationParams() : scale( 1, 0, 0, 0 ) {}

		mat4 toMat4( float time ) const
		{
			float x = translateX.evaluate( time );
			float y = translateY.evaluate( time );
			float z = rotateZ.evaluate( time );
			float s = scale.evaluate( time );

			mat4 transform;
			transform *= glm::translate( vec3( x + 0.5f, y + 0.5f, 0.5f ) );
			transform *= glm::scale( vec3( 0.5f * s ) );
			transform *= glm::rotate( z, vec3( 0, 0, 1 ) );

			return transform;
		}

		AnimParam translateX;
		AnimParam translateY;
		AnimParam rotateZ;
		AnimParam scale;
	};

public:
	SpotLight()
		: Light( Spot ), mPosition( 0 ), mDirection( 0, -1, 0 ), mRange( 100 ), mSpotRatio( 1 ), mHotspotRatio( 1 )
		, mAttenuation( 0 ), mPointAt( 0 ), mIsPointingAt( false ), mModulationIndex( 0 ), mShadowIndex( 0 ), mIsDirty( true )
	{
	}
	virtual ~SpotLight() {}

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	virtual Data getData( double time = 0.0, const mat4 &transform = mat4() ) const override;

	const vec3& getPosition() const override { return mPosition; }
	vec3 getPosition( const mat4& transform ) const override { return vec3( transform * vec4( mPosition, 1 ) ); }
	void setPosition( const vec3& worldPosition ) override { mIsDirty = true; mPosition = worldPosition; if( mIsPointingAt ) pointAt( mPointAt ); }

	const vec3& getDirection() const override { return mDirection; }
	vec3 getDirection( const mat4& transform ) const override { return vec3( transform * vec4( mDirection, 0 ) ); }
	virtual void setDirection( const vec3& direction ) override { mIsDirty = true; mIsPointingAt = false; mDirection = glm::normalize( direction ); }

	/*! Returns the range of the light, which is a function of the intensity and the distance attenuation.
	Surfaces beyond the range do not receive any light.*/
	float getRange() const override { return mRange; }
	/*! Adjust the intensity of the light, based on the current distance attenuation parameters, so that surfaces beyond the range do not receive any light.*/
	void setRange( float range ) override { mRange = range; }

	//! Calculates the range of the light, based on the intensity and distance attenuation. Returns TRUE on success.
	bool calcRange( float threshold = 2.0f / 255.0f ) override { return Light::calcRange( mIntensity, mAttenuation, &mRange, threshold ); }
	//! Calculates the required intensity of the light for the light's range and attenuation. Returns TRUE on success.
	bool calcIntensity( float threshold = 2.0f / 255.0f ) override { return Light::calcIntensity( mRange, mAttenuation, &mIntensity, threshold ); }

	//! Returns true if the light remains pointed at the same location when moved.
	bool isPointingAt() const { return mIsPointingAt; }
	//!
	virtual void pointAt( const vec3& point ) { mIsDirty = true; mIsPointingAt = true; mPointAt = point; mDirection = glm::normalize( mPointAt - mPosition ); }

	//! Returns the spot angle in radians. The spot angle defines the outer cone of the spot light, beyond which there is no light.
	float getSpotAngle() const { return math<float>::atan( mSpotRatio ); }
	//! Set the spot angle in radians. The spot angle defines the outer cone of the spot light, beyond which there is no light.
	void setSpotAngle( float radians ) { mSpotRatio = math<float>::tan( radians ); }
	/*! Returns the spot ratio, which defines the spot angle relative to the range of the spot.
	The spot angle defines the outer cone of the spot light, beyond which there is no light. A ratio of 1 is equal to an angle of 45 degrees.*/
	float getSpotRatio() const { return mSpotRatio; }
	/*! Set the spot ratio, which defines the spot angle relative to the range of the spot.
	The spot angle defines the outer cone of the spot light, beyond which there is no light. A ratio of 1 is equal to an angle of 45 degrees.*/
	void setSpotRatio( float ratio ) { mSpotRatio = math<float>::max( 0, ratio ); mIsDirty = true; }

	/*! Returns the hotspot angle in radians. The hotspot angle defines the inner cone of the spot light. Light inside this inner cone
	will have the maximum intensity. The light will be attenuated between the inner and outer cone of the spot light.*/
	float getHotspotAngle() const { return math<float>::atan( math<float>::min( mHotspotRatio, mSpotRatio ) ); }
	/*! Set the hotspot angle in radians. The hotspot angle defines the inner cone of the spot light. Light inside this inner cone
	will have the maximum intensity. The light will be attenuated between the inner and outer cone of the spot light.*/
	void setHotspotAngle( float radians ) { mHotspotRatio = math<float>::tan( radians ); }
	/*! Returns the hotspot ratio, which defines the hotspot angle relative to the range of the spot. A ratio of 1 is equal to an angle of 45 degrees.
	The hotspot angle defines the inner cone of the spot light. Light inside this inner cone will have the maximum intensity.
	The light will be attenuated between the inner and outer cone of the spot light.*/
	float getHotspotRatio() const { return math<float>::min( mHotspotRatio, mSpotRatio ); }
	/*! Set the hotspot ratio, which defines the hotspot angle relative to the range of the spot. A ratio of 1 is equal to an angle of 45 degrees.
	The hotspot angle defines the inner cone of the spot light. Light inside this inner cone will have the maximum intensity.
	The light will be attenuated between the inner and outer cone of the spot light.*/
	void setHotspotRatio( float ratio ) { mHotspotRatio = math<float>::max( 0, ratio ); }

	//! Return the light's cone parameters: x = cos(outer angle), y = cos(inner angle)
	vec2 getConeParams() const;

	//! Returns the linear and quadratic distance attenuation.
	const vec2& getAttenuation() const override { return mAttenuation; }
	//! Set constant, linear and quadratic distance \a attenuation. 
	void setAttenuation( const vec2 &attenuation ) override { mAttenuation = attenuation; }
	//! Set \a constant, \a linear and \a quadratic distance attenuation. 
	void setAttenuation( float linear, float quadratic ) override { setAttenuation( vec2( linear, quadratic ) ); }

	//! Returns the light's view matrix.
	const mat4& getViewMatrix() const { updateMatrices(); return mViewMatrix; }
	//! Returns the light's projection matrix.
	const mat4& getProjectionMatrix() const { updateMatrices(); return mProjectionMatrix; }
	//! Returns a matrix that converts world coordinates to shadow map coordinates.
	const mat4& getShadowMatrix() const { updateMatrices(); return mShadowMatrix; }
	//! Returns a matrix that converts world coordinates to modulation map coordinates.
	mat4 getModulationMatrix( double time ) const;

	//! Returns the modulation map animation parameters.
	const ModulationParams& getModulationParams() const { return mModulationParams; }
	//! Returns the modulation map animation parameters.
	ModulationParams& getModulationParams() { return mModulationParams; }
	//! Set the modulation map animation parameters.
	void setModulationParams( const ModulationParams &params ) { mModulationParams = params; }

	//! Enables or disables shadow casting for this light.
	void enableShadows( bool enabled = true ) { if( enabled ) mFlags |= Data::ShadowEnabled; else mFlags &= ~Data::ShadowEnabled; }
	//! Enables or disables the modulation texture for this light.
	void enableModulation( bool enabled = true ) { if( enabled ) mFlags |= Data::ModulationEnabled; else mFlags &= ~Data::ModulationEnabled; }

protected:
	SpotLight( Type type )
		: Light( type ), mPosition( 0 ), mDirection( 0, -1, 0 ), mRange( 100 ), mSpotRatio( 1 ), mHotspotRatio( 1 )
		, mAttenuation( 0 ), mPointAt( 0 ), mIsPointingAt( false ), mModulationIndex( 0 ), mShadowIndex( 0 ), mIsDirty( true )
	{
	}

private:
	void updateMatrices() const;

protected:
	vec3       mPosition;
	vec3       mDirection;
	float      mRange;
	float      mSpotRatio;
	float      mHotspotRatio;
	vec2       mAttenuation;
	vec3       mPointAt;
	bool       mIsPointingAt;

	uint8_t    mModulationIndex;
	uint8_t    mShadowIndex;

	ModulationParams   mModulationParams;

	mutable bool       mIsDirty;
	mutable mat4       mViewMatrix;;
	mutable mat4       mProjectionMatrix;
	mutable mat4       mShadowMatrix;

	static const mat4  sBiasMatrix;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<class WedgeLight> WedgeLightRef;

class WedgeLight : public SpotLight, public ILightLength {
public:
	WedgeLight()
		: SpotLight( Wedge ), mLength( 0 ), mAxis( 1, 0, 0 ) {}
	~WedgeLight() {}

	/*! Returns a structure containing all data for this light as required by the shader.
		You can optionally specify a \a time in seconds for animation effects. Light position,
		direction, axis and the matrices are defined in world space. If you rather work in
		view space, simply supply the camera's view matrix as the \a transform.*/
	Data getData( double time = 0.0, const mat4 &transform = mat4() ) const override;

	//! Set the \a direction of the light. Direction as automatically adjusted to be perpendicular to the light's axis.
	void setDirection( const vec3& direction ) override
	{
		mIsDirty = true;
		mIsPointingAt = false;

		// Safely normalize direction.
		vec3 d = direction - glm::dot( direction, mAxis ) * mAxis;
		float length = glm::length( d );
		if( length > 0 )
			length = 1 / length;
		mDirection = length * d;
	}

	//! Set the direction of the light by pointing at a a specific \a point. Direction as automatically adjusted to be perpendicular to the light's axis.
	void pointAt( const vec3& point )
	{
		mIsDirty = true;
		mIsPointingAt = true;
		mPointAt = point;

		vec3 direction = mPointAt - mPosition;
		direction -= glm::dot( direction, mAxis ) * mAxis;

		// Safely normalize direction.
		float length = glm::length( direction );
		if( length > 0 )
			length = 1 / length;
		mDirection = length * direction;
	}

	float getLength() const override { return mLength; }
	//! Set the light's \a length.
	void setLength( float length ) override { mLength = math<float>::max( 0, length ); }

	const vec3& getAxis() const override { return mAxis; }
	//! Set the light's \a axis, which is the direction in which the light is stretched. This will also affect the direction in which the light is pointed!
	void setAxis( const vec3 &axis ) override
	{
		// Safely normalize axis.
		float length = glm::length( axis );
		if( length > 0 )
			length = 1 / length;
		mAxis = length * axis;

		if( mIsPointingAt )
			pointAt( mPointAt );
		else {
			vec3 direction = mDirection - glm::dot( mDirection, mAxis ) * mAxis;

			// Safely normalize direction.
			float length = glm::length( direction );
			if( length > 0 )
				length = 1 / length;
			mDirection = length * direction;
		}
	}

	//! Set the light's \a length and \a axis, by specifying the start and end points. This will also affect the direction in which the light is pointed!
	void setLengthAndAxis( const vec3 &a, const vec3 &b ) override
	{
		vec3 line = b - a;
		mLength = glm::length( line );
		setPosition( ( a + b ) * 0.5f );
		setAxis( line );
	}

private:
	float mLength;
	vec3  mAxis;

};

} // namespace cinder
