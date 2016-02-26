varying vec4 texture_coords;


void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	texture_coords = gl_MultiTexCoord0;
}
