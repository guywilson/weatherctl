#include <string>
#include "types.h"

#ifndef _INCL_EXCEPTION
#define _INCL_EXCEPTION

#define	ERR_MALLOC					0x00000001
#define ERR_INDEX_OUT_OF_RANGE		0x00000002
#define ERR_ARRAY_OVERFLOW			0x00000004
#define ERR_UNMATCHED_PARENTHESIS	0x00000008
#define ERR_INVALID_NUM_ARGUMENTS	0x00000010
#define ERR_RPN_PARSE				0x00000020
#define ERR_INVALID_TOKEN			0x00000040

#define ERR_UNDEFINED				0x0000FFFF

using namespace std;

class Exception
{
	private:
		dword		errorCode;
		string		message;

		string		fileName;
		string		className;
		string		methodName;

		string 		exception;

		dword		lineNumber;

		void		_initialise();

	public:
					Exception();
					Exception(const string & message);
					Exception(dword errorCode, const string & message);
					Exception(
							dword errorCode,
							const string & message,
							const string & fileName,
							const string & className,
							const string & methodName,
							dword lineNumber);

		dword		getErrorCode();
		dword		getLineNumber();

		string &	getFileName();
		string &	getClassName();
		string &	getMethodName();

		string &	getMessage();

		string &	getExceptionString();
};

#endif
