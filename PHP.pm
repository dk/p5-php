package PHP;

# $Id: PHP.pm,v 1.5 2005/02/15 16:11:26 dk Exp $

use strict;
require DynaLoader;
use vars qw($VERSION @ISA);
@ISA = qw(DynaLoader);

# remove this or change to 0x00 of your OS croaks here
sub dl_load_flags { 0x01 }

$VERSION = '0.02';
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
	PHP::exec( 1, $php_class, $self, @params);
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

1;

__DATA__

=pod

=head1 NAME

PHP - embedded PHP interpreter

=head1 DESCRIPTION

The module makes it possible to execute PHP code, call PHP functions and methods,
manipulating PHP arrays, and create PHP objects.

=head1 SYNOPSIS

	use PHP;

	# 1 - general use

	# 1.1
	# evaluate arbitrary PHP code; exception is thrown
	# and can be catched via standard eval{}/$@ block 
	PHP::eval(<<EVAL);
	function print_val(\$arr,\$val) {
		echo \$arr[\$val];
	}
	
	class TestClass {
		function TestClass () {}
		function method(\$val) { return \$val + 1; }
	};
	EVAL

	# catch output of PHP code
	PHP::options( stdout => sub {
		print "PHP says: $_[0]\n";
	});
	PHP::eval('echo 42;');

	# 2 - arrays
	# create a php array
	my $array = PHP::array();
	# tie it either to an array or a hash
	my ( @array, %hash);
	$array-> tie(\%hash);
	$array-> tie(\@array);

	# access array content
	$array[1] = 42;
	$hash{2} = 43;

	# pass arrays to function
	# Note - function name is not known by perl in advance, and
	# is called via AUTOLOAD
	PHP::print_val($a, 1);
	PHP::print_val($a, 2);

	# 3 - classes
	my $TestClass = PHP::Object-> new('TestClass');
	print $TestClass-> method(42), "\n";

=head1 API

=over

=item call FUNCTION ...

Calls PHP function with list of parameters. 
Returns exactly one value.

=item include, include_once, require, require_once

Shortcuts to the identical PHP constructs.

=item array

Returns a handle to a newly created PHP array of type C<PHP::Array>.
The handle can be later tied with perl hashes or arrays via C<tie> call.

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

Environment variable C<P5PHPDEBUG> set to 1 turns the debug mode on. The
same effect can be achieved programmatically by calling

	PHP::options( debug => 1);

=head1 INSTALLATION

The module uses php-embed SAPI extension to interoprate with PHP interpreter.
That means php must be configured with '--enable-embed' parameters prior to
using the module.

The C<sub dl_load_flags { 0x01 }> code in F<PHP.pm> is required for PHP
to load correctly its extenstions. If your platform does RTLD_GLOBAL by
default and croaks upon this line, it is safe to remove the line.

=head1 SEE ALSO

Using Perl code from PHP: L<http://www.zend.com/php5/articles/php5-perl.php>

=head1 COPYRIGHT

Copyright (c) 2005 catpipe Systems ApS. All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 AUTHOR

Dmitry Karasik <dmitry@karasik.eu.org>

=cut
