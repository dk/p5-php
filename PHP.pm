package PHP;

# $Id: PHP.pm,v 1.16 2005/03/09 09:19:18 dk Exp $

use strict;
require DynaLoader;
use vars qw($VERSION @ISA);
@ISA = qw(DynaLoader);

# remove this or change to 0x00 of your OS croaks here
sub dl_load_flags { 0x01 }

$VERSION = '0.05';
bootstrap PHP $VERSION;

PHP::options( debug => 1) if $ENV{P5PHPDEBUG}; 

sub END
{
	&PHP::done();
}

sub call	{ PHP::exec( 0, @_) }
sub include	{ PHP::eval( "include('$_[0]');") }
sub require	{ PHP::eval( "require('$_[0]');") }
sub include_once{ PHP::eval( "include_once('$_[0]');") }
sub require_once{ PHP::eval( "require_once('$_[0]');") }

sub array	{ PHP::Array-> new() }
sub hash        { PHP::PseudoHash-> new() }

my $LOADED = 1;

sub AUTOLOAD
{
	die "Module PHP failed to load" unless $LOADED;
	no strict;
	my $method = $AUTOLOAD;
	$method =~ s/^.*://;
	PHP::exec( 0, $method, @_);
}

# arrays and objects base class
package PHP::Entity;

sub CREATE
{
	my $class = shift;
	my $self = {};
	bless( $self, $class);
	return $self;
}

sub tie
{
	my ( $self, $tie_to) = @_;
	if ( ref( $tie_to) eq 'HASH') {
		tie %$tie_to, 'PHP::TieHash', $self;
	} elsif ( ref( $tie_to) eq 'ARRAY') {
		tie @$tie_to, 'PHP::TieArray', $self;
	} else {
		die "PHP::Array::tie: Can't tie to ", ref($tie_to), "\n";
	}
}

package PHP::Object;
use vars qw(@ISA);
@ISA = qw(PHP::Entity);

sub new
{
	my ( $class, $php_class, @params) = @_;
	my $self = $class-> _new( $php_class);
	PHP::exec( 1, $php_class, $self, @params)
		if PHP::exec( 0, 'method_exists', $self, $php_class);
	return $self;
}

sub AUTOLOAD
{
	no strict;
	my $method = $AUTOLOAD;
	$method =~ s/^.*://;
	PHP::exec( 1, $method, @_);
}

package PHP::Array;
use vars qw(@ISA);
@ISA = qw(PHP::Entity);

package PHP::TieHash;

sub TIEHASH
{
	my ( $class, $self) = @_;
	my $alias = {};
	PHP::Entity::link( $self, $alias);
	return bless( $alias, $class);
}

sub UNTIE
{
	PHP::Entity::unlink( $_[0] );
}

package PHP::TieArray;

sub TIEARRAY
{
	my ( $class, $self) = @_;
	my $alias = {};
	PHP::Entity::link( $self, $alias);
	return bless( $alias, $class);
}

sub UNTIE
{
	PHP::Entity::unlink( $_[0] );
}

sub EXTEND {}
sub STORESIZE {}

package PHP::PseudoHash;

my ( %hash_instances, %array_instances);

use overload 
	'%{}' => sub { $hash_instances{PHP::stringify($_[0])} },
	'@{}' => sub { $array_instances{PHP::stringify($_[0])} },
	'""'  => sub { PHP::stringify($_[0]) };

sub new
{
	my $class = shift;
	my $handle = PHP::Array-> new;
	my $hash_instance = {};
	my $array_instance = [];
	my $self = {
		hash  => $hash_instance,
		array => $array_instance,
		handle => $handle,
	};
	my $string = PHP::stringify( $self);
	$hash_instances{$string} = $hash_instance;
	$array_instances{$string} = $array_instance;
	tie %$hash_instance, 'PHP::TieHash', $handle;
	tie @$array_instance, 'PHP::TieArray', $handle;
	bless ( $self, $class);
	return $self;
}

sub DESTROY
{
	my $string = PHP::stringify( $_[0]);
	delete $hash_instances{ $string};
	delete $array_instances{ $string};
}

1;

__DATA__

=pod

=head1 NAME

PHP - embedded PHP interpreter

=head1 DESCRIPTION

The module makes it possible to execute PHP code, call PHP functions and methods,
manipulate PHP arrays, and create PHP objects.

=head1 SYNOPSIS

	use PHP;

General use

	# evaluate arbitrary PHP code; exception is thrown
	# and can be caught via standard eval{}/$@ block 
	PHP::eval(<<EVAL);
	function print_val(\$arr,\$val) {
		echo \$arr[\$val];
	}
	
	class TestClass {
		function TestClass ( $param ) {}
		function method(\$val) { return \$val + 1; }
	};
	EVAL

	# catch output of PHP code
	PHP::options( stdout => sub {
		print "PHP says: $_[0]\n";
	});
	PHP::eval('echo 42;');

Arrays, high level

	# create a php array
	my $array = PHP::hash;

	# access pseudo-hash content
	$array-> [1] = 42;
	$array-> {string} = 43;
	
	# pass arrays to function
	# Note - function name is not known by perl in advance, and
	# is called via AUTOLOAD
	PHP::print_val($array, 1);
	PHP::print_val($array, 'string');

Arrays, low level

	# create a php array
	my $array = PHP::array;
	# tie it either to an array or a hash
	my ( @array, %hash);
	$array-> tie(\%hash);
	$array-> tie(\@array);

	# access array content
	$array[1] = 42;
	$hash{2} = 43;

Objects and properties

	my $TestClass = PHP::Object-> new('TestClass');
	print $TestClass-> method(42), "\n";
	
	$TestClass-> tie(\%hash);
	# set a property
	$hash{new_prop} = 'string';

=head1 API

=over

=item eval $CODE

Feeds embedded PHP interpreter with $CODE, throws an exception on
failure.

=item call FUNCTION ...

Calls PHP function with list of parameters. 
Returns exactly one value.

=item include, include_once, require, require_once

Shortcuts to the identical PHP constructs.

=item array

Returns a handle to a newly created PHP array of type C<PHP::Array>.
The handle can be later tied with perl hashes or arrays via C<tie> call.

=item hash

Returns a handle to a newly created C<PHP::PseudoHash> object, which 
can be accessed both as array and hash reference:

	$_ = PHP::hash;
	$_->[42] = 'hello';
	$_->{world} = '!';

=item PHP::Object->new($class_name, @parameters)

Instantiates a PHP object of PHP class $class_name and returns a handle to it.
The methods of the class can be called directly via the handle:

	my $obj = PHP::Object-> new( 'MyClass', @params_to_constructor);
	$object-> method( @some_params);


=item PHP::Entity->tie($array_handle, $tie_to)

Ties existing handle to a PHP entity to either a perl hash or a perl array.
The tied hash or array can be used to access PHP pseudo_hash values indexed
either by string or integer value. 

The PHP entity can be either an array, represented by C<PHP::Array>, or
an object, represented by C<PHP::Object>. In the latter case, the object 
properties are represented as hash/array values.

=item PHP::Entity->link($original, $link)

Records a reference to an arbitrary perl scalar $link as an
alias to $original C<PHP::Entity> object. This is used internally
by C<PHP::TieHash> and C<PHP::TieArray>, but might be also used
for other purposes.

=item PHP::Entity::unlink($link)

Removes association between a C<PHP::Entity> object and $link.

=item PHP::options

Contains set of internal options. If called without parameters,
returns the names of the options. If called with a single parameter,
return the associated value. If called with two parameters, replaces
the associated value.

=over

=item debug $integer

If set, loads of debugging information are dumped to stderr

Default: 0

=item stdout/stderr $callback

C<stdout> and C<stderr> options define callbacks that are called
when PHP decides to print something or complain, respectively.

Default: undef

=back

=back

=head1 DEBUGGING

Environment variable C<P5PHPDEBUG>, if set to 1, turns the debug mode on. The
same effect can be achieved programmatically by calling

	PHP::options( debug => 1);

=head1 INSTALLATION

The module uses php-embed SAPI extension to inter-operate with PHP interpreter.
That means php must be configured with '--enable-embed' parameters prior to
using the module. Also, no '--with-apxs' must be present in to configuration
agruments either, otherwise the PHP library will be linked with Apache functions,
and will be unusable from the command line.

The C<sub dl_load_flags { 0x01 }> code in F<PHP.pm> is required for PHP
to load correctly its extensions. If your platform does RTLD_GLOBAL by
default and croaks upon this line, it is safe to remove the line.

=head1 WHY?

While I do agree that in general it is absolutely pointless to use PHP
functionality from within Perl, scenarios where one must connect an existing
PHP codebase to something else, are not something unusual. Also, this module
might be handy for people who know PHP but afraid to switch to Perl, or want to
reuse their old PHP code.

Currently, not all PHP functionality is implemented, but OTOH I don't really
expect this module to grow that big, because I believe it is easier to call
C<PHP::eval> rather than implement all the subtleties of Zend API. There are no
callbacks to Perl from PHP code, and I don't think these are needed, because
one thing is to be lazy and not to rewrite PHP code, and another is to make new
code in PHP that uses Perl when PHP is not enough. As I see it, the latter
would kill all incentive to switch to Perl, so I'd rather leave callbacks
unimplemented.

=head1 SEE ALSO

Using Perl code from PHP: L<http://www.zend.com/php5/articles/php5-perl.php>

=head1 COPYRIGHT

Copyright (c) 2005 catpipe Systems ApS. All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 AUTHOR

Dmitry Karasik <dk@catpipe.net>

=cut
