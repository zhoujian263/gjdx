#include "CdrFilterTask.h"
#include <cstring>
#include <assert.h>
#include "Poco/Util/ServerApplication.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetException.h"
#include "Poco/StringTokenizer.h"
#include "Poco/NumberParser.h" 

using namespace std;
using Poco::Util::Application;
using Poco::Net::NetException;
using Poco::StringTokenizer;
using Poco::NumberParser;

extern	UInt16	msgSeq;
extern	UInt32	sendCheckLink;

extern	UInt64	totalRecord;
extern	UInt64	totalCdr;
extern	UInt64	totalIsupCdr;
extern	UInt64	totalMapCCCdr;
extern	UInt64	totalSipCdr;
extern	UInt64	totalTupCdr;
extern	UInt64	totalUnknowCdr;

void CdrFilterTask::runTask()
{
	Application& app = Application::instance();

	connectToSS7Server(app);

	while( !isCancelled() )
	{
		try
		{
			recvRespFromSS7Server(app);

		}catch ( NetException& exc)
		{
			app.logger().information( exc.displayText() );
			break;

		}catch( TimeoutException& exc)
        {
			app.logger().information( exc.displayText() );
			app.logger().debug("socket timeout,try again!!!");

		}

		if (  sendCheckLink == 1 ){
			sendCheckLinkToSS7Server(app);
			sendCheckLink = 0;
		}

	}

	// ������յ�ͳ������
	app.logger().information("totalRecord:" + NumberFormatter::format(totalRecord) + " totalCdr:" + NumberFormatter::format(totalCdr) + " IsupCdr:" + NumberFormatter::format(totalIsupCdr) + " SipCdr:" + NumberFormatter::format(totalSipCdr) + " TupCdr:" + NumberFormatter::format(totalTupCdr)  + " MapCdr:" + NumberFormatter::format(totalMapCCCdr) + " UnknowCdr:" + NumberFormatter::format(totalUnknowCdr) );

	Poco::Util::ServerApplication::terminate();
}

void CdrFilterTask::sendCdrToGjdxServer(string msg)
{
	gjdx_socket.sendBytes(msg.c_str(), msg.length());
}

void CdrFilterTask::connectToSS7Server(Application& app)
{
	if( readFromDataFile ) return;

	unsigned char sendbuf[sizeof(MSGHEAD)+sizeof(BINDBODY)];

	MSGHEAD		*mh = (MSGHEAD *)sendbuf;
	BINDBODY	*bind = (BINDBODY *)(sendbuf + sizeof(MSGHEAD));

	memset(mh,0,sizeof(MSGHEAD));
	memset(bind,0,sizeof(BINDBODY));

	sendbuf[0]=0x9e;
	sendbuf[1]=0x62;

	mh->msglen =  sizeof(MSGHEAD)+sizeof(BINDBODY)-5 ;
	mh->cmd = CMD_BIND;

	mh->cmdstatus = 0x0;
	mh->seq = msgSeq;

	msgSeq++;
	memcpy(bind->username,appCfg.ss7server_username.c_str(),appCfg.ss7server_username.length());
	memcpy(bind->password,appCfg.ss7server_password.c_str(),appCfg.ss7server_password.length());
	
	bind->data_type_count = 1;
	bind->data_type1=htons(0x0601);
//	bind->data_type2=htons(0x0501);

	ss7_socket.sendBytes(sendbuf,sizeof(MSGHEAD)+sizeof(BINDBODY),0);
	app.logger().information("bind to ss7server,wait response....");
}

void CdrFilterTask::sendCheckLinkToSS7Server(Application& app)
{

	unsigned char sendbuf[sizeof(MSGHEAD)];
	MSGHEAD		*mh = (MSGHEAD *)sendbuf;

	if( readFromDataFile ) return;

	memset(mh,0,sizeof(MSGHEAD));

	sendbuf[0]=0x9e;
	sendbuf[1]=0x62;

	mh->msglen = sizeof(MSGHEAD)-5;
	mh->cmd = CMD_CHECKLINK;
	mh->cmdstatus=0x0;
	mh->seq = msgSeq;

	msgSeq++;

	ss7_socket.sendBytes(sendbuf,sizeof(MSGHEAD),0);
	app.logger().information("send CMD_CHECKLINK message to ss7server, sequence number is " + NumberFormatter::format(msgSeq) + ", wait for response...");

}

void CdrFilterTask::recvRespFromSS7Server(Application& app)
{
	UInt16	msglen = 0;
	Int32	leftlen = 0;
	Int32	recvlen;

	memset(recvbuf,0,sizeof(recvbuf));

	if( saveToDataFile )
	{
		recvlen = ss7_socket.receiveBytes(recvbuf,BUFFER_SIZE,0);
		
		if( recvlen > 0 )
		{
			app.logger().information("recv a message,parse start!!!");
			saveToFile.write((char *)recvbuf,recvlen);
		}else
		{
			throw Poco::Net::NetException(" connection closed from the peer.");
		}
		return;
	}

	if( readFromDataFile )
	{
		if( readFromFile.read((char *)recvbuf,5) == 0)
			throw Poco::Net::NetException("read ss7data file eof!!!");

		if( (recvbuf[0]==0x9e) && (recvbuf[1]==0x62) )
		{
			//app.logger().information("recv a message,parse start!!!");
			totalRecord++;
			msglen = *((u_short *)(recvbuf+2));

		}else
			throw Poco::ApplicationException("ss7server protocal error:lost message head flag");

		if( msglen > 0 )
		{
			if( readFromFile.read((char *)(recvbuf + 5),msglen) == 0)
				throw Poco::Net::NetException("read ss7data file eof!!!");
		}

		parseMessage(app);

		return;
	}


	if(ss7_socket.receiveBytes(recvbuf,5,0)==5)
	{
		app.logger().debug("recv a message,parse start!!!");
		if( (recvbuf[0]==0x9e) && (recvbuf[1]==0x62) )
		{
			totalRecord++;
			msglen = *((u_short *)(recvbuf+2));
			leftlen = msglen;

		}else
			throw Poco::ApplicationException("ss7server protocal error:lost message head flag");
	}

	while( leftlen > 0 )
	{

			recvlen = ss7_socket.receiveBytes((recvbuf + 5 + msglen - leftlen),leftlen,0);
			if( recvlen > 0 )	leftlen -= recvlen;
	}

	parseMessage(app);
	
}

void CdrFilterTask::parseMessage(Application& app)
{
	MSGHEAD *mh;
	string	err_reason;

	mh = (MSGHEAD *)recvbuf;
	
	switch(mh->cmd)
	{
		case CMD_RECORD:
			//app.logger().information("recv Record message: sequence number is " + NumberFormatter::format(mh->seq) );
			parseCdrRecord(app);
			break;
		case CMD_CHECKLINK_RESP:
			app.logger().information("recv Check_Link_Resp message: sequence number is " + NumberFormatter::format(mh->seq) );
			break;
		case CMD_BIND_RESP:
			app.logger().information("recv Bind_Resp message: sequence number is " + NumberFormatter::format(mh->seq) );
			break;
		case CMD_UNBIND_RESP:
			app.logger().information("recv UnBind_Resp message: sequence number is " + NumberFormatter::format(mh->seq) );
			break;
	   default:
		   break;
	}
	
	switch(mh->cmdstatus)
	{
		case 1:
			err_reason = "message length error";
			break;
		case 2:
			err_reason = "unknow command";
			break;
		case 3:
			err_reason = "message sequence number duplicate";
			break;
		case 4:
			err_reason = "invalid username";
			break;
		case 5:
			err_reason = "invalid password";
			break;
		case 99:
			err_reason = "unknow error";
			break;
	   default:
			err_reason = "resver error";
			break;
	}

	if( mh->cmdstatus == 0 )
	{
		app.logger().debug("recv Message success!!!");
	}else{
		app.logger().information("recv message occur error: " + err_reason);
		throw Poco::ApplicationException("ss7server protocal error:lost message head flag");
	}
}

void CdrFilterTask::parseCdrRecord(Application& app)
{
	unsigned char	*ptr;
	unsigned char	*end;

	MSGHEAD *recordhead = ( MSGHEAD *)recvbuf;
	XDRHEAD *xdrhead = (XDRHEAD *)(recvbuf + sizeof(MSGHEAD) );

	end = recvbuf + 5 + recordhead ->msglen;
	ptr = recvbuf + sizeof(MSGHEAD);

	do
	{
		xdrhead = (XDRHEAD *)ptr;

		if ( xdrhead->flag != 0x01 )
		{
			app.logger().information("xdrhead flag not 0x01, this record not xdrrecord,ingore it!!!");
			ptr = ptr + xdrhead->data_len + sizeof(XDRHEAD);
			continue;
		}
		if ( (xdrhead->type != 0x426e) && (xdrhead->type != 0x426a) )
		{
			app.logger().information("xdrhead type error, this record is unknow,ingore it!!!");
			ptr = ptr + xdrhead->data_len + sizeof(XDRHEAD);
			continue;
		}
		if ( xdrhead->format != 0x01 )
		{
			app.logger().information("xdrhead format not 0x01, this record format is unknow,ingore it!!!");
			ptr = ptr + xdrhead->data_len + sizeof(XDRHEAD);
			continue;
		}

		totalCdr++;

		ptr = ptr + sizeof(XDRHEAD);

		parseV6txtCdrRecord(app,ptr,xdrhead->data_len);

		ptr = ptr + xdrhead->data_len;

	}while( ptr != end );

	return;

}

void CdrFilterTask::parseIsupCdrRecord(Application& app,unsigned char *isup)
{
	unsigned	char	*ptr;
	UInt32		cdrlen,i;

	string		startTime,endTime,wReleaseType;
	UInt32		reason;

	cdrlen = *(UInt32 *)isup;


	ptr = isup + 4 ;
	*(ptr + cdrlen -1 ) = 0x0;

	string cdr((char *)ptr);
	StringTokenizer splitcdr(cdr, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

	Poco::StringTokenizer::Iterator it = splitcdr.begin();

	//  iam:20140114 10:16:49:014,rlc:20140114 10:23:20:834,calling:13382712411,called:13546510865,opc:16585312,dpc:327442,releasetype:11

	startTime = *it;
	i=startTime.find(":");
	startTime = startTime.substr(i+1);

	it++;
	endTime = *it;
	i=endTime.find(":");
	endTime = endTime.substr(i+1);

	it++;
	wReleaseType = *it;
	detailcdr.caller = *it;
	i=detailcdr.caller.find(":");
	detailcdr.caller = detailcdr.caller.substr(i+1);

	it++;
	detailcdr.called = *it;
	i=detailcdr.called.find(":");
	detailcdr.called = detailcdr.called.substr(i+1);

	it++;
	detailcdr.opc = *it;
	i = detailcdr.opc.find(":");
	detailcdr.opc = detailcdr.opc.substr(i+1);

	it++;
	detailcdr.dpc = *it;
	i = detailcdr.dpc.find(":");
	detailcdr.dpc = detailcdr.dpc.substr(i+1);
	
	sortingCdr();
	// ��������к��붼�����û��б��У�ֱ�ӷ��أ����������ж�
	if(detailcdr.sorting == 0 ) return;

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	reason = Poco::NumberParser::parse( wReleaseType );			// if failed,throw SyntaxException 

/*	ISUP �ͷ�ԭ�����
	13��117---124�����������ߺ���		�ͷ�ԭ��ΪNORMAL
	14---20			�����û�δӦ��			�ͷ�ԭ��ΪNOANSWER
	21��131			�����û������ܽ�		�ͷ�ԭ��ΪREJECT
	
	0---5			�������ڲ�����			�˲��ֺ��в���һ�����ƽ̨����
	22				���л���				�˲��ֺ��в���һ�����ƽ̨����
	27				Ŀ�ĵز��ɴ�			�˲��ֺ��в���һ�����ƽ̨����
	28				��Ч�����ʽ			�˲��ֺ��в���һ�����ƽ̨����

	�������		�ݶ���������ԭ�����	�ͷ�ԭ��ΪOTHER

	���嶨������˹�˾�ġ����Źһ����Žӿڷ���������V5.x�汾����

*/
	if( reason == 13 )		// �����ͷ�
		wReleaseType = "Normal";
	else if( reason >= 117 && reason <= 124 )	// Ӧ��Ʒ�ָʾ�����ӼƷ�ָʾ��������������
		wReleaseType = "Normal";
	else if( reason >= 14 && reason <= 20 )	// �û�æ���û�δӦ���û�δ��Ӧ���û�ȱϯ
		wReleaseType = "NoAnswer"; 
	else if( reason ==21 || reason== 131 )	// �û��ܽ�
		wReleaseType = "Reject";
	else if( reason < 5 || reason == 22 || reason == 27 || reason == 28 )	// �������
		return;
	else
		wReleaseType = "Other";


	app.logger().information( detailcdr.caller + "," + detailcdr.called + "," +  detailcdr.opc + "," + detailcdr.dpc +  "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" );
//	app.logger().information( caller + "," + called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" );
	sendCdrToGjdxServer( detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" ); 
	
	return;
}

void CdrFilterTask::parseMapCcCdrRecord(Application& app,unsigned char *mapcc)
{
	unsigned	char	*ptr;
	UInt32		cdrlen,i;

	string		startTime,endTime,wReleaseType;
	UInt32		optype,issucess,result;

	cdrlen = *(UInt32 *)mapcc;
	
	ptr = mapcc + 4 ;
	*(ptr + cdrlen -1 ) = 0x0;

	string cdr((char *)ptr);
	StringTokenizer splitcdr(cdr, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

	Poco::StringTokenizer::Iterator it = splitcdr.begin();

	//  btime:20140114 10:23:16:632,type:15,kind:24,opc:16585286,ossn:8,ogt:460030944750100,dpc:16585240,dssn:6,dgt:8613383537123,etime:20140114 10:23:16:635,mdn:8613383537123,uchcallingnum:13313530888,issuccess:0,result:255

	startTime = *it;
	i=startTime.find(":");
	startTime = startTime.substr(i+1);

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	optype = Poco::NumberParser::parse( wReleaseType );			// if failed,throw SyntaxException 

	it += 2;
	detailcdr.opc = *it;
	i = detailcdr.opc.find(":");
	detailcdr.opc = detailcdr.opc.substr(i+1);

	it += 3;
	detailcdr.dpc = *it;
	i = detailcdr.dpc.find(":");
	detailcdr.dpc = detailcdr.dpc.substr(i+1);

	it += 3;
	endTime = *it;
	i=endTime.find(":");
	endTime = endTime.substr(i+1);

	it++;
	detailcdr.called = *it;
	i = detailcdr.called.find(":");
	detailcdr.called = detailcdr.called.substr(i+1);

	it++;
	detailcdr.caller = *it;
	i=detailcdr.caller.find(":");
	detailcdr.caller = detailcdr.caller.substr(i+1);

	sortingCdr();
	// ��������к��붼�����û��б��У�ֱ�ӷ��أ����������ж�
	if(detailcdr.sorting == 0 ) return;

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	issucess = Poco::NumberParser::parse( wReleaseType );			// ȡ��issucessֵ��if failed,throw SyntaxException 

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	result = Poco::NumberParser::parse( wReleaseType );			// ȡ��resultֵ��if failed,throw SyntaxException 

/*	MAP �����ͷ�ԭ�����
	optype 15   λ�����룬�ֻ���MSC����
	optype 16	·�����룬MSC��HLR����

	�����1 = 1	��������1 = δ����ĵ绰����	Ӣ������1 = Unassigned directory number
	�����2 = 2	��������2 = �ƶ�̨ȥ��	Ӣ������2 = Inactive
	�����3 = 3	��������3 = �ƶ�̨æ	Ӣ������3 = Busy
	�����4 = 4	��������4 = �����������սᵽ�ƶ�̨	Ӣ������4 = Termination Denied
	�����5 = 5	��������5 = ��Ѱ����Ӧ	Ӣ������5 = No Page Response
	�����6 = 6	��������6 = ������	Ӣ������6 = Unavailable

	���嶨������˹�˾�ġ����Źһ����Žӿڷ���������V5.x�汾����
*/
	if( optype != 15 )	return;

	if( issucess == 0 && result == 255 )	// ��������
		wReleaseType = "Normal";
	else if( issucess == 3 && result == 3 )	// MAP�����ƶ�̨æ
		wReleaseType = "NoAnswer"; 
	else
		wReleaseType = "Other";
	
	app.logger().information( detailcdr.caller + "," + detailcdr.called + "," +  detailcdr.opc + "," + detailcdr.dpc +  "," + startTime + "," + endTime + "," + NumberFormatter::format(issucess) + "," + NumberFormatter::format(result)+ "," + NumberFormatter::format(optype)+ ",MAP" );	
//	app.logger().information( caller + "," + called + "," + startTime + "," + endTime + "," + NumberFormatter::format(issucess) + "," + NumberFormatter::format(result)+ "," + NumberFormatter::format(optype)+ ",MAP" );	
	sendCdrToGjdxServer( detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(result) + ",MAP" ); 
	
	return;

}

void CdrFilterTask::sortingCdr(){

	detailcdr.sorting = 0;

	if( detailcdr.called.compare(0,2,"86") == 0 ) detailcdr.called = detailcdr.called.substr(2);
	if( detailcdr.called.compare(0,3,"086") == 0 ) detailcdr.called = detailcdr.called.substr(3);
	if( detailcdr.called.compare(0,4,"0086") == 0 ) detailcdr.called = detailcdr.called.substr(4);

	if( detailcdr.caller.compare(0,2,"86") == 0 ) detailcdr.caller = detailcdr.caller.substr(2);
	if( detailcdr.caller.compare(0,3,"086") == 0 ) detailcdr.caller = detailcdr.caller.substr(3);
	if( detailcdr.caller.compare(0,4,"0086") == 0 ) detailcdr.caller = detailcdr.caller.substr(4);

	// ���������ͨ����ҵ�񣬻��ڱ��к���ǰ�� 70096 �� 70090 ǰ׺
	if(detailcdr.called.compare(0,3,"700") == 0 ) detailcdr.called = detailcdr.called.substr(5);

	// ����������������룬��û�м����ţ�����opc������������
	if(detailcdr.caller.length()<=8)
	{
		detailcdr.caller=sm->Spcode2Areacode(detailcdr.opc) + detailcdr.caller;
	}
	// ����������������룬��û�м����ţ�����dpc������������
	if(detailcdr.called.length()<=8)
	{
		detailcdr.called=sm->Spcode2Areacode(detailcdr.dpc) + detailcdr.called;
	}

	if( um->VerifyUser(detailcdr.caller) || um->VerifyUser(detailcdr.called) )
	{
		detailcdr.sorting = 1;	
	}

	//if( detailcdr.called.compare(detailcdr.called.size()-11,11,"18103524234") == 0 )
	//{
	//	detailcdr.sorting = 1;
	//}

	return;

}


void CdrFilterTask::parseV6txtCdrRecord(Application& app,unsigned char *rec,UInt32 cdrlen)
{
	unsigned	char	*ptr;
	UInt32		i;

	string		str,startTime,endTime,eventcause,eventid,wReleaseType;
	UInt32		protocolid,eventresult;

	ptr = rec ;
	*(ptr + cdrlen -1 ) = 0x0;

	string cdr((char *)ptr);
	StringTokenizer splitcdr(cdr, ",", StringTokenizer::TOK_TRIM | StringTokenizer::TOK_IGNORE_EMPTY);

	Poco::StringTokenizer::Iterator it = splitcdr.begin();

	str = *it;
	i=str.find(":");
	str = str.substr(i+1);

	protocolid = Poco::NumberParser::parse( str );

	switch( protocolid )
	{
		case 117:		// SIP CALL
			totalSipCdr++;	
			break;

		case 115:		// ISUP CALL---- eventresult���� 1����������(�û���ͨ)��2������ǰ���йһ���3����������йһ���4������󣬱��оܾ���5������æ(����)��6:����
			totalIsupCdr++;			
			break;

		case 119:		// MAP CALL
			totalMapCCCdr++;
			break;

		case 114:		// TUP CALL
			totalTupCdr++;
			break;

		default:
			//app.logger().information("protocolid "+ *it +" is unknow. ");
			totalUnknowCdr++;
			return;
			break;
	}

	if(protocolid== 119){
		// ����17006�ṹ
		// wProtocolId:119,ucBTime:2016-11-16 10:20:35.565,linkid:0,ucSpcKind:24,unOPC:16585286,ucOSSN:8,ucOGT:460030944750100,unDPC:16585218,ucDSSN:6,ucDGT:460036061000000,ucETime:2016-11-16 10:20:35.775,ucCallingIMSI:68305070710f000,ucCallingNum:18103508288,ucCallingIMEI:,ucCalledNum:18030156085,ucCalledIMSI:040851053100000,ucAddtionNum:,wEventId:15,wEventCause:65535,dwEventResult:0
		it++;
		startTime = *it;
		i=startTime.find(":");
		startTime = startTime.substr(i+1);
		
		it += 3;
		detailcdr.opc = *it;
		i = detailcdr.opc.find(":");
		detailcdr.opc = detailcdr.opc.substr(i+1);

		it += 3;
		detailcdr.dpc = *it;
		i = detailcdr.dpc.find(":");
		detailcdr.dpc = detailcdr.dpc.substr(i+1);

		it += 3;
		endTime = *it;
		i=endTime.find(":");
		endTime = endTime.substr(i+1);

		it += 2;
		detailcdr.caller = *it;
		i=detailcdr.caller.find(":");
		detailcdr.caller = detailcdr.caller.substr(i+1);

		it += 2;
		detailcdr.called = *it;
		i = detailcdr.called.find(":");
		detailcdr.called = detailcdr.called.substr(i+1);

		it++;
		str = *it;
		i = str.find(":");
		eventid =str.substr(i+1);
		if( eventid.compare(0,2,"16") != 0 )	return;   // ���Ǻ��м�¼��ֱ�ӷ���

		sortingCdr();
		// ��������к��붼�����û��б��У�ֱ�ӷ��أ����������ж�
		if(detailcdr.sorting == 0 ) return;

		it++;
		eventcause = *it;
		i = eventcause.find(":");
		eventcause = eventcause.substr(i+1);

		it++;
		str = *it;
		i = str.find(":");
		str = str.substr(i+1);
		eventresult = Poco::NumberParser::parse(str);

		if( eventresult == 0)
			wReleaseType = "Normal";
		else
			wReleaseType = "NoAnswer"; 

		eventcause = eventcause + ",SIP";

	}else{
		// ����17002�ṹ
		// protocolid:115,btime:2016-11-12 14:40:57.349,linkid:0,spckind:24,opc:16585533,dpc:16723063,callingnum:035112340,  callednum:4038076,    ocallednum:,pcm:0,cic:11,answerdur:0,    ansoffset:3988,anmoffset:0,    reloffset:33648,rlcoffset:33836,sls:0,callingstflag:0,calledstatus:0,saooffset:0,msrn:4038076,loccationno:,calltranstype:0,eventresult:6,eventcause:4
		// protocolid:117,btime:2016-11-12 14:39:56.997,linkid:0,spckind:0, opc:5060,    dpc:5060,    callingnum:03575777665,callednum:15535779117,ocallednum:,pcm:2,cic:2, answerdur:74185,ansoffset:0,   anmoffset:21489,reloffset:95668,rlcoffset:1,    sls:2,callingstflag:0,calledstatus:0,saooffset:0,msrn:,       loccationno:,calltranstype:0,eventresult:0,eventcause:200
		
		it++;
		startTime = *it;
		i=startTime.find(":");
		startTime = startTime.substr(i+1);
		endTime = startTime;

		it += 3;
		detailcdr.opc = *it;
		i = detailcdr.opc.find(":");
		detailcdr.opc = detailcdr.opc.substr(i+1);

		it++;
		detailcdr.dpc = *it;
		i = detailcdr.dpc.find(":");
		detailcdr.dpc = detailcdr.dpc.substr(i+1);

		it++;
		detailcdr.caller = *it;
		i=detailcdr.caller.find(":");
		detailcdr.caller = detailcdr.caller.substr(i+1);

		it++;
		detailcdr.called = *it;
		i = detailcdr.called.find(":");
		detailcdr.called = detailcdr.called.substr(i+1);

		sortingCdr();
		// ��������к��붼�����û��б��У�ֱ�ӷ��أ����������ж�
		if(detailcdr.sorting == 0 ) return;

		it += 16;
		str = *it;
		i = str.find(":");
		str = str.substr(i+1);
		eventresult = Poco::NumberParser::parse( str );			// if failed,throw SyntaxException 
		
		it++;
		str = *it;
		i = str.find(":");
		eventcause = str.substr(i+1);

		switch( protocolid )
		{
			case 117:		// SIP CALL
			/*	
				���ں����¼���0���û�Ӧ�������ͷţ�1���û�Ӧ�𡢶�ʱ����ʱ�ͷţ�2���û�Ӧ�������쳣�ͷţ�3���û����ͣ�����ǰ���йһ�����4���������ͣ���������йһ�����5���ýв�Ӧ�������ʱ�ͷţ���6�������,�û��ܽӣ�7�������壬�û��ܽӣ�8���û�æ��100����������
				���ڳ�����֮����¼���101������������102��ʧ��
			*/
	
				if( eventresult < 5)
					wReleaseType = "Normal";
				else if( eventresult < 8 )	// ���оܾ�
					wReleaseType = "Reject"; 
				else if( eventresult == 8 )	// ����æ
					wReleaseType = "NoAnswer"; 
				else if(eventresult >99 )
					return;
				else
					wReleaseType = "Other";

				eventcause = eventcause + ",SIP";

				break;

			case 115:		// ISUP CALL---- eventresult���� 1����������(�û���ͨ)��2������ǰ���йһ���3����������йһ���4������󣬱��оܾ���5������æ(����)��6:����
	
				if( eventresult == 1 || eventresult == 2 || eventresult == 3)
					wReleaseType = "Normal";
				else if( eventresult == 4 )	// ���оܾ�
					wReleaseType = "Reject"; 
				else if( eventresult == 5 )	// ����æ
					wReleaseType = "NoAnswer"; 
				else
					wReleaseType = "Other";

				eventcause = eventcause + ",ISUP";

				break;

			case 114:		// TUP CALL

				if( eventresult == 1 || eventresult == 2 || eventresult == 3)
					wReleaseType = "Normal";
				else if( eventresult == 4 )	// ���оܾ�
					wReleaseType = "Reject"; 
				else if( eventresult == 5 )	// ����æ
					wReleaseType = "NoAnswer"; 
				else
					wReleaseType = "Other";

				eventcause = eventcause + ",TUP";
				break;

			default:
				break;
		}

	}

	app.logger().information( detailcdr.caller + "," + detailcdr.called + "," +  detailcdr.opc + "," + detailcdr.dpc +  "," + startTime + "," + endTime + "," + wReleaseType + "," + eventcause );
	sendCdrToGjdxServer( detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + eventcause ); 

	return;

}