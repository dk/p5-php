#$Id: test.pl,v 1.11 2005/05/24 09:54:57 dk Exp $

use Test::More tests => 27;
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
PHP::options( stdout => sub { $output = shift});
PHP::eval( 'echo 42;');
ok( $output eq '42', 'catch output');

# 8 
my $a = PHP::new_array();
ok( $a, 'get array from php');

# 9
my $b = PHP::ArrayHandle-> new();
ok( $b, 'create array handle');

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

# 19
my $arr = PHP::array;
$arr->[1] = 42;
ok( $arr->[1] == 42, 'pseudo-hash, as array');

# 20
$arr->{'mm'} = 42;
ok( $arr->{'mm'} == 42, 'pseudo-hash, as hash');

# 21
my @k = keys %$arr;
ok(( 2 == @k and 2 == scalar grep { m/^(1|mm)$/ } @k), 'hash keys');

$output = '';
SKIP:{
	skip "php5 required", 3 unless PHP::options('version') =~ /^(\d+)/ and $1 > 4;
	eval { PHP::eval(<<MOO); };
class P5 {
	function __construct() { echo "CREATE"; }
	function __destruct() { echo "DESTROY"; }
}
function p5(){\$a = new P5;return \$a;}
MOO
	{
	my $P5 = PHP::Object-> new('P5');
	ok(!$@ && $P5, 'php5 syntax');
	ok($output eq 'CREATE', 'php5 constructors');
	}
	ok($output eq 'DESTROY', 'php5 destructors');
}

# 25
ok( scalar( @$arr) == 2, 'sparse arrays');

# 26
ok( 5 == push (@$arr, qw(1 2 3)), 'push');

# 27
my $k = 0 || pop @$arr; 
ok(( 4 == @$arr and '3' eq $k), 'pop');

undef $arr;
