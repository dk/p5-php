/* 

$Id: array.c,v 1.1.1.1 2005/02/14 16:08:39 dk Exp $ 

Implemenmtation of PHP::TieHash and PHP::TieArray methods

*/

#include "PHP.h"

XS(PHP_Array_new)
{
	dXSARGS;
	STRLEN na;
	zval * array;
	
	if ( items != 1)
		croak("PHP::Array::new: 1 parameter expected");

	SP -= items;

	MAKE_STD_ZVAL( array);
	array_init( array);

	XPUSHs( sv_2mortal( Entity_create( SvPV( ST(0), na), array)));
	PUTBACK;
	return;
}

XS( PHP_TieHash_EXISTS)
{
	dXSARGS;
	char * key;
	STRLEN na, klen;
	zval * array;

#define METHOD "PHP::TieHash::EXISTS"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvPV( ST(1), klen);
	DEBUG("exists 0x%x->{%s}", array, key);

	SP -= items;
	PUTBACK;

	return XSRETURN_IV( zend_hash_exists( array-> value.ht, key, klen + 1));
#undef METHOD
}

XS( PHP_TieHash_FETCH)
{
	dXSARGS;
	char * key;
	STRLEN na, klen;
	zval * array, **zobj;
	SV * retsv;

#define METHOD "PHP::TieHash::FETCH"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvPV( ST(1), klen);
	DEBUG("fetch 0x%x->{%s}", array, key);

	SP -= items;

	if ( zend_hash_find( array-> value.ht, key, klen + 1, (void**) &zobj) == FAILURE) {
		XPUSHs( &PL_sv_undef);
		PUTBACK;
		return;
	}

	if ( !( retsv = zval2sv( *zobj))) {
		warn("%s: value cannot be converted\n", METHOD);
		retsv = &PL_sv_undef;
	}
	retsv = sv_2mortal( retsv);
	XPUSHs( retsv);

#undef METHOD
	PUTBACK;

	return;
}

XS( PHP_TieHash_STORE)
{
	dXSARGS;
	char * key;
	STRLEN na, klen;
	zval * array, *zobj;
	SV * val;

#define METHOD "PHP::TieHash::STORE"
	if ( items != 3) 
		croak("%s: expect 3 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvPV( ST(1), klen);
	DEBUG("store 0x%x->{%s}=%s", array, key, SvPV( ST(2), na));

	MAKE_STD_ZVAL( zobj);
	zobj-> type = IS_NULL;
	if ( !sv2zval( val = ST(2), zobj, -1)) {
		zval_ptr_dtor( &zobj);
		croak("%s: scalar (%s) type=%d cannot be converted", 
			METHOD, SvPV( val, na), SvTYPE( val));
	}

	if ( zend_hash_update( 
		array-> value.ht, key, klen + 1,
		(void *)&zobj, sizeof(zval *), NULL
		) == FAILURE) {
		zval_ptr_dtor( &zobj);
		croak("%s: failed", METHOD);
	}

#undef METHOD
	SP -= items;
	PUTBACK;
	XSRETURN_EMPTY;
}

XS( PHP_TieHash_DELETE)
{
	dXSARGS;
	char * key;
	STRLEN na, klen;
	zval * array;

#define METHOD "PHP::TieHash::DELETE"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvPV( ST(1), klen);
	DEBUG("delete 0x%x->{%s}", array, key);

	SP -= items;
	PUTBACK;

	zend_hash_del( array-> value.ht, key, klen + 1);

	XSRETURN_EMPTY;
#undef METHOD
}

XS( PHP_TieHash_CLEAR)
{
	dXSARGS;
	STRLEN na;
	zval * array;

#define METHOD "PHP::TieHash::CLEAR"
	if ( items != 1) 
		croak("%s: expect 1 parameter", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	DEBUG("clear 0x%x->{%s}", array);

	SP -= items;
	PUTBACK;

	zend_hash_clean( array-> value.ht);

	XSRETURN_EMPTY;
#undef METHOD
}

/* for internal use by FIRSTKEY and NEXTKEY - construct return value and advance zhash ptr  */
static SV *
do_zenum( 
	char * method,
	zval * array,
	HashPosition * hpos
) {
	SV * ret;
	int rettype, klen;
	unsigned long numkey;
	char * key;

	if ( ( rettype = zend_hash_get_current_key_ex( array-> value.ht, 
		&key, &klen, &numkey, 0, hpos) == HASH_KEY_NON_EXISTANT)) {
		DEBUG( "%s: enum stop", method);
		return &PL_sv_undef;
	}
	
	if ( rettype == HASH_KEY_IS_STRING) {
		ret = newSVpvn( key, klen); 
		DEBUG( "%s: enum %s", method, key);
	} else {
		ret = newSViv( numkey); 
		DEBUG( "%s: enum index %d", method, numkey);
	}

	return sv_2mortal( ret);
}

XS( PHP_TieHash_FIRSTKEY)
{
	dXSARGS;
	zval * array;
	STRLEN na;
	SV * hash_position, * perl_obj;
	HashPosition hpos_buf, *hpos;

#define METHOD "PHP::TieHash::FIRSTKEY"
	if ( items != 1) 
		croak("%s: expect 1 parameter", METHOD);

	if (( array = SV2ZARRAY( perl_obj = ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV( perl_obj, na));

	DEBUG("firstkey 0x%x", array);

	hash_position = newSV( sizeof( HashPosition));
        sv_setpvn( hash_position, ( char *) &hpos_buf, sizeof( hpos_buf));
	hpos = ( HashPosition*) SvPV( hash_position, na);
	hv_store((HV *) SvRV( perl_obj), "__ENUM__", 8, hash_position, 0);

	zend_hash_internal_pointer_reset_ex( array-> value.ht, hpos); 

	SP -= items;

	XPUSHs( do_zenum( METHOD, array, hpos));
	PUTBACK;

#undef METHOD
	return;
}

XS( PHP_TieHash_NEXTKEY)
{
	dXSARGS;
	zval * array;
	STRLEN na;
	SV ** hash_position, * perl_obj;
	HashPosition *hpos;

#define METHOD "PHP::TieHash::NEXTKEY"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( perl_obj = ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV( perl_obj, na));

	DEBUG("nextkey 0x%x", array);

	if ( !( hash_position = hv_fetch(( HV *) SvRV( perl_obj), "__ENUM__", 8, 0)))
		croak("%s: Internal inconsistency", METHOD);
	hpos = ( HashPosition*) SvPV( *hash_position, na);
	
	zend_hash_move_forward_ex( array-> value.ht, hpos);
	
	SP -= items;
	XPUSHs( do_zenum( METHOD, array, hpos));
	PUTBACK;
		
#undef METHOD
	return;
}

XS( PHP_TieArray_EXISTS)
{
	dXSARGS;
	long key;
	STRLEN na;
	zval * array;

#define METHOD "PHP::TieArray::EXISTS"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvIV( ST(1));
	DEBUG("exists 0x%x->[%d]", array, key);

	SP -= items;
	PUTBACK;

	return XSRETURN_IV( zend_hash_index_exists( array-> value.ht, key));
#undef METHOD
}

XS( PHP_TieArray_FETCH)
{
	dXSARGS;
	long key;
	STRLEN na;
	zval * array, **zobj;
	SV * retsv;

#define METHOD "PHP::TieArray::FETCH"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvIV( ST(1));
	DEBUG("fetch 0x%x->[%d]", array, key);

	SP -= items;

	if ( zend_hash_index_find( array-> value.ht, key, (void**) &zobj) == FAILURE) {
		XPUSHs( &PL_sv_undef);
		PUTBACK;
		return;
	}

	if ( !( retsv = zval2sv( *zobj))) {
		warn("%s: value cannot be converted\n", METHOD);
		retsv = &PL_sv_undef;
	}
	if ( retsv != &PL_sv_undef) 
		retsv = sv_2mortal( retsv);
	XPUSHs( retsv);

#undef METHOD
	PUTBACK;

	return;
}

XS( PHP_TieArray_STORE)
{
	dXSARGS;
	long key;
	STRLEN na;
	zval * array, *zobj;
	SV * val;

#define METHOD "PHP::TieArray::STORE"
	if ( items != 3) 
		croak("%s: expect 3 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvIV( ST(1));
	DEBUG("store 0x%x->[%d]=%s", array, key, SvPV( ST(2), na));

	MAKE_STD_ZVAL( zobj);
	zobj-> type = IS_NULL;
	if ( !sv2zval( val = ST(2), zobj, -1)) {
		zval_ptr_dtor( &zobj);
		croak("%s: scalar (%s) type=%d cannot be converted", 
			METHOD, SvPV( val, na), SvTYPE( val));
	}

	if ( zend_hash_index_update( 
		array-> value.ht, key,
		(void *)&zobj, sizeof(zval *), NULL
		) == FAILURE) {
		zval_ptr_dtor( &zobj);
		croak("%s: failed", METHOD);
	}

#undef METHOD
	SP -= items;
	PUTBACK;
	XSRETURN_EMPTY;
}

XS( PHP_TieArray_DELETE)
{
	dXSARGS;
	long key;
	STRLEN na;
	zval * array;

#define METHOD "PHP::TieArray::DELETE"
	if ( items != 2) 
		croak("%s: expect 2 parameters", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	key = SvIV( ST(1));
	DEBUG("delete 0x%x->[%d]", array, key);

	SP -= items;
	PUTBACK;

	zend_hash_index_del( array-> value.ht, key);

	XSRETURN_EMPTY;
#undef METHOD
}

XS( PHP_TieArray_FETCHSIZE)
{
	dXSARGS;
	STRLEN na;
	zval * array;

#define METHOD "PHP::TieArray::FETCHSIZE"
	if ( items != 1) 
		croak("%s: expect 1 parameter", METHOD);

	if (( array = SV2ZARRAY( ST(0))) == NULL)
		croak("%s: (%s) is not a PHP array", METHOD, SvPV(ST(0), na));

	DEBUG("fetchsize 0x%x->{%s}", array);

	SP -= items;
	PUTBACK;

	XSRETURN_IV( zend_hash_num_elements( array-> value.ht));
#undef METHOD
}

void
register_PHP_Array()
{
	newXS( "PHP::Array::new", PHP_Array_new, "PHP::Array");

	newXS( "PHP::TieHash::EXISTS",	PHP_TieHash_EXISTS,	"PHP::TieHash");
	newXS( "PHP::TieHash::FETCH",	PHP_TieHash_FETCH,	"PHP::TieHash");
	newXS( "PHP::TieHash::STORE",	PHP_TieHash_STORE,	"PHP::TieHash");
	newXS( "PHP::TieHash::DELETE",	PHP_TieHash_DELETE,	"PHP::TieHash");
	newXS( "PHP::TieHash::CLEAR",	PHP_TieHash_CLEAR,	"PHP::TieHash");
	newXS( "PHP::TieHash::FIRSTKEY",PHP_TieHash_FIRSTKEY,	"PHP::TieHash");
	newXS( "PHP::TieHash::NEXTKEY",	PHP_TieHash_NEXTKEY,	"PHP::TieHash");

	newXS( "PHP::TieArray::FETCHSIZE",PHP_TieArray_FETCHSIZE,"PHP::TieArray");
	newXS( "PHP::TieArray::EXISTS",	PHP_TieArray_EXISTS,	"PHP::TieArray");
	newXS( "PHP::TieArray::FETCH",	PHP_TieArray_FETCH,	"PHP::TieArray");
	newXS( "PHP::TieArray::STORE",	PHP_TieArray_STORE,	"PHP::TieArray");
	newXS( "PHP::TieArray::DELETE",	PHP_TieArray_DELETE,	"PHP::TieArray");
	newXS( "PHP::TieArray::CLEAR",	PHP_TieHash_CLEAR,	"PHP::TieArray");
}
