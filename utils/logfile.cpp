//#include "stdafx.h"


#include "logfile.h"


#include <assert.h>
#include <stdarg.h>

Logfile::Logfile(const char* filename)
{
	f = fopen(filename, "w");

	if(!f)
		assert(0);

}

Logfile::Logfile(const std::string& filename)
{
	f = fopen(filename.c_str(), "w");

	if(!f)
		assert(0);

}

Logfile::~Logfile()
{
	fclose(f);
	f = NULL;
}

void Logfile::write(const char* text, ...)
{
	static char textBuffer[1024];
	va_list args;
	va_start(args, text);
	vsprintf(textBuffer, text, args);
	va_end(args);

	fprintf(f, textBuffer);
}

void Logfile::write(const std::string& text)
{
	assert(f);

	fprintf(f, text.c_str());
}

void Logfile::flushWrites()
{


	fflush(f);


}