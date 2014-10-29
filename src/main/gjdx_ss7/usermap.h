#pragma once
#include "Poco/Foundation.h"
#include "Poco/HashMap.h"
#include "Poco/Mutex.h"

using namespace std;
using Poco::HashMap;
using Poco::Int32;

typedef HashMap<string,Int32> phonenumMap;

class usermap
{
public:
	usermap(void);
	~usermap(void);

	bool VerifyUser(string);
	void InitUserMap(string);
	void TraversalUserMap();

private:
	Poco::Mutex m_m;
	phonenumMap *pm;
};


