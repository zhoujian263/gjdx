#include "gjdx_ss7.h"
#include "usermap.h"
#include "spcodemap.h"
#include "CheckUserListTask.h"
#include "CdrFilterTask.h"
#include "Poco/Logger.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/FileChannel.h" 
#include "Poco/SplitterChannel.h" 
#include "Poco/FormattingChannel.h" 
#include "Poco/PatternFormatter.h"
#include "Poco/AutoPtr.h"

using	Poco::Logger;
using	Poco::ConsoleChannel;
using	Poco::FileChannel;
using	Poco::SplitterChannel;
using	Poco::FormattingChannel;
using	Poco::PatternFormatter;
using	Poco::AutoPtr;

//全局变量
usermap *um=new usermap();	//存贮用户HASHMAP
spcodemap *sm=new spcodemap();	//存贮spcode

APP_CONFIG	appCfg;
UInt16		msgSeq = 10;
UInt32		sendCheckLink = 0;

UInt64		totalRecord = 0;
UInt64		totalCdr = 0;
UInt64		totalIsupCdr = 0;
UInt64		totalMapCCCdr = 0;

void SS7FilterServer::initialize(Application& self)
{
	loadConfiguration(); // load default configuration files, if present
	ServerApplication::initialize(self);
	Poco::Util::AbstractConfiguration & config(Poco::Util::Application::instance().config());

	appCfg.ss7server_ip=config.getString("ss7server.ip");
	appCfg.ss7server_port=config.getString("ss7server.port");
	appCfg.ss7server_username=config.getString("ss7server.username");
	appCfg.ss7server_password=config.getString("ss7server.password");
	
	appCfg.gjdxserver_ip=config.getString("gjdxserver.ip");
	appCfg.gjdxserver_port=config.getString("gjdxserver.port");

	appCfg.checkLinkInterval=config.getInt("appconfig.checklinkinterval",30);
	appCfg.checkUserlistInterval=config.getInt("appconfig.checkuserlistinterval",30);

	appCfg.saveToDataFile=config.getString("appconfig.savetodatafile","NULL");
	appCfg.readFromDataFile=config.getString("appconfig.readfromdatafile","NULL");

	AutoPtr<ConsoleChannel> pCons(new ConsoleChannel); 
	AutoPtr<FileChannel> pFile(new FileChannel);
	
	pFile->setProperty("rotation", "20 M"); 
	pFile->setProperty("path","gjdx_ss7.log"); 
	pFile->setProperty("archive", "timestamp");
	pFile->setProperty("purgeAge","3 months");

	AutoPtr<SplitterChannel> pSplitter(new SplitterChannel); 
	pSplitter->addChannel(pCons); 
	pSplitter->addChannel(pFile);

	AutoPtr<PatternFormatter> pPF(new PatternFormatter);
	
	pPF->setProperty("times", "local");
	pPF->setProperty("pattern", "%Y-%m-%d %H:%M:%S: %t");
	AutoPtr<FormattingChannel> pFC(new FormattingChannel(pPF,pSplitter));
	
	Application::instance().logger().setChannel(pFC);
	Application::instance().logger().setLevel(config.getString("appconfig.loglevel","information"));
	
	logger().information("starting up");
	logger().information("gjdxserver: " + appCfg.gjdxserver_ip + " " + appCfg.gjdxserver_port );
	logger().information("ss7server: "+appCfg.ss7server_ip + " " + appCfg.ss7server_port );
	logger().critical("ss7server: " + appCfg.ss7server_username + " " +appCfg.ss7server_password);
		
}
		
void SS7FilterServer::uninitialize()
{
	logger().information("shutting down");
	ServerApplication::uninitialize();
}

void SS7FilterServer::defineOptions(OptionSet& options)
{
	ServerApplication::defineOptions(options);
		
	options.addOption(
		Option("help", "h", "display help information on command line arguments")
			.required(false)
			.repeatable(false)
			.callback(OptionCallback<SS7FilterServer>(this, &SS7FilterServer::handleHelp)));
}

void SS7FilterServer::handleHelp(const std::string& name, const std::string& value)
{
	_helpRequested = true;
	displayHelp();
	stopOptionsProcessing();
}

void SS7FilterServer::displayHelp()
{
	HelpFormatter helpFormatter(options());
	helpFormatter.setCommand(commandName());
	helpFormatter.setUsage("OPTIONS");
	helpFormatter.setHeader("A sample server application that demonstrates some of the features of the Util::ServerApplication class.");
	helpFormatter.format(std::cout);
}

int SS7FilterServer::main(const std::vector<std::string>& args)
{
	if (!_helpRequested)
	{
		TaskManager tm;
		tm.start(new CheckUserListTask);
		Thread::sleep(5000);
		tm.start(new CdrFilterTask);
		waitForTerminationRequest();
		tm.cancelAll();
		tm.joinAll();
	}

	return Application::EXIT_OK;

}

POCO_SERVER_MAIN(SS7FilterServer);


