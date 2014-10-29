#pragma once
#pragma comment(lib, "ws2_32.lib")

#include "usermap.h"
#include "spcodemap.h"
#include "gjdx_ss7.h"
#include "Poco/Task.h"
#include "Poco/Types.h"
#include "Poco/TaskManager.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Logger.h"
#include "Poco/BinaryWriter.h"
#include <fstream>

using namespace std;
using namespace Poco;
using Poco::Task;
using Poco::TaskManager;
using Poco::NumberFormatter;
using Poco::Logger;
using Poco::BinaryWriter;

extern usermap *um;
extern spcodemap *sm;
extern APP_CONFIG	appCfg;

#define	TIMELABELLENS	12
#define	BUFFER_SIZE		65550

#define	CMD_BIND		0x0001
#define	CMD_BIND_RESP	0x8001
#define	CMD_RECORD		0x0002
#define	CMD_CHECKLINK	0x0003
#define	CMD_CHECKLINK_RESP	0x8003
#define	CMD_UNBIND		0x0004
#define	CMD_UNBIND_RESP	0x8004

#pragma pack(1)
typedef struct	msghead
{
	char	headflag[2];
	UInt16	msglen;
	char	reserve;
	UInt16	cmd;
	UInt16	cmdstatus;
	UInt16	seq;

}MSGHEAD;

typedef struct	bindbody
{
	char	username[16];
	char	password[16];
	UInt16	data_type_count;
	UInt16	data_type1;
//	UInt16	data_type2;

}BINDBODY;

typedef struct xdrhead
{
	UInt8	flag;			// 0x01
	UInt16	type;
	UInt16	ver_info;		// 0xe9bf
	UInt8	format;			// 0x01		text format
	UInt32	data_len;
}XDRHEAD;

typedef struct isupcdr
{
	char	uchStartTime[TIMELABELLENS];    //��ʼʱ��
	char	uchEndTime[TIMELABELLENS];      //����ʱ��
	char	uchcallingnum[13];          //���к���
	char	uchcallednum[13];          //���к���
	char    uchOPC[3];		          //Դ��������
	char	uchDPC[3];		          //Ŀ����������
    UInt32	wReleaseType;		    //�ͷ�ԭ�򣨺���ԭ��
 }ISUPCDR;

typedef	struct	tupcdr
{
	char	uchStartTime[TIMELABELLENS];    //��ʼʱ��
	char	uchEndTime[TIMELABELLENS];      //����ʱ��
	char	uchcallingnum[13];          //���к���
	char	uchcallednum[13];          //���к���
	char    uchOPC[3];		          //Դ��������
	char	uchDPC[3];		          //Ŀ����������
    UInt32	wReleaseType;		    //�ͷ�ԭ�򣨺���ԭ��
}TUPCDR;

typedef	struct	detailcdr
{
	string	caller;          //���к���
	string	called;          //���к���
	string  opc;	          //Դ��������
	string	dpc;	          //Ŀ����������
	UInt32	sorting;		// �Ƿ���Ҫ��Ŀ����У�0 ��	 1 ��

}DETAILCDR;

#pragma pack()

class CdrFilterTask: public Task
{
public:
	CdrFilterTask():Task("CdrFilterTask")
	{
		Poco::Net::SocketAddress gjdx_serveraaddress(appCfg.gjdxserver_ip, appCfg.gjdxserver_port);
		gjdx_socket.connect(gjdx_serveraaddress);

		Poco::Net::SocketAddress ss7_serveraddress(appCfg.ss7server_ip, appCfg.ss7server_port);

		if( appCfg.readFromDataFile == "NULL" )
		{
			readFromDataFile = false;
			ss7_socket.connect(ss7_serveraddress);
			ss7_socket.setReceiveTimeout(5*Timespan::SECONDS);
		}else{
			readFromDataFile = true;
			readFromFile.open(appCfg.readFromDataFile.c_str(), std::ios::binary);
		}

		if( appCfg.saveToDataFile != "NULL" )
		{
			saveToDataFile = true;
			saveToFile.open(appCfg.saveToDataFile.c_str(), std::ios::binary|std::ios::trunc);
		}else
			saveToDataFile = false;
	}

	~CdrFilterTask()
	{
		gjdx_socket.close();

		if( readFromDataFile )
		{
			readFromFile.close();					
		}else
			ss7_socket.close();	

		if( saveToDataFile )
		{
			saveToFile.flush();
			saveToFile.close();
		}
	}

	void runTask();

private:

	Poco::Net::StreamSocket		ss7_socket;
	Poco::Net::DatagramSocket	gjdx_socket;
	std::ofstream saveToFile;
	std::ifstream readFromFile;

	bool	readFromDataFile,saveToDataFile;

	unsigned char	recvbuf[BUFFER_SIZE];

	DETAILCDR	detailcdr;

	void sendCdrToGjdxServer(string);

	void connectToSS7Server(Application& app);
	void disconnectToSS7Server(Application& app);
	void sendCheckLinkToSS7Server(Application& app);
	void recvRespFromSS7Server(Application& app);

	void parseMessage(Application& app);
	void parseCdrRecord(Application& app);
	void parseIsupCdrRecord(Application& app,unsigned char *isup);
	void parseMapCcCdrRecord(Application& app,unsigned char *mapcc);
	void sortingCdr();
};