/*=====================================================================
MTwister.h
----------
File created by ClassTemplate on Sun Jul 02 03:29:07 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MTWISTER_H_666_
#define __MTWISTER_H_666_





/*=====================================================================
MTwister
--------

=====================================================================*/
class MTwister
{
public:
	/*=====================================================================
	MTwister
	--------
	
	=====================================================================*/
	MTwister(unsigned long seed);

	~MTwister();


	float unitRandom();
	unsigned long genrand_int32();

private:
	void init_genrand(unsigned long s);


	static const unsigned int N = 624;
	static const unsigned int M = 397;

	unsigned long mt[N]; /* the array for the state vector  */
	int mti; /* mti==N+1 means mt[N] is not initialized */
};





#endif //__MTWISTER_H_666_




