/*=====================================================================
RunningAverage.h
----------------
File created by ClassTemplate on Sun Feb 19 21:23:18 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __RUNNINGAVERAGE_H_666_
#define __RUNNINGAVERAGE_H_666_



#include <vector>

/*=====================================================================
RunningAverage
--------------

=====================================================================*/
class RunningAverage
{
public:
	/*=====================================================================
	RunningAverage
	--------------
	
	=====================================================================*/
	RunningAverage(int history_len);

	~RunningAverage();

	double getAv() const;
	
	void addValue(double val);	

private:
	std::vector<double> values;
	int history_len;

};



#endif //__RUNNINGAVERAGE_H_666_




