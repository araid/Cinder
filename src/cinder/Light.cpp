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

//#include "cinder/app/AppBasic.h"
//#include "cinder/gl/gl.h"
#include "cinder/Light.h"
#include "cinder/Log.h"

namespace cinder {

const mat4 SpotLight::sBiasMatrix = mat4( 0.5f, 0.0f, 0.0f, 0.0f,
										  0.0f, 0.5f, 0.0f, 0.0f,
										  0.0f, 0.0f, 0.5f, 0.0f,
										  0.5f, 0.5f, 0.5f, 1.0f );

Light::Data Light::getData( double time, const mat4 &transform ) const
{
	static ColorA luminance = ColorA( 0.2125f, 0.7154f, 0.0721f, 0.0f );

	Light::Data params;
	memset( &params, 0, sizeof( Light::Data ) );

	params.color = vec4( mColor[0], mColor[1], mColor[2], mColor.dot( luminance ) );
	params.intensity = mIntensity;
	params.flags = mFlags | ( mType & 0xF );

	return params;
}

void Light::setColor( const Color &color )
{
	// Keep values for red, green and blue within range [0, 1].
	float m = math<float>::max( color.r, math<float>::max( color.g, color.b ) );

	if( m > 0 )
		m = 1.0f / m;
	else
		m = 0.0f;

	mColor = m * color;
}

void Light::setColorTemperature( float kelvin )
{
	vec3 a, b, c;

	if( kelvin <= 6500.0f ) {
		a = vec3( 0.0, -2902.1955373783176, -8257.7997278925690 );
		b = vec3( 0.0, 1669.5803561666639, 2575.2827530017594 );
		c = vec3( 1.0, 1.3302673723350029, 1.8993753891711275 );
	}
	else {
		a = vec3( 1745.0425298314172, 1216.6168361476490, -8257.7997278925690 );
		b = vec3( -2666.3474220535695, -2173.1012343082230, 2575.2827530017594 );
		c = vec3( 0.55995389139931482, 0.70381203140554553, 1.8993753891711275 );
	}

	vec3 rgb = a / ( kelvin + b ) + c;
	rgb.r = glm::clamp( rgb.r, 0.0f, 1.0f );
	rgb.g = glm::clamp( rgb.g, 0.0f, 1.0f );
	rgb.b = glm::clamp( rgb.b, 0.0f, 1.0f );

	mColor = Color( rgb.r, rgb.g, rgb.b );
}

Light::Data DirectionalLight::getData( double time, const mat4 &transform ) const
{
	// Populate the LightData structure.
	Light::Data params = Light::getData( time, transform );
	params.direction = glm::normalize( mat3( transform ) * mDirection );

	return params;
}

Light::Data PointLight::getData( double time, const mat4 &transform ) const
{
	// Populate the LightData structure.
	Light::Data params = Light::getData( time, transform );
	params.position = vec3( transform * vec4( mPosition, 1 ) );
	params.range = mRange;
	params.attenuation = getAttenuation();

	if( mFlags & ( Data::ShadowEnabled | Data::ModulationEnabled ) ) {
		mat4 invTransform = glm::inverse( transform );

		if( mFlags & Data::ShadowEnabled ) {
			//params.shadowMatrix = getShadowMatrix() * invTransform;
			params.shadowIndex = mShadowIndex;
		}

		if( mFlags & Data::ModulationEnabled ) {
			params.modulationMatrix = getModulationMatrix( time ) * invTransform;
			params.modulationIndex = mModulationIndex;
		}
	}

	return params;
}

void PointLight::updateMatrices() const
{
	if( mIsDirty ) {
		mViewMatrix[POSITIVE_X] = glm::lookAt( mPosition, vec3( 1, 0, 0 ), vec3( 0, -1, 0 ) );
		mViewMatrix[NEGATIVE_X] = glm::lookAt( mPosition, vec3( -1, 0, 0 ), vec3( 0, -1, 0 ) );
		mViewMatrix[POSITIVE_Y] = glm::lookAt( mPosition, vec3( 0, 1, 0 ), vec3( 0, 0, -1 ) );
		mViewMatrix[NEGATIVE_Y] = glm::lookAt( mPosition, vec3( 0, -1, 0 ), vec3( 0, 0, 1 ) );
		mViewMatrix[POSITIVE_Z] = glm::lookAt( mPosition, vec3( 0, 0, 1 ), vec3( 0, -1, 0 ) );
		mViewMatrix[NEGATIVE_Z] = glm::lookAt( mPosition, vec3( 0, 0, -1 ), vec3( 0, -1, 0 ) );
		mProjectionMatrix = glm::perspective( glm::radians( 90.0f ), 1.0f, 0.1f, mRange );

		mIsDirty = false;
	}
}

mat4 PointLight::getModulationMatrix( double time ) const
{
	// Determine the up vector based on the current direction.
	//float dot = glm::abs( glm::dot( mDirection, vec3( 0, 1, 0 ) ) );
	//vec3  up = ( dot < 0.99f ) ? vec3( 0, 1, 0 ) : vec3( 0, 0, 1 );

	//return glm::lookAt( mPosition, mPointAt, up );

	return glm::toMat4( glm::quat( mDirection, vec3( 0, -1, 0 ) ) );
}

Light::Data CapsuleLight::getData( double time, const mat4 &transform ) const
{
	// Adjust position.
	vec3 position = mPosition - 0.5f * mLength * mAxis;

	// Populate the LightData structure.
	Light::Data params = PointLight::getData( time, transform );
	params.position = vec3( transform * vec4( position, 1 ) );
	params.horizontal = glm::normalize( mat3( transform ) * mAxis );
	params.width = mLength;

	return params;
}

Light::Data SpotLight::getData( double time, const mat4 &transform ) const
{
	// Populate the LightData structure.
	Light::Data params = Light::getData( time, transform );
	params.position = vec3( transform * vec4( mPosition, 1 ) );
	params.direction = glm::normalize( mat3( transform ) * mDirection );
	params.range = mRange;
	params.attenuation = getAttenuation();
	params.angle = getConeParams();

	if( mFlags & ( Data::ShadowEnabled | Data::ModulationEnabled ) ) {
		mat4 invTransform = glm::inverse( transform );

		if( mFlags & Data::ShadowEnabled ) {
			params.shadowMatrix = getShadowMatrix() * invTransform;
			params.shadowIndex = mShadowIndex;
		}

		if( mFlags & Data::ModulationEnabled ) {
			params.modulationMatrix = getModulationMatrix( time ) * invTransform;
			params.modulationIndex = mModulationIndex;
		}
	}

	return params;
}

vec2 SpotLight::getConeParams() const
{
	float cosSpot = math<float>::cos( math<float>::atan( mSpotRatio ) );
	float cosHotspot = math<float>::cos( math<float>::atan( math<float>::min( mHotspotRatio, mSpotRatio ) ) );
	return vec2( cosSpot, cosHotspot );
}

void SpotLight::updateMatrices() const
{
	if( mIsDirty ) {
		// Determine the up vector based on the current direction.
		float dot = glm::abs( glm::dot( mDirection, vec3( 0, 1, 0 ) ) );
		vec3  up = ( dot < 0.99f ) ? vec3( 0, 1, 0 ) : vec3( 0, 0, 1 );

		mViewMatrix = glm::lookAt( mPosition, mPointAt, up );
		mProjectionMatrix = glm::perspective( 2.0f * math<float>::atan( mSpotRatio ), 1.0f, 0.1f, mRange );
		mShadowMatrix = sBiasMatrix * mProjectionMatrix * mViewMatrix;

		mIsDirty = false;
	}
}

mat4 SpotLight::getModulationMatrix( double time ) const
{
	updateMatrices();

	mat4 modulation = mModulationParams.toMat4( float( time ) );
	return modulation * mProjectionMatrix * mViewMatrix;
}

Light::Data WedgeLight::getData( double time, const mat4 &transform ) const
{
	// Adjust position.
	vec3 position = mPosition - 0.5f * mLength * mAxis;

	// Populate the LightData structure.
	Light::Data params = Light::getData( time, transform );
	params.position = vec3( transform * vec4( position, 1 ) );
	params.direction = glm::normalize( mat3( transform ) * mDirection );
	params.horizontal = glm::normalize( mat3( transform ) * mAxis );
	params.width = mLength;
	params.range = mRange;
	params.attenuation = getAttenuation();
	params.angle = getConeParams();

	// Disable shadows and modulation.
	params.flags &= ~Data::ShadowEnabled;
	params.flags &= ~Data::ModulationEnabled;

	return params;
}

bool Light::calcRange( float intensity, const vec2 &attenuation, float *range, float threshold )
{
	float L = math<float>::max( 0, attenuation.x );
	float Q = math<float>::max( 0, attenuation.y );
	float T = math<float>::clamp( threshold, 0.001f, 1.0f );

	if( Q > 0 ) {
		// Use quadratic attenuation.
		*range = ( math<float>::sqrt( T * ( T * ( L * L ) + 4 * intensity * Q ) ) - L * T ) / ( 2 * Q * T );
		return true;
	}
	else if( L > 0 ) {
		// Use linear attenuation.
		*range = intensity / ( L * T );
		return true;
	}
	else {
		return false;
	}
}

bool Light::calcIntensity( float range, const vec2 &attenuation, float *intensity, float threshold )
{
	float L = math<float>::max( 0, attenuation.x );
	float Q = math<float>::max( 0, attenuation.y );
	float T = math<float>::clamp( threshold, 0.001f, 1.0f );

	if( Q > 0 || L > 0 ) {
		// Use quadratic and/or linear attenuation.
		*intensity = T * range * ( range * Q + L );
		return true;
	}
	else {
		return false;
	}
}

} // namespace cinder