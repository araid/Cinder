#version 150

struct LightData
{
	vec3  position;
	float intensity;
	vec3  direction;
	float range;
	vec4  length;
	vec4  color;
	vec2  attenuation;
	vec2  angle;
	mat4  shadowMatrix;
	mat4  modulationMatrix;
	int   shadowIndex;
	int   modulationIndex;
	int   flags;
	int   reserved;
};

layout (std140) uniform uLights
{
	LightData uLight[32];
};

uniform int             uLightCount;
uniform sampler2DShadow uShadowMap[8];
uniform sampler2D       uModulationMap[8];
uniform vec4            uSkyDirection;

in vec4 vertPosition;
in vec3 vertNormal;

out vec4 fragColor;

float saturate( in float v )
{
	return clamp( v, 0.0, 1.0 );
}

float calcShadowPCF4x4( in sampler2DShadow map, in vec4 sc )
{
	const int r = 2;
	const int s = 2 * r;

	sc.z -= 0.001; // Apply shadow bias.
	
	float shadow = 0.0;
	shadow += textureProjOffset( map, sc, ivec2(-s,-s) );
	shadow += textureProjOffset( map, sc, ivec2(-r,-s) );
	shadow += textureProjOffset( map, sc, ivec2( r,-s) );
	shadow += textureProjOffset( map, sc, ivec2( s,-s) );
	
	shadow += textureProjOffset( map, sc, ivec2(-s,-r) );
	shadow += textureProjOffset( map, sc, ivec2(-r,-r) );
	shadow += textureProjOffset( map, sc, ivec2( r,-r) );
	shadow += textureProjOffset( map, sc, ivec2( s,-r) );
	
	shadow += textureProjOffset( map, sc, ivec2(-s, r) );
	shadow += textureProjOffset( map, sc, ivec2(-r, r) );
	shadow += textureProjOffset( map, sc, ivec2( r, r) );
	shadow += textureProjOffset( map, sc, ivec2( s, r) );
	
	shadow += textureProjOffset( map, sc, ivec2(-s, s) );
	shadow += textureProjOffset( map, sc, ivec2(-r, s) );
	shadow += textureProjOffset( map, sc, ivec2( r, s) );
	shadow += textureProjOffset( map, sc, ivec2( s, s) );
		
	return shadow * (1.0 / 16.0);
}

float calcSpotAngularAttenuation( in vec3 L, in vec3 D, in vec2 cutoffs )
{
	return smoothstep( cutoffs.x, cutoffs.y, dot( -D, L ) );
}

float calcSpotDistanceAttenuation( in float distance, in vec2 coeffs, in float range )
{
	return step( distance, range ) / ( 1.0 + distance * coeffs.x + distance * distance * coeffs.y );
}

vec3 calcRepresentativePoint( in vec3 p0, in vec3 p1, in vec3 v, in vec3 r )
{
	// See: http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf (page 17)
	vec3 l0 = p0 - v;
	vec3 l1 = p1 - v;
	vec3 ld = l1 - l0;
	float a = dot( r, ld );
	float t = ( dot( r, l0 ) * a - dot( l0, ld ) ) / ( dot( ld, ld ) - a * a );

	return p0 + saturate( t ) * (p1 - p0);
}

void main(void)
{
	const vec3  materialDiffuseColor = vec3( 1 );
	const vec3  materialSpecularColor = vec3( 1 );
	const float materialShininess = 250.0;

	// Initialize ambient, diffuse and specular colors.
	vec3 ambient = vec3( 0 );
	vec3 diffuse = vec3( 0 );
	vec3 specular = vec3( 0 );

	// Calculate normal and eye vector.
	vec3 N = normalize( vertNormal );
	vec3 E = normalize( -vertPosition.xyz );

	// Hemispherical ambient lighting.
	float hemi = 0.5 + 0.5 * dot( uSkyDirection.xyz, N );
	ambient = mix( vec3( 0 ), vec3( 0.05 ), hemi ) * materialDiffuseColor;

	// Calculate lighting.
	for( int i=0; i<uLightCount; ++i )
	{
		// Fetch light type.
		int type = uLight[i].flags & 0xF;

		// Calculate modulation.
		vec3 modulation = vec3( 1 );
		if( ( uLight[i].flags & 0x10 ) > 0 )
		{
			vec4 modulationCoord = uLight[i].modulationMatrix * vertPosition;
			modulation = textureProj( uModulationMap[ uLight[i].modulationIndex ], modulationCoord ).rgb;
		}

		// Calculate shadow.
		float shadow = 1.0;
		if( ( uLight[i].flags & 0x20 ) > 0 )
		{
			vec4 shadowCoord = uLight[i].shadowMatrix * vertPosition;
			shadow = calcShadowPCF4x4( uShadowMap[ uLight[i].shadowIndex ], shadowCoord );
		}

		// Calculate end-points of the light for convenience.
		vec3 p0 = uLight[i].position.xyz;
		vec3 p1 = uLight[i].position.xyz + uLight[i].length.w * uLight[i].length.xyz;

		// Calculate direction and distance to light.
		vec3 lightPosition = p0 + clamp( dot( uLight[i].length.xyz, vertPosition.xyz - p0 ), 0.0, uLight[i].length.w ) * uLight[i].length.xyz;
		vec3 L = mix( -uLight[i].direction, lightPosition - vertPosition.xyz, ( type & 0xF ) > 0 ); // Use light direction for directional lights.
		float distance = length( L );
		L /= distance;

		// Calculate attenuation.
		float angularAttenuation = mix( 1.0, calcSpotAngularAttenuation( L, uLight[i].direction, uLight[i].angle ), ( type & 0xC ) > 0 ); // Spot & wedge light only.
		float distanceAttenuation = mix( 1.0, calcSpotDistanceAttenuation( distance, uLight[i].attenuation, uLight[i].range ), ( type & 0xF ) > 0 ); // Point, capsule, spot & wedge light only.
		vec3  colorAttenuation = modulation * uLight[i].color.rgb * uLight[i].intensity;

		// Calculate diffuse color.
		float lambert = max( 0.0, dot( N, L ) );
		diffuse += shadow * colorAttenuation * distanceAttenuation * angularAttenuation * lambert * materialDiffuseColor;

		// Calculate representative light vector (for linear lights only).
		if( ( type & 0xA ) > 0 ) {
			lightPosition = calcRepresentativePoint( p0, p1, vertPosition.xyz, -reflect( E, N ) );
			L = lightPosition - vertPosition.xyz;
			distance = length( L );
			distanceAttenuation = calcSpotDistanceAttenuation( distance, uLight[i].attenuation, 1.0e14 );
			L /= distance;
		}

#define USE_BLINN_PHONG 0
#if USE_BLINN_PHONG
		const float normalization = ( materialShininess + 8.0 ) / ( 3.14159265 * 8.0 );		
		
		// Calculate specular color.
		vec3 H = normalize( L + E );
		float reflection = normalization * pow( max( 0.0, dot( N, H ) ), materialShininess );
		specular += colorAttenuation * distanceAttenuation * angularAttenuation * reflection * materialSpecularColor;
#else
		const float normalization = ( materialShininess + 2.0 ) / ( 3.14159265 * 2.0 );

		// Calculate specular color.
		vec3 R = normalize( -reflect( L, N ) );
		float reflection = normalization * pow( max( 0.0, dot( R, E ) ), materialShininess );
		specular += colorAttenuation * distanceAttenuation * angularAttenuation * reflection * materialSpecularColor;
#endif
	}

	// Output gamma-corrected color.
	fragColor.rgb = sqrt( ambient + diffuse + specular );
	fragColor.a = 1.0;
}