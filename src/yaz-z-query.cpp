/*
 * Copyright (c) 1998-1999, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 * 
 * $Log: yaz-z-query.cpp,v $
 * Revision 1.1  1999-03-23 14:17:57  adam
 * More work on timeout handling. Work on yaz-client.
 *
 */

#include <yaz-z-query.h>
#include <pquery.h>

Yaz_Z_Query::Yaz_Z_Query()
{
    odr_encode = odr_createmem (ODR_ENCODE);
    odr_decode = odr_createmem (ODR_DECODE);
}

void Yaz_Z_Query::set_rpn (const char *rpn)
{
    buf = 0;
    odr_reset (odr_encode);
    Z_Query *query = (Z_Query*) odr_malloc (odr_encode, sizeof(*query));
    query->which = Z_Query_type_1;
    query->u.type_1 = p_query_rpn (odr_encode, PROTO_Z3950, rpn);
    if (!query->u.type_1)
	return;
    if (!z_Query (odr_encode, &query, 0))
	return;
    buf = odr_getbuf (odr_encode, &len, 0);
}

void Yaz_Z_Query::set_Z_Query(Z_Query *z_query)
{
    buf = 0;
    odr_reset (odr_encode);
    if (!z_Query (odr_encode, &z_query, 0))
	return;
    buf = odr_getbuf (odr_encode, &len, 0);
}

Yaz_Z_Query::~Yaz_Z_Query()
{
    odr_destroy (odr_encode);
    odr_destroy (odr_decode);
}

Z_Query *Yaz_Z_Query::get_Z_Query ()
{
    Z_Query *query;
    if (!buf)
	return 0;
    odr_setbuf (odr_decode, buf, len, 0);
    if (!z_Query(odr_decode, &query, 0))
	return 0;
    return query;
}

void Yaz_Z_Query::print(char *str, int len)
{

}
