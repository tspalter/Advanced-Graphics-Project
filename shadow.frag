/////////////////////////////////////////////////////////////////////////
// Pixel shader for shading
////////////////////////////////////////////////////////////////////////
#version 330

in vec4 position;

void main()
{
	// Basic shadow map
    //gl_FragData[0] = position;

    // Debug Shadow Map
    //gl_FragData[0] = vec4(position.w / 100);

    // Shadow Blur
    float wVal = position.w/150;
    gl_FragColor.x = wVal;
    gl_FragColor.y = pow(wVal, 2);
    gl_FragColor.z = pow(wVal, 3);
    gl_FragColor.w = pow(wVal, 4);

}