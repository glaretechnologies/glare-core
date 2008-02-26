/*=====================================================================

=====================================================================*/
#ifndef __BoxFilterFunction_H_666_
#define __BoxFilterFunction_H_666_

#include "FilterFunction.h"


/*=====================================================================

=====================================================================*/
class BoxFilterFunction : public FilterFunction
{
public:
	BoxFilterFunction();

	virtual ~BoxFilterFunction();

	virtual double supportRadius() const;

	virtual double eval(double abs_x) const;

	virtual const std::string description() const;

private:
};



#endif //__BoxFilterFunction_H_666_




