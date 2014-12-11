#pragma once

#include "cinder/gl/Texture.h"
#include "cinder/DataSource.h"
#include "cinder/Exception.h"

struct LightProfileData {
	enum Symmetry { LATERAL, QUADRANT, HEMISPHERE, NONE };

	LightProfileData() : numberOfLamps( 0 ), numberOfVerticalAngles( 0 ), numberOfHorizontalAngles( 0 ) {}

	int   numberOfLamps;
	float lumensPerLamp;
	float candelaMultiplier;
	int   numberOfVerticalAngles;
	int   numberOfHorizontalAngles;
	int   photometricType;
	int   unitsType;
	float width;
	float length;
	float height;
	float ballastFactor;
	float inputWatts;

	int   numberOfCandelaValues;
	float maxCandelaValue;

	Symmetry horizontalSymmetry;

	std::vector<float> verticalAngles;
	std::vector<float> horizontalAngles;
	std::vector<float> candelaValues;
};

typedef std::shared_ptr<class LightProfile> LightProfileRef;

class LightProfile {
public:
	LightProfile() {}
	LightProfile( ci::DataSourceRef src ) { readData( src ); }

	static std::shared_ptr<LightProfile> create() { return std::make_shared<LightProfile>(); }
	static std::shared_ptr<LightProfile> create( ci::DataSourceRef src ) { return std::make_shared<LightProfile>( src ); }

	ci::gl::Texture2dRef createTexture() const;

protected:
	void readData( ci::DataSourceRef src );

	void wrapAngles( float *horAngle, float *vertAngle ) const;

	//! Returns the index of the largest horizontal angle smaller than \a angle.
	size_t   getHorizontalIndex( float angle ) const;
	//! Returns the index of the largest vertical angle smaller than \a angle.
	size_t   getVerticalIndex( float angle ) const;

	//! Returns a single candela value for the specified indices.
	float    getCandela( int horIndex, int vertIndex ) const;
	//! Returns four candela values that can be used for cubic interpolation.
	void     getCandela( int horIndex, int vertIndex, float *c0, float *c1, float *c2, float *c3 ) const;
	float    getClosestCandela( float horAngle, float vertAngle ) const;
	float    getInterpolatedCandela( float horAngle, float vertAngle ) const;

	template<typename T>
	static T interpolate( T p0, T p1, T p2, T p3, float t )
	{
		return p1 + 0.5f * t * ( p2 - p0 + t * ( 2 * p0 - 5 * p1 + 4 * p2 - p3 + t * ( 3 * ( p1 - p2 ) + p3 - p0 ) ) );
	}

	static float interpolate( const ci::vec4 &p, float t ) { return interpolate( p.x, p.y, p.z, p.w, t ); }
	static float mod( float x, float y ) { if( 0.0f == y ) return x; return x - y * ::floorf( x / y ); }
	static float wrap( float x, float min = 0, float max = 1 ) { return mod( x - min, max - min ) + min; }

protected:
	LightProfileData mData;
};

class LightProfileExc : public ci::Exception {
public:
	LightProfileExc( const std::string &description ) : ci::Exception( description ) {}
};

class LightProfileInvalidExc : public LightProfileExc {
public:
	LightProfileInvalidExc() : LightProfileExc( "Invalid data." ) {}
};

class LightProfileUnexpectedEOFExc : public LightProfileExc {
public:
	LightProfileUnexpectedEOFExc() : LightProfileExc( "Unexpected End Of File." ) {}
};