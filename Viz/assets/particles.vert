#version 120

//varying vec2 Uv;
uniform sampler2D Rand;
uniform vec2 RandSize;
uniform sampler2D ControlPoints;
uniform vec2 ControlPointsSize;
varying float brightness;
uniform int numRandomsPerParticle;
uniform int textureRowSize;
uniform float time;

int randomCount = -1;
float id;
// lookup in random texture, which is organised into columns (see Renderer.cpp)
float randRow;
float randColOffset;
float amount;
int inst0;
int inst1;

const float pi = 3.14159;
const int NUM_INSTRUMENTS = 8;


float sq(float x)
{
	return x*x;
}

float rand()
{
	randomCount = randomCount + 1;
	return texture2D(Rand, vec2(randColOffset + randomCount, randRow)/RandSize).x;
}




//
// Description : Array and textureless GLSL 2D/3D/4D simplex
// noise functions.
// Author : Ian McEwan, Ashima Arts.
// Maintainer : ijm
// Lastmod : 20110822 (ijm)
// License : Copyright (C) 2011 Ashima Arts. All rights reserved.
// Distributed under the MIT License. See LICENSE file.
// https://github.com/ashima/webgl-noise
//

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x*34.0)+1.0)*x);
}

vec4 taylorInvSqrt(vec4 r)
{
	return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v)
{
	const vec2 C = vec2(1.0/6.0, 1.0/3.0) ;
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
	
	// First corner
	vec3 i = floor(v + dot(v, C.yyy) );
	vec3 x0 = v - i + dot(i, C.xxx) ;
	
	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min( g.xyz, l.zxy );
	vec3 i2 = max( g.xyz, l.zxy );
	
	// x0 = x0 - 0.0 + 0.0 * C.xxx;
	// x1 = x0 - i1 + 1.0 * C.xxx;
	// x2 = x0 - i2 + 2.0 * C.xxx;
	// x3 = x0 - 1.0 + 3.0 * C.xxx;
	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
	vec3 x3 = x0 - D.yyy; // -1.0+3.0*C.x = -0.5 = -D.y
	
	// Permutations
	i = mod289(i);
	vec4 p = permute( permute( permute(
									   i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
							  + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
					 + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));
	
	// Gradients: 7x7 points over a square, mapped onto an octahedron.
	// The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
	float n_ = 0.142857142857; // 1.0/7.0
	vec3 ns = n_ * D.wyz - D.xzx;
	
	vec4 j = p - 49.0 * floor(p * ns.z * ns.z); // mod(p,7*7)
	
	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_ ); // mod(j,N)
	
	vec4 x = x_ *ns.x + ns.yyyy;
	vec4 y = y_ *ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);
	
	vec4 b0 = vec4( x.xy, y.xy );
	vec4 b1 = vec4( x.zw, y.zw );
	
	//vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
	//vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
	vec4 s0 = floor(b0)*2.0 + 1.0;
	vec4 s1 = floor(b1)*2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));
	
	vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
	vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;
	
	vec3 p0 = vec3(a0.xy,h.x);
	vec3 p1 = vec3(a0.zw,h.y);
	vec3 p2 = vec3(a1.xy,h.z);
	vec3 p3 = vec3(a1.zw,h.w);
	
	//Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;
	
	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
	m = m * m;
	return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1),
								 dot(p2,x2), dot(p3,x3) ) );
}

// -------------------------------------------------------

vec2 hermiteSpline(vec2 point0, vec2 tangent0, vec2 point1, vec2 tangent1, float s)
{
	float h1 =  2*s*s*s - 3*s*s + 1;          // calculate basis function 1
	float h2 = -2*s*s*s + 3*s*s;              // calculate basis function 2
	float h3 =   s*s*s  - 2*s*s + s;         // calculate basis function 3
	float h4 =   s*s*s  -  s*s;              // calculate basis function 4
	vec2 p = h1*point0 +                    // multiply and sum all funtions
	h2*point1 +                    // together to build the interpolated
	h3*tangent0 +                    // point along the curve.
	h4*tangent1;
//	col = vec4(1,p.x*.2,0,1);
	return p;
}

vec2 hermiteSpline(int inst0, int inst1, float t)
{
	float tex_x = (inst0*NUM_INSTRUMENTS + inst1)/ControlPointsSize.x;
	float numPoints = texture2D(ControlPoints, vec2(tex_x, 0)).x;
	float numSegments = numPoints - 1;
	float segment = min(numSegments-1, int(t*numSegments));
	float p = t*numSegments - segment;
	
	// +1's here are because index 0 is used to store number of
	// values
	vec4 segmentPoint = texture2D(ControlPoints, vec2(tex_x, (segment+1)/ControlPointsSize.y));
	vec4 segmentPointPlus1 = texture2D(ControlPoints, vec2(tex_x, (segment+1+1)/ControlPointsSize.y));
	// last segment is a special case
	// (cached as optimization)
//	vec2 tangent0 = segmentPointPlus1 - segmentPoint;
//	vec2 segmentPointPlus2 = texture2D(ControlPoints, vec2(tex_x, (segment+2+1)/ControlPointsSize.y)).xy;
//	vec2 tangent1 = vec2(0);
//	if (segment+2<numPoints)
//	{
//		tangent1 = segmentPointPlus2 - segmentPointPlus1;
//	}

	// temp
//	vec2 r = segmentPointPlus1;
//	r.x = numPoints / 10. * 2 - 1;
//	return r;
//	return vec2(0, texture2D(ControlPoints, vec2(1, 0)).y);
//	return segmentPoint;
//	return mix(segmentPoint, segmentPointPlus1, p);
//	return hermiteSpline(segmentPoint, tangent0, segmentPointPlus1, tangent1, p);
	
	float s = p;
//	vec2 point0 = segmentPoint;
//	vec2 point1 = segmentPointPlus1;
	
	float h1 =  2*s*s*s - 3*s*s + 1;          // calculate basis function 1
	float h2 = -2*s*s*s + 3*s*s;              // calculate basis function 2
	float h3 =   s*s*s  - 2*s*s + s;         // calculate basis function 3
	float h4 =   s*s*s  -  s*s;              // calculate basis function 4
	vec2 v = h1*segmentPoint.xy                    // multiply and sum all funtions
	+ h2*segmentPointPlus1.xy                    // together to build the interpolated
	+ h3*segmentPoint.zw                    // point along the curve.
		+ h4*segmentPointPlus1.zw;
	//	col = vec4(1,p.x*.2,0,1);
	return v;

	
}



vec4 calculatePositionNoise(float x, float y)
{
	vec2 pos = vec2(x,y);
	float noise = snoise(vec3(pos, time*(.16+.1)*.4)) - 0.5;
	noise *= noise;
	float noise2 = snoise(vec3(pos, time*(.6+.1)))*(sin(time)+0.5);
	float noise3 = snoise(vec3(pos, time*0.007))*2;
	return vec4(cos(2*pi*noise+noise3), sin(2*pi*noise+noise3), 0, 0)*(0.1015+(noise2*0.02-0.05) + 0.03*noise3);
}

float ups_sq(float x)
{
	return 1. - (1.-x)*(1.-x);
}



void main()
{
	id = gl_Vertex.z;
	randRow = mod(id, textureRowSize);
	randColOffset = int(randRow / textureRowSize) * numRandomsPerParticle;

	inst0 = int(gl_Vertex.x);
	inst1 = int(gl_Vertex.y);
	if (inst0 == inst1)// || inst0!=3 || inst1!=4)
	{
		// discard
		gl_Position = vec4(0, 0, -300, 1);
		return;
	}
	float period = rand()*50. + 10;
	float phase = rand()*period;
	float t = mod((time+phase)/period, 1.f);
	t *= min(1., t+0.2);
	gl_Position = gl_ModelViewProjectionMatrix * vec4(hermiteSpline(inst0, inst1, t), 0, 1);
	
	amount = gl_Vertex.w;
//	gl_Position = vec4(gl_Vertex.xy, 0, 1);
//	gl_Position = getPosition();
	gl_Position += calculatePositionNoise(gl_Position.x, gl_Position.y)*0.24;//*rand();
	gl_Position.z = 0;
	//	Uv = gl_MultiTexCoord0.st;
	gl_PointSize = 2.05*(20.12*(1+2*cos(rand())-0.5)*0.7*amount*.75);// + (6 + 6)*amount);
	brightness = ups_sq(amount)*(2*rand()*.21315+0.7543214) ;//sq(rand()*0.63) * (0.3+0.7*amount);
//	gl_Position.xy += vec2(rand()*0.002, rand()*0.002);
	if (rand()<0.04)
	{
		float x = max(0., sin(time*rand()*rand()*0.572)-0.97)/0.03;
		gl_PointSize += 0.5*x*amount*100;
		//gl_PointSize = 0;
	}
}


