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
		}catch( Exception& exc )
		{
			app.logger().information( exc.displayText() );
			break;
		}

		if (  sendCheckLink == 1 ){
			sendCheckLinkToSS7Server(app);
			sendCheckLink = 0;
		}

	}

	// 输出最终的统计数据
	app.logger().information("totalRecord:" + NumberFormatter::format(totalRecord) + " totalCdr:" + NumberFormatter::format(totalCdr) + " IsupCdr:" + NumberFormatter::format(totalIsupCdr) + " SipCdr:" + NumberFormatter::format(totalSipCdr) + " TupCdr:" + NumberFormatter::format(totalTupCdr)  + " MapCdr:" + NumberFormatter::format(totalMapCCCdr) + " UnknowCdr:" + NumberFormatter::format(totalUnknowCdr) );

	Poco::Util::ServerApplication::terminate();
}

void CdrFilterTask::sendCdrToGjdxServer(Application& app,string msg)
{
	try
	{
		gjdx_socket.sendBytes(msg.c_str(), msg.length());
	}
	catch ( NetException& exc)
	{
		 app.logger().information( exc.displayText() );
	}
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
	
	bind->data_type_count = 2;
	bind->data_type1=htons(17002);
	bind->data_type2=htons(17006);

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

		}else{
			string tmp = format("%2X %2X %2X %2X \n",(UInt32)recvbuf[0],(UInt32)recvbuf[1],(UInt32)recvbuf[2],(UInt32)recvbuf[3]);
			app.logger().information("get message head data " + tmp);
			throw Poco::ApplicationException("ss7server protocal error:lost message head flag");
		}
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
	// 如果主被叫号码都不在用户列表中，直接返回，不做后面判断
	if(detailcdr.sorting == 0 ) return;

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	reason = Poco::NumberParser::parse( wReleaseType );			// if failed,throw SyntaxException 

/*	ISUP 释放原因分析
	13、117---124属于正常拆线呼叫		释放原因定为NORMAL
	14---20			属于用户未应答			释放原因定为NOANSWER
	21、131			属于用户主动拒接		释放原因定为REJECT
	
	0---5			基本属于拨错误			此部分呼叫不向挂机短信平台传递
	22				被叫换号				此部分呼叫不向挂机短信平台传递
	27				目的地不可达			此部分呼叫不向挂机短信平台传递
	28				无效号码格式			此部分呼叫不向挂机短信平台传递

	其它类别		暂定属于网络原因呼损	释放原因定为OTHER

	具体定义见中兴公司的《电信挂机短信接口方案（适用V5.x版本）》

*/
	if( reason == 13 )		// 正常释放
		wReleaseType = "Normal";
	else if( reason >= 117 && reason <= 124 )	// 应答计费指示、连接计费指示，归入正常呼叫
		wReleaseType = "Normal";
	else if( reason >= 14 && reason <= 20 )	// 用户忙、用户未应答、用户未响应、用户缺席
		wReleaseType = "NoAnswer"; 
	else if( reason ==21 || reason== 131 )	// 用户拒接
		wReleaseType = "Reject";
	else if( reason < 5 || reason == 22 || reason == 27 || reason == 28 )	// 号码错误
		return;
	else
		wReleaseType = "Other";


	app.logger().information( detailcdr.caller + "," + detailcdr.called + "," +  detailcdr.opc + "," + detailcdr.dpc +  "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" );
//	app.logger().information( caller + "," + called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" );
	sendCdrToGjdxServer( app,detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(reason) + ",ISUP" ); 
	
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
	// 如果主被叫号码都不在用户列表中，直接返回，不做后面判断
	if(detailcdr.sorting == 0 ) return;

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	issucess = Poco::NumberParser::parse( wReleaseType );			// 取出issucess值，if failed,throw SyntaxException 

	it++;
	wReleaseType = *it;
	i = wReleaseType.find(":");
	wReleaseType = wReleaseType.substr(i+1);
	result = Poco::NumberParser::parse( wReleaseType );			// 取出result值，if failed,throw SyntaxException 

/*	MAP 呼叫释放原因分析
	optype 15   位置申请，手机向MSC发送
	optype 16	路由申请，MSC向HLR发送

	差错码1 = 1	中文描述1 = 未分配的电话号码	英文描述1 = Unassigned directory number
	差错码2 = 2	中文描述2 = 移动台去活	英文描述2 = Inactive
	差错码3 = 3	中文描述3 = 移动台忙	英文描述3 = Busy
	差错码4 = 4	中文描述4 = 不允许将呼叫终结到移动台	英文描述4 = Termination Denied
	差错码5 = 5	中文描述5 = 无寻呼响应	英文描述5 = No Page Response
	差错码6 = 6	中文描述6 = 不可用	英文描述6 = Unavailable

	具体定义见中兴公司的《电信挂机短信接口方案（适用V5.x版本）》
*/
	if( optype != 15 )	return;

	if( issucess == 0 && result == 255 )	// 正常呼叫
		wReleaseType = "Normal";
	else if( issucess == 3 && result == 3 )	// MAP错误，移动台忙
		wReleaseType = "NoAnswer"; 
	else
		wReleaseType = "Other";
	
	app.logger().information( detailcdr.caller + "," + detailcdr.called + "," +  detailcdr.opc + "," + detailcdr.dpc +  "," + startTime + "," + endTime + "," + NumberFormatter::format(issucess) + "," + NumberFormatter::format(result)+ "," + NumberFormatter::format(optype)+ ",MAP" );	
//	app.logger().information( caller + "," + called + "," + startTime + "," + endTime + "," + NumberFormatter::format(issucess) + "," + NumberFormatter::format(result)+ "," + NumberFormatter::format(optype)+ ",MAP" );	
	sendCdrToGjdxServer( app, detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + NumberFormatter::format(result) + ",MAP" ); 
	
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

	// 被叫如果开通彩玲业务，会在被叫号码前加 70096 或 70090 前缀
	if(detailcdr.called.compare(0,3,"700") == 0 ) detailcdr.called = detailcdr.called.substr(5);

	// 主叫如果是座机号码，且没有加区号，根据opc给其增加区号
	if(detailcdr.caller.length()<=8)
	{
		detailcdr.caller=sm->Spcode2Areacode(detailcdr.opc) + detailcdr.caller;
	}
	// 被叫如果是座机号码，且没有加区号，根据dpc给其增加区号
	if(detailcdr.called.length()<=8)
	{
		detailcdr.called=sm->Spcode2Areacode(detailcdr.dpc) + detailcdr.called;
	}else if(detailcdr.called.compare(0,1,"3") == 0){
		detailcdr.called= "0" + detailcdr.called;
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

		case 115:		// ISUP CALL---- eventresult定议 1：正常呼叫(用户接通)；2：振铃前主叫挂机；3：振铃后主叫挂机；4：振铃后，被叫拒绝；5：被叫忙(优先)；6:其它
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
		// 分析17006结构
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

		it += 3;
		str = *it;
		i = str.find(":");
		eventid =str.substr(i+1);
		if( eventid.compare(0,2,"15") != 0 )	return;   // 不是呼叫记录，直接放弃

		sortingCdr();
		// 如果主被叫号码都不在用户列表中，直接返回，不做后面判断
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
		// 分析17002结构
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
		// 如果主被叫号码都不在用户列表中，直接返回，不做后面判断
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
				对于呼叫事件：0、用户应答，正常释放；1、用户应答、定时器超时释放；2、用户应答、其他异常释放；3、用户早释（振铃前主叫挂机）；4、振铃早释（振铃后主叫挂机）；5、久叫不应（振铃后超时释放）；6、振铃后,用户拒接；7、无振铃，用户拒接；8、用户忙；100、其他呼损
				对于除呼叫之外的事件：101、正常结束；102、失败
			*/
	
				if( eventresult < 5)
					wReleaseType = "Normal";
				else if( eventresult < 8 )	// 被叫拒绝
					wReleaseType = "Reject"; 
				else if( eventresult == 8 )	// 被叫忙
					wReleaseType = "NoAnswer"; 
				else if(eventresult >99 )
					return;
				else
					wReleaseType = "Other";

				eventcause = eventcause + ",SIP";

				break;

			case 115:		// ISUP CALL---- eventresult定议 1：正常呼叫(用户接通)；2：振铃前主叫挂机；3：振铃后主叫挂机；4：振铃后，被叫拒绝；5：被叫忙(优先)；6:其它
	
				if( eventresult == 1 || eventresult == 2 || eventresult == 3)
					wReleaseType = "Normal";
				else if( eventresult == 4 )	// 被叫拒绝
					wReleaseType = "Reject"; 
				else if( eventresult == 5 )	// 被叫忙
					wReleaseType = "NoAnswer"; 
				else
					wReleaseType = "Other";

				eventcause = eventcause + ",ISUP";

				break;

			case 114:		// TUP CALL

				if( eventresult == 1 || eventresult == 2 || eventresult == 3)
					wReleaseType = "Normal";
				else if( eventresult == 4 )	// 被叫拒绝
					wReleaseType = "Reject"; 
				else if( eventresult == 5 )	// 被叫忙
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
	sendCdrToGjdxServer( app, detailcdr.caller + "," + detailcdr.called + "," + startTime + "," + endTime + "," + wReleaseType + "," + eventcause ); 

	return;

}
