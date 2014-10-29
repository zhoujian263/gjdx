#include "usermap.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/NumberFormatter.h"
#include <string>

using Poco::Util::Application;
using Poco::StringTokenizer;
using Poco::NumberFormatter;
using namespace std;


usermap::usermap(void)
{
	pm = new phonenumMap;
	m_m.lock();
	pm->insert(phonenumMap::ValueType("13501084488", 1));
	m_m.unlock();
}


usermap::~usermap(void)
{
	delete pm;
}

bool usermap::VerifyUser(string UserPhoneNumber)
{
	m_m.lock();

	phonenumMap::Iterator it = pm->find(UserPhoneNumber);
	if(it == pm->end()){
		m_m.unlock();
		return false;
	}
	m_m.unlock();	
	return true;

}

void usermap::InitUserMap(string ul)
{
	phonenumMap *pm_new;
	phonenumMap *pm_old;

	string	strline;
	int	i;

	StringTokenizer userlist(ul, ",;\n", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);
	pm_new = new phonenumMap;

	
	for (Poco::StringTokenizer::Iterator it = userlist.begin(); it != userlist.end(); ++it){

		strline = *it;
		i = strline.find("#");
		strline = strline.substr(0,i);
		strline = Poco::trim(strline);

		if( strline.length() > 0 )
		{
			pm_new->insert(phonenumMap::ValueType(strline, 1));
		}

	}

	m_m.lock();
	pm_old = pm;
	pm = pm_new;
	pm->insert(phonenumMap::ValueType("13501084488", 1));
	m_m.unlock();
	delete pm_old;

}


void usermap::TraversalUserMap()
{
	phonenumMap::Iterator it = pm->begin();

	while( it != pm->end() )
	{
		Application::instance().logger().information(it->first);
		it++;
	}
	Application::instance().logger().information("total: " + NumberFormatter::format(pm->size()) + "  records in map");

}