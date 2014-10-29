#pragma once
#include "Poco/Foundation.h"
#include "Poco/HashMap.h"
#include "Poco/File.h"
#include <fstream>
#include <iostream>

using namespace std;

using Poco::HashMap;
using Poco::Int32;


typedef HashMap<string,string> spMap;

class spcodemap
{
public:
	spcodemap(void);
	~spcodemap(void);

	string Spcode2Areacode(string);
	void InitSpcodeMap(string);
	void TraversalSpcodeMap();

private:
	spMap *sm;
};