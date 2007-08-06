/*=====================================================================
nffio.h
-------
File created by ClassTemplate on Fri Nov 19 08:04:42 2004
Code By Nicholas Chapman.

Feel free to use for any commercial or other purpose
=====================================================================*/
#ifndef __NFFIO_H_666_
#define __NFFIO_H_666_


#include <string>
#include <vector>


class NFFioExcep
{
public:
	NFFioExcep(const std::string& s_) : s(s_) {}
	~NFFioExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};

/*=====================================================================
NFFio
-----

=====================================================================*/
class NFFio
{
public:
	/*=====================================================================
	NFFio
	-----
	
	=====================================================================*/

	
	//throws NFFioExcep
	static void writeNFF(const std::string& pathname, const std::vector<float>& imgdata,
							int width, int height);

	//throws NFFioExcep
	static void readNNF(const std::string& pathname, std::vector<float>& imgdata_out, 
							int& width_out, int& height_out);


private:
	NFFio();

};



#endif //__NFFIO_H_666_




