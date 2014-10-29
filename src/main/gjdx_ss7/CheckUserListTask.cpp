#include "CheckUserListTask.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Timestamp.h"
#include "Poco/Types.h"

using Poco::Util::Application;
using Poco::StringTokenizer;
using Poco::NumberFormatter;
using Poco::Timestamp;
using Poco::UInt32;
using Poco::UInt64;
using namespace std;

extern	APP_CONFIG	appCfg;
extern	UInt32	sendCheckLink;

extern	UInt64	totalRecord;
extern	UInt64	totalCdr;
extern	UInt64	totalIsupCdr;
extern	UInt64	totalMapCCCdr;

CheckUserListTask::CheckUserListTask():Task("CheckUserListTask")
{	
}

	
void CheckUserListTask::runTask()
{
	Application& app = Application::instance();

	Timestamp	lastSendCheckLinkTime;
	Timestamp	lastCheckFileTime(0);
	Timestamp	now;
	Timestamp::TimeDiff interVal;
	string str;

	try
	{
		sm->InitSpcodeMap("spcode.ini");
		app.logger().information("SpcodeMap Init:");
//		sm->TraversalSpcodeMap();

	}catch( Poco::ApplicationException& exc )
	{
		app.logger().information( exc.displayText() );
		app.logger().information( "spcode.ini not exist!!! " );
		Poco::Util::ServerApplication::terminate();
	}


	do{
		now.update();
		interVal = now - lastCheckFileTime;

		if( (appCfg.checkUserlistInterval * 1000 * 1000 ) < interVal || interVal < 100 ){
			if( isFileChanged("userlist.ini") ){

				app.logger().information("userlist.ini is changed!!!");
				
				FileToString(&str,"userlist.ini");
				um->InitUserMap(str);
				app.logger().information("UserMap Refresh:");
//				um->TraversalUserMap();				
			}else
				app.logger().debug("userlist.ini is not change!!!");

			lastCheckFileTime.update();
		}

		//  检查是否应该发出心跳包
		if( sendCheckLink != 1)
		{
			interVal = now - lastSendCheckLinkTime;

			if( (appCfg.checkLinkInterval * 1000 * 1000 ) < interVal )
			{
				sendCheckLink = 1;
				lastSendCheckLinkTime.update();

				// 输出目前的统计数据
				app.logger().information("totalRecord:" + NumberFormatter::format(totalRecord) + " totalCdr:" + NumberFormatter::format(totalCdr) + " totalIsupCdr:" + NumberFormatter::format(totalIsupCdr) + " totalMapCdr:" + NumberFormatter::format(totalMapCCCdr) );

			}
		}

	}while ( !sleep(2000) );

	Poco::Util::ServerApplication::terminate();

}

bool CheckUserListTask::isFileChanged(string filename)
{
	
	bool bChanged = false;
	static time_t lastChangeTime=0;

	File  userList(filename);
	if( userList.exists()){
		time_t newChangeTime=userList.getLastModified().epochTime();
		if(  newChangeTime != lastChangeTime )
		{
			lastChangeTime = newChangeTime;
			bChanged = true;
		}
	}else{
		Application::instance().logger().information(filename + " is not exist!!!");
	}
	return bChanged;
}

bool CheckUserListTask::FileToString(string *str,string filename)
{
	std::fstream	fs;
	char	buffer[200];

	str->clear();
	fs.open(filename.c_str(),ios::in);

	if (fs.bad() || fs.fail()) 
		throw Poco::ApplicationException("Input File error in Open!");

	while( fs.getline (buffer,200) ){
			str->append(buffer);
			str->append(",");
	}
	return true;
}

