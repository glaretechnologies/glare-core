/*=====================================================================
BufferedPrintOutput.h
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "PrintOutput.h"
#include <vector>


/*=====================================================================
BufferedPrintOutput
-------------------
Implements PrintOutput interface, just saves messages to a buffer.
=====================================================================*/
class BufferedPrintOutput : public PrintOutput
{
public:
	BufferedPrintOutput();
	~BufferedPrintOutput();

	virtual void print(const std::string& s) { msgs.push_back(s); }
	virtual void printStr(const std::string& s) { msgs.push_back(s); }

	void writeBufferedMessages(PrintOutput& p)
	{
		for(size_t i=0; i<msgs.size(); ++i)
			p.print(msgs[i]);
	}

	void clear() { msgs.clear(); }
	const std::vector<std::string>& getMessages() const { return msgs; }
private:
	std::vector<std::string> msgs;
};
