#pragma once
#include "usermap.h"
#include "spcodemap.h"
#include "gjdx_ss7.h"
#include "Poco/Task.h"
#include "Poco/TaskManager.h"
#include "Poco/File.h"
#include <fstream>
#include <iostream>

using namespace std;
using Poco::Task;
using Poco::TaskManager;
using Poco::File;

extern usermap *um;
extern spcodemap *sm;

class CheckUserListTask: public Task
{
public:
	CheckUserListTask();
	void runTask();

private:	

	bool isFileChanged(string);
	bool FileToString(string *,string );

};