#================================================================
# �ļ���: gjdx_user_sync.pl
# �汾: ver1.0
# ����: ʵ�ֹһ�����ƽ̨ͬǰ���ߺ�������˽ӿڻ����û���ͬ�����ܡ�ͬ����������״̬���û��ֻ���
# ����: zhoujian
# ��������: 2014-1-4
#================================================================

#!/usr/bin/perl
use strict;

sub fatal
{
	printf STDERR $_[0];
	printf STDERR "error:[$!]\n";
	exit -1;
}
sub run
{
	print "$_[0]\n";
	if( system("$_[0]") != 0 )
	{
		fatal "run $_[0] fail]\n";
	}
}

sub main
{
	
	run( "mysql -ugjdx -pgjdx -h192.168.2.12 ltdb -N -e\"select distinct mobile from lbs_user where enabled=1 \">user.db" );

	my $new_user_str = `cat user.db`;
	my @new_users = split( /\n/, $new_user_str );

	my $old_user_str = `cat userlist.ini`;
	my @old_users = split( /\n/, $old_user_str );
	
	if( @new_users ne @old_users )
	{
		# �¾��û�����һ�£��϶������仯�������ļ����Ǿ��ļ�
		print "userlist have changed! replace it.\r\n";
		run( "mv -f user.db userlist.ini" );
		return;
	}
	
	foreach( @new_users )
	{
		my $user=$_;

		#	���new_users�еĳ�Ա���Ƿ���old_users�д���		
		if ( grep { $_ eq $user } @old_users )
		{
			print $user." exist!!! \n" ;
		}else{
			# �¾��û���Ա��һ�£������ļ����Ǿ��ļ�
			print "userlist have changed! replace it.\r\n";
			run( "mv -f user.db userlist.ini" );
			return;
		}
	}
	print "userlist is unchanged,check it next time.\r\n";
	
}

main();
