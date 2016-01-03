#pragma once

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <stdint.h>

namespace TestUtils {

class TestFailure : public std::exception {
public:
	TestFailure(const char* inExpression, const char* inFunction, const int32_t inLine)
	{
		std::stringstream sstream;
		sstream << "\"" << inExpression << "\" - " << inFunction << ":" << inLine;
		infoString = sstream.str();
	}
	const std::string& getInfo() const {
		return infoString;
	}
private:
	std::string infoString;
};

class ScopedFunctionTrace {
public:
	ScopedFunctionTrace(const char* inFunctionName)
		: functionName(inFunctionName)
	{
		std::cout << "-- " << functionName << " begin" << std::endl;
	}
	~ScopedFunctionTrace() {
		std::cout << "   " << functionName << " end" << std::endl;
	}
private:
	std::string functionName;
};

} // namespace TestUtils

#define TEST_ENTER ScopedFunctionTrace(__FUNCTION__);
#define TEST_VERIFY(x) if ((x) == false) {throw TestFailure(#x, __FUNCTION__, __LINE__); }
