/* This file is part of the yazpp toolkit.
 * Copyright (C) 1998-2012 Index Data and Mike Taylor
 * See the file LICENSE for details.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <yaz/log.h>

#include <yazpp/socket-manager.h>
#include <yaz/poll.h>

using namespace yazpp_1;

SocketManager::SocketEntry **SocketManager::lookupObserver(
    ISocketObserver *observer)
{
    SocketEntry **se;
    
    for (se = &m_observers; *se; se = &(*se)->next)
        if ((*se)->observer == observer)
            break;
    return se;
}

int SocketManager::getNumberOfObservers()
{
    int i = 0;
    SocketEntry *se;
    for (se = m_observers; se; se = se->next, i++)
        ;
    return i;
}

void SocketManager::addObserver(int fd, ISocketObserver *observer)
{
    SocketEntry *se;

    se = *lookupObserver(observer);
    if (!se)
    {
        se = new SocketEntry;
        se->next= m_observers;
        m_observers = se;
        se->observer = observer;
    }
    se->fd = fd;
    se->mask = 0;
    se->last_activity = 0;
    se->timeout = -1;
}

void SocketManager::deleteObserver(ISocketObserver *observer)
{
    SocketEntry **se = lookupObserver(observer);
    if (*se)
    {
        removeEvent (observer);
        SocketEntry *se_tmp = *se;
        *se = (*se)->next;
        delete se_tmp;
    }
}

void SocketManager::deleteObservers()
{
    SocketEntry *se = m_observers;
    
    while (se)
    {
        SocketEntry *se_next = se->next;
        delete se;
        se = se_next;
    }
    m_observers = 0;
}

void SocketManager::maskObserver(ISocketObserver *observer, int mask)
{
    SocketEntry *se;

    yaz_log(m_log, "obs=%p read=%d write=%d except=%d", observer,
                    mask & SOCKET_OBSERVE_READ,
                    mask & SOCKET_OBSERVE_WRITE,
                    mask & SOCKET_OBSERVE_EXCEPT);

    se = *lookupObserver(observer);
    if (se)
        se->mask = mask;
}

void SocketManager::timeoutObserver(ISocketObserver *observer,
                                        int timeout)
{
    SocketEntry *se;

    se = *lookupObserver(observer);
    if (se)
        se->timeout = timeout;
}


void SocketManager::inspect_poll_result(int res, struct yaz_poll_fd *fds,
                                        int no_fds, int timeout)

{
    yaz_log(m_log, "yaz_poll returned res=%d", res);
    time_t now = time(0);
    int i;
    int no_put_events = 0;
    SocketEntry *p;

    for (i = 0, p = m_observers; p; p = p->next, i++)
    {
        enum yaz_poll_mask output_mask = fds[i].output_mask;

        int mask = 0;
        if (output_mask & yaz_poll_read)
            mask |= SOCKET_OBSERVE_READ;

        if (output_mask & yaz_poll_write)
            mask |= SOCKET_OBSERVE_WRITE;

        if (output_mask & yaz_poll_except)
            mask |= SOCKET_OBSERVE_EXCEPT;
        
        if (mask)
        {
            SocketEvent *event = new SocketEvent;
            p->last_activity = now;
            event->observer = p->observer;
            event->event = mask;
            putEvent (event);
            no_put_events++;
            yaz_log (m_log, "putEvent I/O mask=%d", mask);
        }
        else if (res == 0 && p->timeout_this == timeout)
        {
            SocketEvent *event = new SocketEvent;
            assert (p->last_activity);
            yaz_log (m_log, "putEvent timeout fd=%d, now = %ld last_activity=%ld timeout=%d",
                     p->fd, now, p->last_activity, p->timeout);
            p->last_activity = now;
            event->observer = p->observer;
            event->event = SOCKET_OBSERVE_TIMEOUT;
            putEvent (event);
            no_put_events++;
            
        }
    }
    SocketEvent *event = getEvent();
    if (event)
    {
        event->observer->socketNotify(event->event);
        delete event;
    }
    else
    {
        // bug #2035
        
        yaz_log(YLOG_WARN, "unhandled socket event. yaz_poll returned %d", res);
        yaz_log(YLOG_WARN, "no_put_events=%d no_fds=%d i=%d timeout=%d",
                no_put_events, no_fds, i, timeout);
    }
}

int SocketManager::processEvent()
{
    SocketEntry *p;
    SocketEvent *event = getEvent();
    int timeout = -1;
    yaz_log (m_log, "SocketManager::processEvent manager=%p", this);
    if (event)
    {
        event->observer->socketNotify(event->event);
        delete event;
        return 1;
    }

    int res;
    time_t now = time(0);
    int i;
    int no_fds = 0;
    for (p = m_observers; p; p = p->next)
        no_fds++;

    if (!no_fds)
        return 0;
    struct yaz_poll_fd *fds = new yaz_poll_fd [no_fds];
    for (i = 0, p = m_observers; p; p = p->next, i++)
    {
        fds[i].fd = p->fd;
        int input_mask = 0;
        if (p->mask & SOCKET_OBSERVE_READ)
            input_mask += yaz_poll_read;
        if (p->mask & SOCKET_OBSERVE_WRITE)
            input_mask += yaz_poll_write;
        if (p->mask & SOCKET_OBSERVE_EXCEPT)
            input_mask += yaz_poll_except;
        if (p->timeout > 0 ||
            (p->timeout == 0 && (p->mask & SOCKET_OBSERVE_WRITE) == 0))
        {
            int timeout_this;
            timeout_this = p->timeout;
            if (p->last_activity)
                timeout_this -= now - p->last_activity;
            else
                p->last_activity = now;
            if (timeout_this < 0 || timeout_this > 2147483646)
                timeout_this = 0;
            if (timeout == -1 || timeout_this < timeout)
                timeout = timeout_this;
            p->timeout_this = timeout_this;
            yaz_log (m_log, "SocketManager::select timeout_this=%d", 
                     p->timeout_this);
        }
        else
            p->timeout_this = -1;
        fds[i].input_mask = (enum yaz_poll_mask) input_mask;
    }

    int pass = 0;
    while ((res = yaz_poll(fds, no_fds, timeout, 0)) < 0 && pass < 10)
    {
        if (errno == EINTR)
            continue;
        yaz_log(YLOG_ERRNO|YLOG_WARN, "yaz_poll");
        yaz_log(YLOG_WARN, "errno=%d timeout=%d", errno, timeout);
    }

    if (res >= 0)
        inspect_poll_result(res, fds, no_fds, timeout);

    delete [] fds;
    return res >= 0 ? 1 : -1;
}


//    n p    n p  ......   n p    n p
//   front                        back

void SocketManager::putEvent(SocketEvent *event)
{
    // put in back of queue
    if (m_queue_back)
    {
        m_queue_back->prev = event;
        assert (m_queue_front);
    }
    else
    {
        assert (!m_queue_front);
        m_queue_front = event;
    }
    event->next = m_queue_back;
    event->prev = 0;
    m_queue_back = event;
}

SocketManager::SocketEvent *SocketManager::getEvent()
{
    // get from front of queue
    SocketEvent *event = m_queue_front;
    if (!event)
        return 0;
    assert (m_queue_back);
    m_queue_front = event->prev;
    if (m_queue_front)
    {
        assert (m_queue_back);
        m_queue_front->next = 0;
    }
    else
        m_queue_back = 0;
    return event;
}

void SocketManager::removeEvent(ISocketObserver *observer)
{
    SocketEvent *ev = m_queue_back;
    while (ev)
    {
        SocketEvent *ev_next = ev->next;
        if (observer == ev->observer)
        {
            if (ev->prev)
                ev->prev->next = ev->next;
            else
                m_queue_back = ev->next;
            if (ev->next)
                ev->next->prev = ev->prev;
            else
                m_queue_front = ev->prev;
            delete ev;
        }
        ev = ev_next;
    }
}

SocketManager::SocketManager()
{
    m_observers = 0;
    m_queue_front = 0;
    m_queue_back = 0;
    m_log = YLOG_DEBUG;
}

SocketManager::~SocketManager()
{
    deleteObservers();
}
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

