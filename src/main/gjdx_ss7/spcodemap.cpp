#include "spcodemap.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Types.h"

using Poco::Util::Application;
using Poco::StringTokenizer;
using Poco::NumberFormatter;
using namespace std;

spcodemap::spcodemap(void)
{
	sm = new spMap;
}


spcodemap::~spcodemap(void)
{
	delete sm;
}

string spcodemap::Spcode2Areacode(string sp)
{
	spMap::Iterator it = sm->find(sp);
	if(it == sm->end()){
		return "";
	}
	return it->second;
}

void spcodemap::InitSpcodeMap(string filename)
{
	std::fstream	fs;
	char	buffer[500];
	string	strline;
	int	i;

	fs.open(filename.c_str(),ios::in);

	if (fs.bad() || fs.fail()) 
		throw Poco::ApplicationException("Input File error in Open!");

	while( fs.getline (buffer,500) ){

		strline = buffer;
		i = strline.find("#");
		strline = strline.substr(0,i);
		strline = Poco::trim(strline);

		if( strline.length() > 0 )
		{
			StringTokenizer splitspcode( strline, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

			Poco::StringTokenizer::Iterator firstfield = splitspcode.begin();
			Poco::StringTokenizer::Iterator secondfield = firstfield++;
			sm->insert(spMap::ValueType(*secondfield,*firstfield));

//			Application::instance().logger().information(*firstfield + ":" + *secondfield );
		}

	}
	fs.close();

}

void spcodemap::TraversalSpcodeMap()
{
	spMap::Iterator it = sm->begin();

	while( it != sm->end() )
	{
		Application::instance().logger().information(it->first + ":" + it->second );
		it++;
	}
	
	Application::instance().logger().information("total: " + NumberFormatter::format(sm->size()) + " spcode records ");
}
