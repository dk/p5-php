/*
$Id: PHP.c,v 1.3 2005/02/16 13:50:37 dk Exp $
*/
#include "PHP.h"

int opt_debug = 0;

static int initialized = 0;
static HV *z_objects = NULL; 	/* SV => zval  */ 
static HV *z_links = NULL; 	/* SV => zval */ 
static SV *ksv = NULL;		/* local SV for key storage */
static SV *stdout_hook = NULL;/* if non-null, is a callback for stdout */
static SV *stderr_hook = NULL;/* if non-null, is a callback for stderr */
static char * eval_ptr = NULL;
/*
these macros allow re-entrant accumulation of php errors
to be reported, if any, by croak() 
*/
#define PHP_EVAL_BUFSIZE 2048
#define dPHP_EVAL   char eval_buf[PHP_EVAL_BUFSIZE], *old_eval_ptr
#define PHP_EVAL_ENTER \
	old_eval_ptr = eval_ptr;\
	eval_ptr = eval_buf;\
	eval_buf[0] = 0
#define PHP_EVAL_LEAVE eval_ptr = old_eval_ptr
#define PHP_EVAL_CROAK(default_message)	


void 
debug( char * format, ...)
{
	va_list args;

	if ( !opt_debug) return;
	va_start( args, format);
	vfprintf( stderr, format, args);
	fprintf( stderr, "\n");
	va_end( args);
}

/* use perl hashes to store non-sv values */

/* store and/or delete */
#define hv_delete_zval(h,key,kill) hv_store_zval(h,key,NULL,kill)
static void
hv_store_zval( HV * h, const SV* key, zval* val, int kill_object)
{
	HE *he;

	if ( !ksv) ksv = newSV( sizeof( SV*)); 
	sv_setpvn( ksv, ( char *) &key, sizeof( SV*));           
	he = hv_fetch_ent( h, ksv, 0, 0);
	if ( he) {
		zval * z = ( zval *) HeVAL( he);
		if ( z && kill_object) {
			DEBUG("%s 0x%x", val ? "replace" : "delete", z);
			zval_ptr_dtor( &z);
		}
		HeVAL( he) = &PL_sv_undef;
		hv_delete_ent( h, ksv, G_DISCARD, 0);
	}

	if ( val) {
		he = hv_store_ent( h, ksv, &PL_sv_undef, 0);
		HeVAL( he) = ( SV *) val;
	}
}

/* fetch */
static zval *
hv_fetch_zval( HV * h, const SV * key)
{
	SV ** v = hv_fetch( h, (char*)&key, sizeof(SV*), 0);
	return v ? (zval*)(*v) : NULL;
}

/* kill the whole hash */
static void
hv_destroy_zval( HV * h, int kill)
{
	HE * he;

	hv_iterinit( h);
	for (;;)
	{
		if (( he = hv_iternext( h)) == NULL) 
			break;

		if ( kill) {
			zval *value = ( zval*) HeVAL( he);
			DEBUG("force delete 0x%x", value);
		 	if ( value)
				zval_ptr_dtor( &value);
		}
		HeVAL( he) = &PL_sv_undef;
	}
	sv_free((SV *) h);
}

/* create a blessed instance of PHP::Entity */
SV *
Entity_create( char * class, void * data)
{
	SV * obj, * mate;
	dSP;
	
	ENTER;
	SAVETMPS;
	PUSHMARK( sp);
	XPUSHs( sv_2mortal( newSVpv( class, 0)));
	PUTBACK;
	perl_call_method( "CREATE", G_SCALAR);
	SPAGAIN;
	mate = SvRV( POPs);
	if ( !mate)
		croak("PHP::Entity::create: something really bad happened");
	obj = newRV_inc( mate);
	hv_store_zval( z_objects, mate, data, 1);
	PUTBACK;
	FREETMPS;
	LEAVE;
	
	DEBUG("new %s(0x%x), handle=0x%x", class, data, mate);

	return obj;
}

/* instantiate php object from a given class */
XS(PHP_Object__new)
{
	dXSARGS;
	STRLEN i, len;
	zval * object;
	zend_class_entry * 
#if PHP_MAJOR_VERSION > 4
		*
#endif
		zclass;
	char *class, *save_class, uclass[2048], *uc;

	if ( items != 2)
		croak("PHP::Object::new: 2 parameter expected");
	
	save_class = class = SvPV( ST( 1), len);

	DEBUG("new '%s'", save_class);

	if ( len > 2047) len = 2047;
	for ( i = 0, uc = uclass; i < len + 1; i++)
		*(uc++) = tolower( *(class++));

	if ( zend_hash_find(CG(class_table), uclass, len + 1, (void **) &zclass) == FAILURE)
		croak("PHP::Object::new: undefined class name '%s'", save_class);


	SP -= items;

	MAKE_STD_ZVAL( object);

	object_init_ex( object, 
#if PHP_MAJOR_VERSION > 4
		*
#endif
		zclass);

	XPUSHs( sv_2mortal( Entity_create( SvPV( ST(0), len), object)));
	PUTBACK;
	return;
}


/* map SV into zval */
zval * 
get_php_entity( SV * perl_object, int check_type)
{
	HV *obj;
	zval * z;
	
	if ( !SvROK( perl_object)) 
		return NULL;
	obj = (HV*) SvRV( perl_object);
	DEBUG("object? SV*(0x%x)", obj);
	
	z = hv_fetch_zval( z_objects, (SV*) obj); 

	if ( !z) {
		DEBUG("link? SV*(0x%x)", obj);
		z = hv_fetch_zval( z_links, (SV*) obj); 
	}
	
	if ( z && check_type >= 0 && z-> type != check_type)
		return NULL;
	return z;
}

/* copy SV content into ZVAL */
int
sv2zval( SV * sv, zval * zarg, int suggested_type )
{
	STRLEN len;

	if ( !SvOK( sv)) {
		DEBUG("%s: NULL", "sv2zval");
		zarg-> type = IS_NULL;
	} else if ( !SvROK( sv)) {
		int type;
		
		if ( suggested_type < 0) {
			if ( SvIOK( sv))
				type = SVt_IV;
			else if ( SvNOK( sv)) 
				type = SVt_NV;
			else if ( SvPOK( sv)) 
				type = SVt_PV;
			else
				type = -1;
		} else {
			type = suggested_type; 
		}
			
		switch ( type) {
		case SVt_IV:
			DEBUG("%s: LONG %d", "sv2zval", SvIV(sv));
			ZVAL_LONG(zarg, SvIV( sv));
			break;
		case SVt_NV:
			DEBUG("%s: DOUBLE %g", "sv2zval", SvNV(sv));
			ZVAL_DOUBLE(zarg, SvNV( sv));
			break;
		case SVt_PV: {
			char * c = SvPV( sv, len);
			DEBUG("%s: STRING %s", "sv2zval", c);
			ZVAL_STRINGL( zarg, c, len, 1);
			break;
		} 
		default:
			DEBUG("%s: cannot convert scalar %d/%s", "sv2zval", SvTYPE( sv), SvPV( sv, len));
			return 0;
		}
	} else {
		switch ( SvTYPE( SvRV( sv))) {
		case SVt_PVHV: {
			zval * obj;
			if (( obj = SV2ZANY( sv)) == NULL) {
				warn("%s: not a PHP entity %d/%s", 
					"sv2zval", SvTYPE( sv), SvPV( sv, len));
				return 0;
			}
			DEBUG("%s: %s %x ref=%d", "sv2zval", 
				(obj->type == IS_OBJECT) ? "OBJECT" : "ARRAY",
				obj, 
				obj-> refcount);
			*zarg = *obj;
			zval_copy_ctor( zarg);
			break;
		}	
		default:
			DEBUG("%s: cannot convert reference %d/%s", "sv2zval", SvTYPE( sv), SvPV( sv, len));
			return 0;
		}
	}

	return 1;
}

/* copy ZVAL content into a fresh SV */
SV *
zval2sv( zval * zobj)
{
	switch ( zobj-> type) {
	case IS_NULL:
		DEBUG("%s: NULL", "zval2sv");
		return &PL_sv_undef;
	case IS_BOOL:
		DEBUG("%s: BOOL %s", "zval2sv", Z_LVAL( *zobj) ? "TRUE" : "FALSE");
		return Z_LVAL( *zobj) ? &PL_sv_yes : &PL_sv_no;
	case IS_LONG:
		DEBUG("%s: LONG %d", "zval2sv", Z_LVAL( *zobj));
		return newSViv( Z_LVAL( *zobj));
	case IS_DOUBLE:
		DEBUG("%s: DOUBLE %d", "zval2sv", Z_DVAL( *zobj));
		return newSVnv( Z_DVAL( *zobj));
	case IS_STRING:
		DEBUG("%s: STRING %d", "zval2sv", Z_STRVAL( *zobj));
		return newSVpv( Z_STRVAL( *zobj), Z_STRLEN( *zobj));
	case IS_ARRAY: 
		DEBUG("%s: ARRAY %x ref=%d", "zval2sv", zobj, zobj-> refcount);
		return Entity_create( "PHP::Array", zobj);
	case IS_OBJECT:		
		DEBUG("%s: OBJECT %x ref=%d", "zval2sv", zobj, zobj-> refcount);
		return Entity_create( "PHP::Object", zobj);
	default:
		DEBUG("%s: ENTITY %x type=%i\n", "zval2sv", zobj, zobj->type);
		return Entity_create( "PHP::Entity", zobj);
	}
}

/* free zval corresponding to a SV */ 
XS(PHP_Entity_DESTROY)
{
	dXSARGS;
	zval * obj;

	if ( !initialized) /* if called after PHP::done */
		XSRETURN_EMPTY;
	
	if ( items != 1)
		croak("PHP::Entity::destroy: 1 parameter expected");

	if (( obj = SV2ZANY( ST(0))) == NULL)
		croak("PHP::Entity::destroy: not a PHP entity");

	DEBUG("delete object 0x%x", obj);
	hv_delete_zval( z_objects, SvRV( ST(0)), 1);
	
	PUTBACK;
	XSRETURN_EMPTY;
}

/* 
link and unlink manage a hash of aliases, used when different SVs can
represent single zval. This is useful for tied hashes and arrays.
*/
XS( PHP_Entity_link)
{
	dXSARGS;
	zval * obj;

	if ( items != 2)
		croak("PHP::Entity::link: 2 parameters expected");

	if (( obj = SV2ZANY( ST(0))) == NULL)
		croak("PHP::Entity::link: not a PHP entity");

	DEBUG("link 0x%x => %x", obj, SvRV( ST( 1)));
	hv_store_zval( z_links, SvRV( ST(1)), obj, 1);
	
	PUTBACK;
	XSRETURN_EMPTY;
}

XS( PHP_Entity_unlink)
{
	dXSARGS;

	if ( items != 1)
		croak("PHP::Entity::unlink: 1 parameter expected");

	DEBUG("unlink 0x%x", SvRV( ST( 0)));
	hv_store_zval( z_links, SvRV( ST( 0)), NULL, 1);
	
	PUTBACK;
	XSRETURN_EMPTY;
}

#define ZARG_STATIC_BUFSIZE 32
/* call a php function or method, croak if it fails */
XS(PHP_exec)
{
	dXSARGS;
	dPHP_EVAL;
	STRLEN len;
	int i, zargc, zobject, as_method;
	zval *retval;
	SV * retsv;
	
	/* zvals with actial scalar values */
	static zval *zargv_static[ZARG_STATIC_BUFSIZE];
	zval **zargv, **zarg;
	/* array of pointers to these zvals */
	static zval **pargv_static[ZARG_STATIC_BUFSIZE];
	zval ***pargv;

	
	(void)items;

	if ( items < 2)
		croak("%s: expect at least 2 parameters", "PHP::exec");

	zobject = -1;
	as_method = SvIV( ST(0));

#define METHOD ( as_method ? "PHP::method" : "PHP::exec")
	
	DEBUG("%s(%s)(%d args)", 
		METHOD,
		SvPV( ST(1), len), 
		items-1);

	/* alloc arguments */
	zargc = items - 1;
	if ( zargc <= ZARG_STATIC_BUFSIZE) {
		zargv = zargv_static;
		pargv = pargv_static;
	} else {
		if ( !( zargv = malloc( 
			sizeof( zval*) * zargc
			+ 
			sizeof( zval**) * zargc
			))) 
			croak("%s: not enough memory (%d bytes)", 
				METHOD, sizeof( void*) * zargc * 2);
		pargv = (zval***)(zargv + zargc);
	}
	for ( i = 0; i < zargc; i++) {
		pargv[i] = zargv + i;
		MAKE_STD_ZVAL( zargv[i]);
		zargv[i]-> type = IS_NULL;
	}

	/* common cleanup code */
#define CLEANUP \
	for ( i = 0; i < zargc; i++) zval_ptr_dtor( zargv + i);\
	if ( zargv != zargv_static) free( zargv);

	/* parse and store arguments */
	for ( i = 0, zarg = zargv; i < zargc; i++, zarg++) {
		if ( !sv2zval( ST(i+1), *zarg, 
			i ? -1 : SVt_PV)) {  /* name can be something else it seems */
			CLEANUP;
			croak("%s: parameter #%d is of unsupported type and cannot be passed", METHOD, i+1); 
		}

		if ( zobject < 0 && (*zarg)->type == IS_OBJECT)
			zobject = i;
	}

	if ( as_method && zobject != 1) {
		CLEANUP;
		croak("%s: first parameter must be an object", METHOD);
	}

	/* issue php call */
	PHP_EVAL_ENTER;
	TSRMLS_FETCH();
	if(call_user_function_ex(
		( as_method ? NULL : CG(function_table)), /* namespace */
		( as_method ? zargv + 1 : NULL),	  /* object */	
		zargv[0],			          /* function name */	
		&retval, 				  /* return zvalue */
		zargc - 1 - as_method,			  /* param count */ 
		pargv + 1 + as_method, 			  /* param vector */
		0, NULL TSRMLS_CC) != SUCCESS)
	{
		CLEANUP;
		PHP_EVAL_LEAVE;
		if ( eval_buf[0])
			croak("%s", eval_buf);
		else
			croak("%s: function %s call failed", METHOD, SvPV(ST(1), len));
	}
	PHP_EVAL_LEAVE;

	/* read and parse results */
	SPAGAIN;
	SP -= items;

	if ( !( retsv = zval2sv( retval))) {
		warn("%s: function return value cannot be converted\n", METHOD);
		retsv = &PL_sv_undef;
	}
	retsv = sv_2mortal( retsv);
	XPUSHs( retsv);
	CLEANUP;
	if ( retval-> type != IS_OBJECT && retval-> type != IS_ARRAY) 
		zval_ptr_dtor( &retval);
#undef CLEANUP
#undef METHOD

	PUTBACK;
	return;
}

/* eval php code, croak on failure */
XS(PHP_eval)
{
	dXSARGS;
	dPHP_EVAL;

	STRLEN na;
	(void)items;

	DEBUG("PHP::eval(%d args)", items);
	if ( items < 0 || items > 2)
		croak("PHP::eval: expect 1 parameter");
	
	PHP_EVAL_ENTER;
	if ( zend_eval_string( SvPV( ST(0), na), NULL, "Embedded code" TSRMLS_CC) == FAILURE) {
		PHP_EVAL_LEAVE;
		croak( "%s", eval_buf[0] ? eval_buf : "PHP::eval failed");
	}
	PHP_EVAL_LEAVE;
	
	PUTBACK;
	XSRETURN_EMPTY;
}

/* get and set various options */
XS(PHP_options)
{
	dXSARGS;
	STRLEN na;
	char * c;

	(void)items;

	if ( items > 2) 
		croak("PHP::options: must be 0, 1, or 2 parameters");

	switch ( items) {
	case 0:
		SPAGAIN;
		SP -= items;
		EXTEND( sp, 1);
		PUSHs( sv_2mortal( newSVpv( "debug", 5)));
		PUSHs( sv_2mortal( newSVpv( "stdout", 6)));
		PUSHs( sv_2mortal( newSVpv( "stderr", 6)));
		return;
	case 1:
	case 2:
		c = SvPV( ST(0), na);
		if ( strcmp( c, "debug") == 0) {
			if ( items == 1) {
				XPUSHs( sv_2mortal( newSViv( opt_debug)));
			} else {
				opt_debug = SvIV( ST( 1));
			}
		} else if ( 
			strcmp( c, "stdout") == 0 ||
			strcmp( c, "stderr") == 0
			) {
			SV ** ptr = ( strcmp( c, "stdout") == 0) ? 
				&stdout_hook : &stderr_hook;
			if ( items == 1) {
				if ( *ptr)
					XPUSHs( sv_2mortal( newSVsv( *ptr)));
				else
					XPUSHs( &PL_sv_undef);
				PUTBACK;
				return;
			} else {
				SV * hook = ST( 1);
				if ( SvTYPE( hook) == SVt_NULL) {
					if ( *ptr) 
						sv_free( *ptr);
					*ptr = NULL;
					PUTBACK;
					return;
				}
			   	if ( !SvROK( hook) || ( SvTYPE( SvRV( hook)) != SVt_PVCV)) {
					warn("PHP::options::stdout: Not a CODE reference passed");
					PUTBACK;
					return; 
				}
				if ( *ptr) 
					sv_free( *ptr);
				*ptr = newSVsv( hook);
				PUTBACK;
			}
		} else {
			croak("PHP::options: unknown option `%s'", c);
		}
	}
	
	XSRETURN_EMPTY;
}

/* collect php warnings */
static void
mod_log_message( char * message)
{
	if ( eval_ptr) 
		strlcat( eval_ptr, message, PHP_EVAL_BUFSIZE);

	if ( stderr_hook) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK( sp);
		XPUSHs( sv_2mortal( newSVpv( message, 0)));
		PUTBACK;
		perl_call_sv( stderr_hook, G_DISCARD);
		SPAGAIN;
		FREETMPS;
		LEAVE;
	} else if ( !eval_ptr) { 
		/* eventual warnings in code outside eval and exec */
		warn("%s", message);
	}
}

/* get php stdout */
static int 
mod_ub_write(const char *str, uint str_length TSRMLS_DC)
{
	if ( stdout_hook) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK( sp);
		XPUSHs( sv_2mortal( newSVpvn( str, str_length)));
		PUTBACK;
		perl_call_sv( stdout_hook, G_DISCARD);
		SPAGAIN;
		FREETMPS;
		LEAVE;
		return str_length;
	} else {
		return PerlIO_write( PerlIO_stdout(), str, str_length);
	}
}

/* php-embed call fflush() here - well, we don't */
static int 
mod_deactivate(TSRMLS_D)
{
	return SUCCESS;
}

/* stop PHP embedded module */
XS(PHP_done)
{
	dXSARGS;
	(void)items;

	initialized = 0;

	hv_destroy_zval( z_links, 0);
	hv_destroy_zval( z_objects, 1);
	sv_free( ksv);
	z_objects = z_links = NULL;
	ksv = NULL;
	if ( stdout_hook) {
		sv_free( stdout_hook);
		stdout_hook = NULL;
	}
	if ( stderr_hook) {
		sv_free( stderr_hook);
		stderr_hook = NULL;
	}

	php_end_ob_buffers(1 TSRMLS_CC);
	php_embed_shutdown(TSRMLS_C);
	DEBUG("PHP::done");
	XSRETURN_EMPTY;
}

/* initialization section */
XS( boot_PHP)
{
	dXSARGS;
	sig_t sig;
	(void)items;
	
	XS_VERSION_BOOTCHECK;

	/* php_embed_init calls signal( SIGPIPE, SIGIGN) for some weird reason -
	   make a work-around */
	sig = signal( SIGPIPE, SIG_IGN);
	php_embed_init(0, NULL PTSRMLS_CC);
	signal( SIGPIPE, sig);
	/* just for the completeness sake, it also does weird
	  setmode(_fileno(stdin/stdout/stderr), O_BINARY)
	  on win32, but I don't really care about this */

	/* overload embed default values and output routines */
	PG(display_errors) = 0;
	PG(log_errors) = 1;
	sapi_module. log_message	= mod_log_message;
	sapi_module. ub_write		= mod_ub_write;
	sapi_module. deactivate		= mod_deactivate;

	php_output_startup();
	php_output_activate(TSRMLS_C);

	/* init our stuff */
	z_objects = newHV();
	z_links = newHV();
	
	newXS( "PHP::done", PHP_done, "PHP");
	newXS( "PHP::options", PHP_options, "PHP");
	
	newXS( "PHP::exec", PHP_exec, "PHP");
	newXS( "PHP::eval", PHP_eval, "PHP");
	
	newXS( "PHP::Entity::DESTROY", PHP_Entity_DESTROY, "PHP::Entity");
	newXS( "PHP::Entity::link", PHP_Entity_link, "PHP::Entity");
	newXS( "PHP::Entity::unlink", PHP_Entity_unlink, "PHP::Entity");
	
	newXS( "PHP::Object::_new", PHP_Object__new, "PHP::Object");

	register_PHP_Array();

	initialized = 1;
	
	ST(0) = newSViv(1);
	
	XSRETURN(1);
}

