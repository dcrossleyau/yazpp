/*
 * Copyright (c) 1998-2003, Index Data.
 * See the file LICENSE for details.
 * 
 * $Id: yaz-proxy.cpp,v 1.41 2003-06-11 21:59:07 adam Exp $
 */

#include <assert.h>
#include <time.h>

#include <yaz/log.h>
#include <yaz++/proxy.h>

Yaz_Proxy::Yaz_Proxy(IYaz_PDU_Observable *the_PDU_Observable) :
    Yaz_Z_Assoc(the_PDU_Observable)
{
    m_PDU_Observable = the_PDU_Observable;
    m_client = 0;
    m_parent = 0;
    m_clientPool = 0;
    m_seqno = 1;
    m_keepalive = 1;
    m_proxyTarget = 0;
    m_proxy_authentication = 0;
    m_max_clients = 50;
    m_seed = time(0);
    m_idletime = 600;
    m_optimize = xstrdup ("1");
}

Yaz_Proxy::~Yaz_Proxy()
{
    xfree (m_proxyTarget);
    xfree (m_proxy_authentication);
    xfree (m_optimize);
}

void Yaz_Proxy::set_proxy_target(const char *target)
{
    xfree (m_proxyTarget);
    m_proxyTarget = 0;
    if (target)
	m_proxyTarget = (char *) xstrdup (target);
}

void Yaz_Proxy::set_proxy_authentication (const char *auth)
{
    xfree (m_proxy_authentication);
    m_proxy_authentication = 0;
    if (auth)
	m_proxy_authentication = (char *) xstrdup (auth);
}

IYaz_PDU_Observer *Yaz_Proxy::sessionNotify(IYaz_PDU_Observable
					    *the_PDU_Observable, int fd)
{
    Yaz_Proxy *new_proxy = new Yaz_Proxy(the_PDU_Observable);
    new_proxy->m_parent = this;
    new_proxy->timeout(m_idletime);
    new_proxy->set_proxy_target(m_proxyTarget);
    new_proxy->set_APDU_log(get_APDU_log());
    new_proxy->set_proxy_authentication(m_proxy_authentication);
    yaz_log (LOG_LOG, "New session p=%p", new_proxy);
    return new_proxy;
}

char *Yaz_Proxy::get_cookie(Z_OtherInformation **otherInfo)
{
    int oid[OID_SIZE];
    Z_OtherInformationUnit *oi;
    struct oident ent;
    ent.proto = PROTO_Z3950;
    ent.oclass = CLASS_USERINFO;
    ent.value = (oid_value) VAL_COOKIE;
    assert (oid_ent_to_oid (&ent, oid));

    if (oid_ent_to_oid (&ent, oid) && 
	(oi = update_otherInformation(otherInfo, 0, oid, 1, 1)) &&
	oi->which == Z_OtherInfo_characterInfo)
	return oi->information.characterInfo;
    return 0;
}

char *Yaz_Proxy::get_proxy(Z_OtherInformation **otherInfo)
{
    int oid[OID_SIZE];
    Z_OtherInformationUnit *oi;
    struct oident ent;
    ent.proto = PROTO_Z3950;
    ent.oclass = CLASS_USERINFO;
    ent.value = (oid_value) VAL_PROXY;
    if (oid_ent_to_oid (&ent, oid) &&
	(oi = update_otherInformation(otherInfo, 0, oid, 1, 1)) &&
	oi->which == Z_OtherInfo_characterInfo)
	return oi->information.characterInfo;
    return 0;
}

Yaz_ProxyClient *Yaz_Proxy::get_client(Z_APDU *apdu)
{
    assert (m_parent);
    Yaz_Proxy *parent = m_parent;
    Z_OtherInformation **oi;
    Yaz_ProxyClient *c = m_client;
    
    get_otherInfoAPDU(apdu, &oi);
    char *cookie = get_cookie(oi);

    const char *proxy_host = get_proxy(oi);
    if (proxy_host)
	set_proxy_target(proxy_host);
    
    // no target specified at all?
    if (!m_proxyTarget)
	return 0;

    if (!strcmp(m_proxyTarget, "stop"))
	exit (0);
    if (cookie && *cookie)
    {
	Yaz_ProxyClient *cc = 0;
	
	for (c = parent->m_clientPool; c; c = c->m_next)
	{
	    assert (c->m_prev);
	    assert (*c->m_prev == c);
	    if (!strcmp(cookie,c->m_cookie) &&
		!strcmp(m_proxyTarget, c->get_hostname()))
	    {
		cc = c;
	    }
	}
	if (cc)
	{
	    // found it in cache
	    c = cc;
	    // The following handles "cancel"
	    // If connection is busy (waiting for PDU) and
	    // we have an initRequest we can safely do re-open
	    if (c->m_waiting && apdu->which == Z_APDU_initRequest)
	    {
		yaz_log (LOG_LOG, "reopen target=%s", c->get_hostname());
		c->close();
		c->client(m_proxyTarget);
		c->m_init_flag = 0;

		c->m_last_ok = 0;
		c->m_last_resultCount = 0;
		c->m_sr_transform = 0;
		c->m_waiting = 0;
		c->timeout(m_idletime); 
	    }
	    c->m_seqno = parent->m_seqno;
	    if (c->m_server && c->m_server != this)
		c->m_server->m_client = 0;
	    c->m_server = this;
	    (parent->m_seqno)++;
	    yaz_log (LOG_DEBUG, "get_client 1 %p %p", this, c);
	    return c;
	}
    }
    else if (!c)
    {
	Yaz_ProxyClient *cc = 0;
	
	for (c = parent->m_clientPool; c; c = c->m_next)
	{
	    assert (c->m_prev);
	    assert (*c->m_prev == c);
	    if (c->m_server == 0 && c->m_cookie[0] == 0 && 
		!strcmp(m_proxyTarget, c->get_hostname()))
	    {
		cc = c;
	    }
	}
	if (cc)
	{
	    // found it in cache
	    c = cc;

	    yaz_log (LOG_LOG, "Reuse session %d to %d %s",
		     c->m_seqno, parent->m_seqno, c->get_hostname());

	    c->m_seqno = parent->m_seqno;
	    assert(c->m_server == 0);
	    c->m_server = this;
	    
	    (parent->m_seqno)++;
	    return c;
	}
    }
    if (!m_client)
    {
	if (apdu->which != Z_APDU_initRequest)
	{
	    yaz_log (LOG_LOG, "no first INIT!");
	    return 0;
	}
        Z_InitRequest *initRequest = apdu->u.initRequest;

        if (!initRequest->idAuthentication)
        {
            if (m_proxy_authentication)
            {
                initRequest->idAuthentication =
                    (Z_IdAuthentication *)
                    odr_malloc (odr_encode(),
                                sizeof(*initRequest->idAuthentication));
                initRequest->idAuthentication->which =
                    Z_IdAuthentication_open;
                initRequest->idAuthentication->u.open =
                    odr_strdup (odr_encode(), m_proxy_authentication);
            }
        }

	// go through list of clients - and find the lowest/oldest one.
	Yaz_ProxyClient *c_min = 0;
	int min_seq = -1;
	int no_of_clients = 0;
	if (parent->m_clientPool)
	    yaz_log (LOG_LOG, "Existing sessions");
	for (c = parent->m_clientPool; c; c = c->m_next)
	{
	    yaz_log (LOG_LOG, " Session %-3d wait=%d %s", c->m_seqno,
			       c->m_waiting, c->get_hostname());
	    no_of_clients++;
	    if (min_seq < 0 || c->m_seqno < min_seq)
	    {
		min_seq = c->m_seqno;
		c_min = c;
	    }
	}
	if (no_of_clients >= parent->m_max_clients)
	{
	    c = c_min;
	    if (c->m_waiting || strcmp(m_proxyTarget, c->get_hostname()))
	    {
		yaz_log (LOG_LOG, "Replace session %d",
		      c->m_seqno);
		if (c->m_server && c->m_server != this)
		    delete c->m_server;
		c->m_server = 0;
	    }
	    else
	    {
		yaz_log (LOG_LOG, "Move session %d to %d %s",
		      c->m_seqno, parent->m_seqno, c->get_hostname());
		if (cookie)
		    strcpy (c->m_cookie, cookie);
		else
		    c->m_cookie[0] = '\0';
		c->m_seqno = parent->m_seqno;
		if (c->m_server && c->m_server != this)
		{
		    c->m_server->m_client = 0;
		    delete c->m_server;
		}
		(parent->m_seqno)++;
		yaz_log (LOG_DEBUG, "get_client 2 %p %p", this, c);
		return c;
	    }
	}
	else
	{
	    yaz_log (LOG_LOG, "Making session %d %s", parent->m_seqno,
			    m_proxyTarget);
	    c = new Yaz_ProxyClient(m_PDU_Observable->clone());
	    c->m_next = parent->m_clientPool;
	    if (c->m_next)
		c->m_next->m_prev = &c->m_next;
	    parent->m_clientPool = c;
	    c->m_prev = &parent->m_clientPool;
	}
	if (cookie)
	    strcpy (c->m_cookie, cookie);
	else
	    c->m_cookie[0] = '\0';
	yaz_log (LOG_LOG, "Connecting to %s", m_proxyTarget);
	c->m_seqno = parent->m_seqno;
	c->client(m_proxyTarget);
	c->m_init_flag = 0;
	c->m_last_resultCount = 0;
        c->m_last_ok = 0;
	c->m_sr_transform = 0;
	c->m_waiting = 0;
	c->timeout(20);

	(parent->m_seqno)++;
    }
    yaz_log (LOG_DEBUG, "get_client 3 %p %p", this, c);
    return c;
}

Z_APDU *Yaz_Proxy::result_set_optimize(Z_APDU *apdu)
{
    if (apdu->which != Z_APDU_searchRequest)
	return apdu;
    if (*m_parent->m_optimize == '0')
        return apdu;      // don't optimize result sets..
    Z_SearchRequest *sr = apdu->u.searchRequest;
    Yaz_Z_Query *this_query = new Yaz_Z_Query;
    Yaz_Z_Databases this_databases;

    this_databases.set(sr->num_databaseNames, (const char **)
                       sr->databaseNames);
    
    this_query->set_Z_Query(sr->query);
    
    if (m_client->m_last_ok && m_client->m_last_query &&
	m_client->m_last_query->match(this_query) &&
        !strcmp(m_client->m_last_resultSetId, sr->resultSetName) &&
        m_client->m_last_databases.match(this_databases))
    {
	delete this_query;
	if (m_client->m_last_resultCount > *sr->smallSetUpperBound &&
	    m_client->m_last_resultCount < *sr->largeSetLowerBound)
	{
	    // medium Set
            // send present request (medium size)
	    yaz_log (LOG_LOG, "Optimizing search for medium set");
	    Z_APDU *new_apdu = create_Z_PDU(Z_APDU_presentRequest);
	    Z_PresentRequest *pr = new_apdu->u.presentRequest;
	    pr->referenceId = sr->referenceId;
	    pr->resultSetId = sr->resultSetName;
	    pr->preferredRecordSyntax = sr->preferredRecordSyntax;
            if (*sr->mediumSetPresentNumber < m_client->m_last_resultCount)
                *pr->numberOfRecordsRequested = *sr->mediumSetPresentNumber;
            else
                *pr->numberOfRecordsRequested = m_client->m_last_resultCount;
	    if (sr->mediumSetElementSetNames)
	    {
		pr->recordComposition = (Z_RecordComposition *)
		    odr_malloc(odr_encode(), sizeof(Z_RecordComposition));
		pr->recordComposition->which = Z_RecordComp_simple;
		pr->recordComposition->u.simple = sr->mediumSetElementSetNames;
	    }
	    m_client->m_sr_transform = 1;
	    return new_apdu;
	}
	else if (m_client->m_last_resultCount >= *sr->largeSetLowerBound ||
	    m_client->m_last_resultCount <= 0)
	{
            // large set. Return pseudo-search response immediately
	    yaz_log (LOG_LOG, "Optimizing search for large set");
	    Z_APDU *new_apdu = create_Z_PDU(Z_APDU_searchResponse);
	    new_apdu->u.searchResponse->referenceId = sr->referenceId;
	    new_apdu->u.searchResponse->resultCount =
		&m_client->m_last_resultCount;
	    send_Z_PDU(new_apdu);
	    return 0;
	}
	else
	{
	    // small set
            // send a present request (small set)
	    yaz_log (LOG_LOG, "Optimizing search for small set");
	    Z_APDU *new_apdu = create_Z_PDU(Z_APDU_presentRequest);
	    Z_PresentRequest *pr = new_apdu->u.presentRequest;
	    pr->referenceId = sr->referenceId;
	    pr->resultSetId = sr->resultSetName;
	    pr->preferredRecordSyntax = sr->preferredRecordSyntax;
	    *pr->numberOfRecordsRequested = m_client->m_last_resultCount;
	    if (sr->smallSetElementSetNames)
	    {
		pr->recordComposition = (Z_RecordComposition *)
		    odr_malloc(odr_encode(), sizeof(Z_RecordComposition));
		pr->recordComposition->which = Z_RecordComp_simple;
		pr->recordComposition->u.simple = sr->smallSetElementSetNames;
	    }
	    m_client->m_sr_transform = 1;
	    return new_apdu;
	}
    }
    else
    {
	delete m_client->m_last_query;
	m_client->m_last_query = this_query;
        m_client->m_last_ok = 0;

        xfree (m_client->m_last_resultSetId);
        m_client->m_last_resultSetId = xstrdup (sr->resultSetName);

        m_client->m_last_databases.set(sr->num_databaseNames,
                                       (const char **) sr->databaseNames);
    }
    return apdu;
}

static const char *apdu_name(Z_APDU *apdu)
{
    switch (apdu->which)
    {
    case Z_APDU_initRequest:
        return "initRequest";
    case Z_APDU_initResponse:
        return "initResponse";
    case Z_APDU_searchRequest:
	return "searchRequest";
    case Z_APDU_searchResponse:
	return "searchResponse";
    case Z_APDU_presentRequest:
	return "presentRequest";
    case Z_APDU_presentResponse:
	return "presentResponse";
    case Z_APDU_deleteResultSetRequest:
	return "deleteResultSetRequest";
    case Z_APDU_deleteResultSetResponse:
	return "deleteResultSetResponse";
    case Z_APDU_scanRequest:
	return "scanRequest";
    case Z_APDU_scanResponse:
	return "scanResponse";
    case Z_APDU_sortRequest:
	return "sortRequest";
    case Z_APDU_sortResponse:
	return "sortResponse";
    case Z_APDU_extendedServicesRequest:
	return "extendedServicesRequest";
    case Z_APDU_extendedServicesResponse:
	return "extendedServicesResponse";
    case Z_APDU_close:
	return "close";
    }
    return "other";
}

void Yaz_Proxy::recv_Z_PDU(Z_APDU *apdu)
{
    yaz_log (LOG_LOG, "Receiving %s from client", apdu_name(apdu));
    // Determine our client.
    m_client = get_client(apdu);
    if (!m_client)
    {
	delete this;
	return;
    }
    m_client->m_server = this;

    if (apdu->which == Z_APDU_initRequest)
    {
	if (m_client->m_init_flag)
	{
	    Z_APDU *apdu = m_client->m_initResponse;
	    apdu->u.initResponse->otherInfo = 0;
	    if (m_client->m_cookie)
		set_otherInformationString(apdu, VAL_COOKIE, 1,
					   m_client->m_cookie);
	    send_Z_PDU(apdu);
	    return;
	}
	m_client->m_init_flag = 1;
    }
    apdu = result_set_optimize(apdu);
    if (!apdu)
	return;

    yaz_log (LOG_LOG, "Sending %s to %s",
		    apdu_name(apdu), m_client->get_hostname());

    // delete other info part from PDU before sending to target
    Z_OtherInformation **oi;
    get_otherInfoAPDU(apdu, &oi);
    if (oi)
        *oi = 0;

    if (m_client->send_Z_PDU(apdu) < 0)
    {
	delete m_client;
	m_client = 0;
	delete this;
    }
    else
	m_client->m_waiting = 1;
}

void Yaz_Proxy::connectNotify()
{
}

void Yaz_Proxy::shutdown()
{
    // only keep if keep_alive flag is set...
    if (m_keepalive && m_client && m_client->m_waiting == 0)
    {
        yaz_log (LOG_LOG, "shutdown (client to proxy) keepalive %s",
                 m_client->get_hostname());
        assert (m_client->m_waiting != 2);
	// Tell client (if any) that no server connection is there..
	m_client->m_server = 0;
    }
    else if (m_client)
    {
        yaz_log (LOG_LOG, "shutdown (client to proxy) close %s",
                 m_client->get_hostname());
        assert (m_client->m_waiting != 2);
	delete m_client;
    }
    else if (!m_parent)
    {
        yaz_log (LOG_LOG, "shutdown (client to proxy) bad state");
        assert (m_parent);
    }
    else 
    {
        yaz_log (LOG_LOG, "shutdown (client to proxy)");
    }
    delete this;
}

void Yaz_ProxyClient::shutdown()
{
    yaz_log (LOG_LOG, "shutdown (proxy to server) %s", get_hostname());
    delete m_server;
    delete this;
}

void Yaz_Proxy::failNotify()
{
    yaz_log (LOG_LOG, "Yaz_Proxy connection closed by client");
    shutdown();
}

void Yaz_ProxyClient::failNotify()
{
    yaz_log (LOG_LOG, "Yaz_ProxyClient connection closed by %s", get_hostname());
    shutdown();
}

void Yaz_ProxyClient::connectNotify()
{
    yaz_log (LOG_LOG, "Connection accepted by %s", get_hostname());
    timeout(600);
}

IYaz_PDU_Observer *Yaz_ProxyClient::sessionNotify(IYaz_PDU_Observable
						  *the_PDU_Observable, int fd)
{
    return new Yaz_ProxyClient(the_PDU_Observable);
}

Yaz_ProxyClient::~Yaz_ProxyClient()
{
    if (m_prev)
	*m_prev = m_next;
    if (m_next)
	m_next->m_prev = m_prev;
    m_waiting = 2;     // for debugging purposes only.
    odr_destroy(m_init_odr);
    delete m_last_query;
    xfree (m_last_resultSetId);
}

void Yaz_Proxy::timeoutNotify()
{
    yaz_log (LOG_LOG, "timeout (client to proxy)");
    shutdown();
}

void Yaz_ProxyClient::timeoutNotify()
{
    yaz_log (LOG_LOG, "timeout (proxy to target) %s", get_hostname());
    shutdown();
}

Yaz_ProxyClient::Yaz_ProxyClient(IYaz_PDU_Observable *the_PDU_Observable) :
    Yaz_Z_Assoc (the_PDU_Observable)
{
    m_cookie[0] = 0;
    m_next = 0;
    m_prev = 0;
    m_init_flag = 0;
    m_last_query = 0;
    m_last_resultSetId = 0;
    m_last_resultCount = 0;
    m_last_ok = 0;
    m_sr_transform = 0;
    m_waiting = 0;
    m_init_odr = odr_createmem (ODR_DECODE);
    m_initResponse = 0;
}

const char *Yaz_Proxy::option(const char *name, const char *value)
{
    if (!strcmp (name, "optimize")) {
	if (value) {
            xfree (m_optimize);	
	    m_optimize = xstrdup (value);
        }
	return m_optimize;
    }
    return 0;
}

void Yaz_ProxyClient::recv_Z_PDU(Z_APDU *apdu)
{
    m_waiting = 0;
    yaz_log (LOG_LOG, "Receiving %s from %s", apdu_name(apdu),
		     get_hostname());
    if (apdu->which == Z_APDU_initResponse)
    {
        NMEM nmem = odr_extract_mem (odr_decode());
	odr_reset (m_init_odr);
        nmem_transfer (m_init_odr->mem, nmem);
        m_initResponse = apdu;
        nmem_destroy (nmem);
    }
    if (apdu->which == Z_APDU_searchResponse)
    {
	m_last_resultCount = *apdu->u.searchResponse->resultCount;
	int status = *apdu->u.searchResponse->searchStatus;
	if (status && 
		(!apdu->u.searchResponse->records ||
                 apdu->u.searchResponse->records->which == Z_Records_DBOSD))
            m_last_ok = 1;
    }
    if (apdu->which == Z_APDU_presentResponse && m_sr_transform)
    {
	m_sr_transform = 0;
	Z_PresentResponse *pr = apdu->u.presentResponse;
	Z_APDU *new_apdu = create_Z_PDU(Z_APDU_searchResponse);
	Z_SearchResponse *sr = new_apdu->u.searchResponse;
	sr->referenceId = pr->referenceId;
	*sr->resultCount = m_last_resultCount;
	sr->records = pr->records;
	sr->nextResultSetPosition = pr->nextResultSetPosition;
	sr->numberOfRecordsReturned = pr->numberOfRecordsReturned;
	apdu = new_apdu;
    }
    if (m_cookie && *m_cookie)
	set_otherInformationString (apdu, VAL_COOKIE, 1, m_cookie);
    if (m_server)
    {
	yaz_log (LOG_LOG, "Sending %s to client", apdu_name(apdu));
	m_server->send_Z_PDU(apdu);
    }
}
