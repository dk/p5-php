#$Id: test.pl,v 1.4 2005/02/23 11:13:28 dk Exp $

use Test::More tests => 18;
use strict;

BEGIN { use_ok('PHP'); }
require_ok('PHP');

# 3 
eval {
PHP::eval(<<S1);

function loopback( \$var)
{
	return \$var;
}

function new_array()
{
	return array();
}

function print_val(\$arr,\$val)
{
	echo \$arr[\$val];
}

class TestClass
{
	var \$prop;
	function TestClass (\$a) { echo \$a; }
	function method(\$val) { return \$val + 1; }
	function getprop() { return \$this->prop; }
};

S1
};
ok( !$@, 'eval');
die $@ if $@;

# 4,5
my $val;
eval {
	$val = PHP::call( 'loopback', 42);
};
ok( !$@, 'call');
ok( defined $val && $val eq '42', 'pass arguments, return values');

# 6
eval {
	PHP::eval('$');
};
ok( $@, 'invalid syntax exceptions');

# 7 
my $output = '';
PHP::options( stdout => sub { $output = shift; });
PHP::eval( 'echo 42;');
ok( $output eq '42', 'catch output');

# 8 
my $a = PHP::new_array();
ok( $a, 'get array from php');

# 9
my $b = PHP::array();
ok( $b, 'create array');

my ( @array, %hash);
$a->tie(\%hash);
$a->tie(\@array);

# 10
$array[1] = 'array';
ok( defined $array[1] && $array[1] eq 'array', 'tied array');

# 11
$hash{'h'} = 'hash';
ok( defined $hash{'h'} && $hash{'h'} eq 'hash', 'tied hash');

# 12
PHP::print_val($a, 1);
ok( $output eq 'array', 'query array value');

# 13
PHP::print_val($a, 'h');
ok( $output eq 'hash', 'query hash value');

# 14
PHP::TieHash::STORE( $b, '42', '42');
ok( PHP::TieHash::FETCH( $b, '42') eq '42', 'direct array access');

# 15
$output = '';
my $TestClass = PHP::Object-> new('TestClass', '43');
ok( $TestClass && $output eq '43', 'class');

# 16
ok( $TestClass-> method(42) == 43, 'methods');

# 17
$TestClass->tie(\%hash);
$hash{prop} = 42;
ok( $TestClass-> getprop() == 42, 'properties');

# 18
eval {
PHP::eval('call_unexistent_function_wekljfhv2kwfwkfvbwkfbvwjkfefv();');
};
ok($@ && $@ =~ /call_unexistent_function/, 'undefined function exceptions');
