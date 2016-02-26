varying vec3 normal;
varying vec3 pos_cs;
varying vec4 texture_coords;


void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	pos_cs = (gl_ModelViewMatrix  * gl_Vertex).xyz;
 
	normal = gl_NormalMatrix * gl_Normal;

	texture_coords = gl_MultiTexCoord0;
}
