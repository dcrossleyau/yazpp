/*
 * Copyright (c) 1998-1999, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 * 
 * $Id: yaz-proxy.h,v 1.6 1999-04-27 07:52:13 adam Exp $
 */

#include <yaz-z-assoc.h>
#include <yaz-z-query.h>

class Yaz_Proxy;

/// Private class
class YAZ_EXPORT Yaz_ProxyClient : public Yaz_Z_Assoc {
    friend Yaz_Proxy;
    Yaz_ProxyClient(IYaz_PDU_Observable *the_PDU_Observable);
    ~Yaz_ProxyClient();
    void recv_Z_PDU(Z_APDU *apdu);
    IYaz_PDU_Observer* clone(IYaz_PDU_Observable *the_PDU_Observable);
    Yaz_Proxy *m_server;
    void failNotify();
    void timeoutNotify();
    char m_cookie[32];
    Yaz_ProxyClient *m_next;
    Yaz_ProxyClient **m_prev;
    int m_init_flag;
    Yaz_Z_Query *m_last_query;
    int m_last_resultCount;
};

/// Information Retrieval Proxy Server.
class YAZ_EXPORT Yaz_Proxy : public Yaz_Z_Assoc {
 public:
    Yaz_Proxy(IYaz_PDU_Observable *the_PDU_Observable);
    ~Yaz_Proxy();
    void recv_Z_PDU(Z_APDU *apdu);
    IYaz_PDU_Observer* clone(IYaz_PDU_Observable *the_PDU_Observable);
    void failNotify();
    void timeoutNotify();
 private:
    char *get_cookie(Z_OtherInformation **otherInfo);
    char *get_proxy(Z_OtherInformation **otherInfo);
    Yaz_ProxyClient *get_client(Z_APDU *apdu);
    Z_APDU *result_set_optimize(Z_APDU *apdu);
    
    Yaz_ProxyClient *m_client;
    IYaz_PDU_Observable *m_PDU_Observable;
    Yaz_ProxyClient *m_clientPool;
    Yaz_Proxy *m_parent;
    int m_seqno;
    int m_keepalive;
};

