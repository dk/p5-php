# $Id: Makefile.PL,v 1.1 2005/02/14 16:08:39 dk Exp $
use ExtUtils::MakeMaker;

use strict;

my %config = map { 
	my $a = `php-config --$_`;
	unless ($a) {
	    print "Error: php-config --$_ failed\n";
	    exit 0;
	}
	chomp $a;
	$_ => $a;
} qw(includes prefix version);

$config{version} =~ s/^(\d+).*/$1/;

WriteMakefile(
    NAME       => 'PHP',
    VERSION_FROM => 'PHP.pm', 
    LIBS      => ["-L$config{prefix}/lib -lphp$config{version}", "-L$config{prefix}/lib/php5 -lphp$config{version}" ], 
    INC       => $config{includes}, 
    OBJECT    => "PHP\$(OBJ_EXT) array\$(OBJ_EXT)",
    CCFLAGS   => '-Wall',
);
