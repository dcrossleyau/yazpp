/*
 * Copyright (c) 1998-2000, Index Data.
 * See the file LICENSE for details.
 * 
 * $Id: yaz-proxy.h,v 1.3 2001-03-26 14:43:49 adam Exp $
 */

#include <yaz++/yaz-z-assoc.h>
#include <yaz++/yaz-z-query.h>

class Yaz_Proxy;

/// Private class
class YAZ_EXPORT Yaz_ProxyClient : public Yaz_Z_Assoc {
    friend Yaz_Proxy;
    Yaz_ProxyClient(IYaz_PDU_Observable *the_PDU_Observable);
    ~Yaz_ProxyClient();
    void recv_Z_PDU(Z_APDU *apdu);
    IYaz_PDU_Observer* sessionNotify
	(IYaz_PDU_Observable *the_PDU_Observable, int fd);
    void shutdown();
    Yaz_Proxy *m_server;
    void failNotify();
    void timeoutNotify();
    void connectNotify();
    char m_cookie[32];
    Yaz_ProxyClient *m_next;
    Yaz_ProxyClient **m_prev;
    int m_init_flag;
    Yaz_Z_Query *m_last_query;
    int m_last_resultCount;
    int m_sr_transform;
    int m_seqno;
    int m_waiting;
};

/// Information Retrieval Proxy Server.
class YAZ_EXPORT Yaz_Proxy : public Yaz_Z_Assoc {
 private:
    char *get_cookie(Z_OtherInformation **otherInfo);
    char *get_proxy(Z_OtherInformation **otherInfo);
    Yaz_ProxyClient *get_client(Z_APDU *apdu);
    Z_APDU *result_set_optimize(Z_APDU *apdu);
    void shutdown();
    
    Yaz_ProxyClient *m_client;
    IYaz_PDU_Observable *m_PDU_Observable;
    Yaz_ProxyClient *m_clientPool;
    Yaz_Proxy *m_parent;
    int m_seqno;
    int m_max_clients;
    int m_keepalive;
    char *m_proxyTarget;
    long m_seed;
 public:
    Yaz_Proxy(IYaz_PDU_Observable *the_PDU_Observable);
    ~Yaz_Proxy();
    void recv_Z_PDU(Z_APDU *apdu);
    IYaz_PDU_Observer* sessionNotify
	(IYaz_PDU_Observable *the_PDU_Observable, int fd);
    void failNotify();
    void timeoutNotify();
    void connectNotify();
    void set_proxyTarget(const char *target);
    char *get_proxyTarget() { return m_proxyTarget; };
    void set_max_clients(int m) { m_max_clients = m; };
};

