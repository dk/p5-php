/*
$Id: PHP.h,v 1.3 2005/03/15 18:56:24 dk Exp $
*/

#ifndef __P5PHP_H__
#define __P5PHP_H__

#include <php_embed.h>
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#define SV2ZANY(sv) get_php_entity(sv, -1)
#define SV2ZARRAY(sv) get_php_entity(sv, IS_ARRAY)
#define SV2ZOBJECT(sv) get_php_entity(sv, IS_OBJECT)

extern int p5PHP_opt_debug;
#define opt_debug p5PHP_opt_debug

extern void 
p5PHP_register_PHP_Array(void);
#define register_PHP_Array p5PHP_register_PHP_Array

extern void
p5PHP_debug( char * format, ...);
#define debug p5PHP_debug

#define Entity_create p5PHP_Entity_create
extern SV * 
p5PHP_Entity_create( char * class, zval * data);

#define get_php_entity p5PHP_get_php_entity
extern zval * 
p5PHP_get_php_entity( SV * perl_object, int check_type);

#define sv2zval p5PHP_sv2zval
extern int
p5PHP_sv2zval( SV * sv, zval * zarg, int suggested_type );

#define zval2sv p5PHP_zval2sv
extern SV *
p5PHP_zval2sv( zval * zobj);


#undef DEBUG
#define DEBUG if(opt_debug)debug

#endif
