#ifndef __JS_BALLSHAPE_H__
#define __JS_BALLSHAPE_H__

namespace js
{

/*============================================================================
BallShape
-----

============================================================================*/
class BallShape
{
public:
	BallShape(){}
	virtual ~BallShape(){}

	virtual bool lineStabsShape(float& t_out, Vec3& normal_out, const Vec3& raystart_ws, const Vec3& rayend_ws);

};







}//end namespace js
#endif //__JS_BALLSHAPE_H__