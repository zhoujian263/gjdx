#================================================================
# 文件名: gjdx_user_sync.pl
# 版本: ver1.0
# 功能: 实现挂机短信平台同前端七号信令过滤接口机中用户的同步功能。同步所有启用状态的用户手机号
# 作者: zhoujian
# 创建日期: 2014-1-4
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
		# 新旧用户数不一致，肯定发生变化，用新文件覆盖旧文件
		print "userlist have changed! replace it.\r\n";
		run( "mv -f user.db userlist.ini" );
		return;
	}
	
	foreach( @new_users )
	{
		my $user=$_;

		#	检查new_users中的成员，是否在old_users中存在		
		if ( grep { $_ eq $user } @old_users )
		{
			print $user." exist!!! \n" ;
		}else{
			# 新旧用户成员不一致，用新文件覆盖旧文件
			print "userlist have changed! replace it.\r\n";
			run( "mv -f user.db userlist.ini" );
			return;
		}
	}
	print "userlist is unchanged,check it next time.\r\n";
	
}

main();
