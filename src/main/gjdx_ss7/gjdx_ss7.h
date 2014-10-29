#ifndef Gjdx_SS7_Application_INCLUDED
#define Gjdx_SS7_Application_INCLUDED

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Timestamp.h"
#include "Poco/Task.h"
#include "Poco/TaskManager.h"
#include "Poco/File.h"
#include "Poco/Types.h"
#include <iostream>
#include <fstream>

using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::OptionCallback;
using Poco::Util::HelpFormatter;
using Poco::Task;
using Poco::TaskManager;
using Poco::DateTimeFormatter;
using Poco::File;
using Poco::Timestamp;
using Poco::UInt32;

using namespace std;

typedef struct appconfig{

	string	ss7server_ip;
	string	ss7server_port;
	string	ss7server_username;
	string	ss7server_password;

	string	gjdxserver_ip;
	string	gjdxserver_port;

	UInt32	checkLinkInterval;
	UInt32	checkUserlistInterval;

	string	saveToDataFile;
	string	readFromDataFile;

}APP_CONFIG;

class SS7FilterServer: public ServerApplication
{
public:
	SS7FilterServer(): _helpRequested(false)
	{
	}
	
	~SS7FilterServer()
	{
	}

protected:
	void initialize(Application& self);
	void uninitialize();
	void defineOptions(OptionSet& options);
	void handleHelp(const std::string& name, const std::string& value);
	void displayHelp();
	int main(const std::vector<std::string>& args);

private:
	bool _helpRequested;

};

#endif