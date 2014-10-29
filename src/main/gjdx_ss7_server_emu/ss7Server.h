#pragma comment(lib, "ws2_32.lib")

#include "Poco/Types.h"

using	Poco::UInt32;
using	Poco::UInt16;
using	Poco::UInt8;
using	Poco::Int32;

#define	TIMELABELLENS	12

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
	UInt16	data_type2;

}BINDBODY;

typedef struct recordbody
{
	UInt8	flag;
	UInt16	type;
	UInt16	ver_info;
	UInt8	format;
	UInt32	data_len;
	UInt32	pkg_len;
}RECORD;

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

#pragma pack()
