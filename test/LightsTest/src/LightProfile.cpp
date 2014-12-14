
#include "LightProfile.h"

#include "cinder/Utilities.h"

using namespace ci;

void LightProfile::readData( DataSourceRef src )
{
	// Open file stream.
	IStreamRef stream = src->createStream();

	// Check file header.
	std::string header = stream->readLine();
	if( header != "IESNA:LM-63-2002" )
		throw LightProfileInvalidExc();

	// Find first data.
	std::string line;
	while( !stream->isEof() && line.compare( 0, 5, "TILT=" ) ) {
		line = stream->readLine();
	};

	// If TILT=INCLUDE, skip the next four lines.
	if( !line.compare( 5, line.length() - 5, "INCLUDE" ) ) {
		for( size_t i = 0; i < 4; ++i )
			stream->readLine();
	}

	// Parse the file.
	LightProfileData data;
	std::vector<std::string> tokens;

	if( stream->isEof() )
		throw LightProfileUnexpectedEOFExc();

	data.verticalAngles.clear();
	data.horizontalAngles.clear();
	data.candelaValues.clear();

	line = stream->readLine();
	tokens = ci::split( line, ", ", true );

	assert( tokens.size() == 10 );
	data.numberOfLamps = ci::fromString<int>( tokens[0] );
	data.lumensPerLamp = ci::fromString<float>( tokens[1] );
	data.candelaMultiplier = ci::fromString<float>( tokens[2] );
	data.numberOfVerticalAngles = ci::fromString<int>( tokens[3] );
	data.numberOfHorizontalAngles = ci::fromString<int>( tokens[4] );
	data.photometricType = ci::fromString<int>( tokens[5] );
	data.unitsType = ci::fromString<int>( tokens[6] );
	data.width = ci::fromString<float>( tokens[7] );
	data.length = ci::fromString<float>( tokens[8] );
	data.height = ci::fromString<float>( tokens[9] );

	if( stream->isEof() )
		throw LightProfileUnexpectedEOFExc();

	line = stream->readLine();
	tokens = ci::split( line, ", ", true );

	assert( tokens.size() == 3 );
	data.ballastFactor = ci::fromString<float>( tokens[0] );
	data.inputWatts = ci::fromString<float>( tokens[2] );

	while( data.verticalAngles.size() < data.numberOfVerticalAngles ) {
		if( stream->isEof() )
			throw LightProfileUnexpectedEOFExc();

		line = stream->readLine();
		tokens = ci::split( line, ", ", true );

		for( size_t i = 0; i < tokens.size(); ++i )
			data.verticalAngles.push_back( ci::fromString<float>( tokens[i] ) );
	}

	while( data.horizontalAngles.size() < data.numberOfHorizontalAngles ) {
		if( stream->isEof() )
			throw LightProfileUnexpectedEOFExc();

		line = stream->readLine();
		tokens = ci::split( line, ", ", true );

		for( size_t i = 0; i < tokens.size(); ++i )
			data.horizontalAngles.push_back( ci::fromString<float>( tokens[i] ) );
	}

	data.maxCandelaValue = 0.0f;

	data.numberOfCandelaValues = data.numberOfHorizontalAngles * data.numberOfVerticalAngles;
	while( data.candelaValues.size() < data.numberOfCandelaValues ) {
		if( stream->isEof() )
			throw LightProfileUnexpectedEOFExc();

		line = stream->readLine();
		tokens = ci::split( line, ", ", true );

		for( size_t i = 0; i < tokens.size(); ++i ) {
			data.candelaValues.push_back( ci::fromString<float>( tokens[i] ) );
			data.maxCandelaValue = math<float>::max( data.maxCandelaValue, data.candelaValues.back() );
		}
	}

	if( data.horizontalAngles.back() == 0.0f )
		data.horizontalSymmetry = LightProfileData::LATERAL;
	else if( data.horizontalAngles.back() == 90.0f )
		data.horizontalSymmetry = LightProfileData::QUADRANT;
	else if( data.horizontalAngles.back() == 180.0f )
		data.horizontalSymmetry = LightProfileData::HEMISPHERE;
	else if( data.horizontalAngles.back() == 360.0f )
		data.horizontalSymmetry = LightProfileData::NONE;
	else
		throw LightProfileInvalidExc();

	mData = data;
}

void LightProfile::wrapAngles( float *horAngle, float *vertAngle ) const
{
	switch( mData.horizontalSymmetry ) {
	case LightProfileData::LATERAL:
		break;
	case LightProfileData::QUADRANT:
		*horAngle = wrap( *horAngle, 0.0f, 180.0f );
		if( *horAngle >= 90.0f )
			*horAngle = 180.0f - *horAngle;
		break;
	case LightProfileData::HEMISPHERE:
		*horAngle = wrap( *horAngle, 0.0f, 360.0f );
		if( *horAngle >= 180.0f )
			*horAngle = 360.0f - *horAngle;
		break;
	default:
		*horAngle = wrap( *horAngle, 0.0f, 360.0f );
		break;
	}
}

size_t LightProfile::getHorizontalIndex( float angle ) const
{
	for( int i = mData.numberOfHorizontalAngles - 1; i >= 0; --i ) {
		if( mData.horizontalAngles[i] <= angle )
			return i;
	}

	return 0;
}

size_t LightProfile::getVerticalIndex( float angle ) const
{
	for( int i = mData.numberOfVerticalAngles - 1; i >= 0; --i ) {
		if( mData.verticalAngles[i] <= angle )
			return i;
	}

	return 0;
}

float LightProfile::getCandela( int horIndex, int vertIndex ) const
{
	horIndex = math<int>::clamp( horIndex, 0, mData.numberOfHorizontalAngles - 1 );
	vertIndex = math<int>::clamp( vertIndex, 0, mData.numberOfVerticalAngles - 1 );

	int index = horIndex * mData.numberOfVerticalAngles + vertIndex;
	if( index < mData.numberOfCandelaValues )
		return mData.candelaValues[index];

	return 0.0f;
}

void LightProfile::getCandela( int horIndex, int vertIndex, float *c0, float *c1, float *c2, float *c3 ) const
{
	horIndex = math<int>::clamp( horIndex, 0, mData.numberOfHorizontalAngles - 1 );
	vertIndex = math<int>::clamp( vertIndex, 0, mData.numberOfVerticalAngles - 1 );

	*c1 = getCandela( horIndex, vertIndex );
	*c2 = getCandela( horIndex, vertIndex + 1 );
	if( vertIndex == 0 )
		*c0 = *c1 * 2.0f - *c2;
	else
		*c0 = getCandela( horIndex, vertIndex - 1 );
	if( vertIndex >= mData.numberOfVerticalAngles - 2 )
		*c3 = *c2 * 2.0f - *c1;
	else
		*c3 = getCandela( horIndex, vertIndex + 2 );
}

float LightProfile::getClosestCandela( float horAngle, float vertAngle ) const
{
	wrapAngles( &horAngle, &vertAngle );

	int hi = getHorizontalIndex( horAngle );
	int vi = getVerticalIndex( vertAngle );
	return getCandela( hi, vi );
}

float LightProfile::getInterpolatedCandela( float horAngle, float vertAngle ) const
{
	float p0, p1, p2, p3;

	// Convert angles to symmetry.
	wrapAngles( &horAngle, &vertAngle );

	// Get index.
	int hi = getHorizontalIndex( horAngle );
	int vi = getVerticalIndex( vertAngle );

	// Get interpolation factor.
	int hn = math<int>::clamp( hi + 1, 0, mData.numberOfHorizontalAngles - 1 );
	int vn = math<int>::clamp( vi + 1, 0, mData.numberOfVerticalAngles - 1 );
	float ht = ( hn == hi ) ? 0.0f : ( horAngle - mData.horizontalAngles[hi] ) / ( mData.horizontalAngles[hn] - mData.horizontalAngles[hi] );
	float vt = ( vn == vi ) ? 0.0f : ( vertAngle - mData.verticalAngles[vi] ) / ( mData.verticalAngles[vn] - mData.verticalAngles[vi] );

	// Interpolate.
	getCandela( hi - 1, vi, &p0, &p1, &p2, &p3 );
	float c0 = interpolate( p0, p1, p2, p3, vt );
	getCandela( hi + 0, vi, &p0, &p1, &p2, &p3 );
	float c1 = interpolate( p0, p1, p2, p3, vt );
	getCandela( hi + 1, vi, &p0, &p1, &p2, &p3 );
	float c2 = interpolate( p0, p1, p2, p3, vt );
	getCandela( hi + 2, vi, &p0, &p1, &p2, &p3 );
	float c3 = interpolate( p0, p1, p2, p3, vt );

	return interpolate( c0, c1, c2, c3, ht );
}

gl::Texture2dRef LightProfile::createTexture() const
{
	static const int kSize = 256;

	if( !mData.numberOfLamps )
		return gl::Texture2dRef();

	// Interpolate.
	Channel32f data( kSize, kSize );
	float *ptr = data.getData();
	for( size_t j = 0; j < kSize; ++j ) {
		float vertAngle = glm::degrees( glm::acos( 2.0f * j / kSize - 1.0f ) );
		for( size_t i = 0; i < kSize; ++i ) {
			float horAngle = i * 360.0f / kSize;
			float candela = getInterpolatedCandela( horAngle, vertAngle );
			*ptr++ = (float) ( glm::clamp( candela / mData.maxCandelaValue, 0.0f, 1.0f ) * 1 );
		}
	}

	return gl::Texture2d::create( data );
}