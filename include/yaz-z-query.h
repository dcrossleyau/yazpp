/*
 * Copyright (c) 1998-1999, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 * 
 * $Log: yaz-z-query.h,v $
 * Revision 1.2  1999-04-09 11:47:23  adam
 * Added object Yaz_Z_Assoc. Much more functional client.
 *
 * Revision 1.1  1999/03/23 14:17:57  adam
 * More work on timeout handling. Work on yaz-client.
 *
 */

#include <proto.h>
#include <yaz-query.h>

/** Z39.50 Query
    RPN, etc.
*/
class YAZ_EXPORT Yaz_Z_Query : public Yaz_Query {
 public:
    /// Make Query from rpn string
    Yaz_Z_Query();
    /// Delete Query
    virtual ~Yaz_Z_Query();
    /// Set RPN
    int set_rpn (const char *rpn);
    /// Set Z Query
    void set_Z_Query (Z_Query *z_query);
    /// Get Z Query
    Z_Query *get_Z_Query ();
    /// print query
    void print(char *str, int len);
 private:
    char *buf;
    int len;
    ODR odr_decode;
    ODR odr_encode;
};
