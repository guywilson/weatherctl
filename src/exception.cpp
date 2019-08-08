#include <stdio.h>

#include "secure_func.h"
#include "exception.h"

void Exception::_initialise()
{
	this->message.clear();
	this->fileName.clear();
	this->className.clear();
	this->methodName.clear();
	this->exception.clear();

	this->errorCode = 0L;
	this->lineNumber = 0L;
}

Exception::Exception()
{
	_initialise();
	this->errorCode = ERR_UNDEFINED;
	this->message = "Undefined exeption";
}

Exception::Exception(const string & message)
{
	_initialise();
	this->message = message;
}

Exception::Exception(dword errorCode, const string & message)
{
	_initialise();
	this->errorCode = errorCode;
	this->message = message;
}

Exception::Exception(dword errorCode, const string & message, const string & fileName, const string & className, const string & methodName, dword lineNumber)
{
	_initialise();
	this->errorCode = errorCode;
	this->message = message;
	this->fileName = fileName;
	this->className = className;
	this->methodName = methodName;
	this->lineNumber = lineNumber;
}

dword Exception::getErrorCode() {
	return this->errorCode;
}

dword Exception::getLineNumber() {
	return this->lineNumber;
}

string & Exception::getFileName() {
	return this->fileName;
}

string & Exception::getClassName() {
	return this->className;
}

string & Exception::getMethodName() {
	return this->methodName;
}

string & Exception::getMessage() {
	return this->message;
}

string & Exception::getExceptionString()
{
	if (errorCode) {
		exception = "*** Exception (" + to_string(errorCode) + ") : ";
	}
	else {
		exception = "*** Exception : ";
	}

	exception += message + " ***";

	if (fileName.length() > 0 && className.length() > 0 && methodName.length() > 0) {
		exception += "\nIn " + fileName + " - " + className + "::" + methodName;
	}

	if (lineNumber > 0) {
		exception += " at line " + to_string(lineNumber);
	}

	return exception;
}
