// Lens Blur
// 
// University of Pennsylvania CIS565 final project
// copyright (c) 2013 Cheng-Tso Lin  
#version  430
layout (pixel_center_integer) in vec4 gl_FragCoord;

uniform sampler2D u_nearestDepthTex;
uniform int u_peelingPass;
uniform int u_ScreenWidth;
uniform int u_ScreenHeight;

void main()
{
    if( u_peelingPass > 0 )
	{
	    vec2 texcoord = vec2( gl_FragCoord.x/float(u_ScreenWidth), gl_FragCoord.y/float(u_ScreenHeight) );
	    float depth = texture( u_nearestDepthTex, texcoord ).r;
		if( depth >= gl_FragCoord.z )
		    discard;
		//gl_FragDepth = depth;
    }
} 