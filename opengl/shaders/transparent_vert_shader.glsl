varying vec3 normal;
varying vec3 pos_cs;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	pos_cs = (gl_ModelViewMatrix  * gl_Vertex).xyz;
 
	normal = gl_NormalMatrix * gl_Normal;
}
