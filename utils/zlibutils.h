/*=====================================================================
zlibutils.h
-----------
File created by ClassTemplate on Fri Oct 08 02:21:10 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __ZLIBUTILS_H_666_
#define __ZLIBUTILS_H_666_


#include <vector>
#include <string>


class ZLibUtilsExcep
{
public:
	ZLibUtilsExcep(const std::string& message_) : message(message_) {}
	~ZLibUtilsExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};



/*=====================================================================
ZLibUtils
---------

=====================================================================*/
class ZLibUtils
{
public:
	/*=====================================================================
	ZLibUtils
	---------
	
	=====================================================================*/
	//ZLibUtils();

	//~ZLibUtils();


	//throws ZLibUtilsExcep
	static void compress(const void* data, const int datalen, 
					std::vector<char>& compressed_data_out);

	//throws ZLibUtilsExcep
	static void compress(const std::vector<char>& data, 
					std::vector<char>& compressed_data_out);


	//throws ZLibUtilsExcep
	static void decompress(const void* compressed_data, 
							const int compressed_len, 
							const int decompressed_len,
							std::vector<char>& decompressed_data_out);

	static unsigned int calcChecksum(const void* data, const int datalen);

private:
	ZLibUtils();

};



#endif //__ZLIBUTILS_H_666_




