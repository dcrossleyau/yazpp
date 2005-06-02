/*
 * Copyright (c) 1998-2004, Index Data.
 * See the file LICENSE for details.
 * 
 * $Id: socket-manager.h,v 1.4 2005-06-02 06:40:21 adam Exp $
 */

#ifndef YAZ_SOCKET_MANAGER_INCLUDED
#define YAZ_SOCKET_MANAGER_INCLUDED

#include <yaz++/socket-observer.h>
#include <time.h>

namespace yazpp_1 {

/** Simple Socket Manager.
    Implements a stand-alone simple model that uses select(2) to
    observe socket events.
*/
class YAZ_EXPORT Yaz_SocketManager : public IYazSocketObservable {
 private:
    struct YazSocketEntry {
	IYazSocketObserver *observer;
	int fd;
	unsigned mask;
	int timeout;
        int timeout_this;
	time_t last_activity;
	YazSocketEntry *next;
    };
    YazSocketEntry *m_observers;       // all registered observers
    struct YazSocketEvent {
	IYazSocketObserver *observer;
	int event;
	YazSocketEvent *next;          // front in queue
	YazSocketEvent *prev;          // back in queue
    };
    YazSocketEvent *m_queue_front;
    YazSocketEvent *m_queue_back;
    
    YazSocketEntry **Yaz_SocketManager::lookupObserver
	(IYazSocketObserver *observer);
    YazSocketEvent *Yaz_SocketManager::getEvent();
    void putEvent(YazSocketEvent *event);
    void removeEvent(IYazSocketObserver *observer);
    int m_log;
 public:
    /// Add an observer
    virtual void addObserver(int fd, IYazSocketObserver *observer);
    /// Delete an observer
    virtual void deleteObserver(IYazSocketObserver *observer);
    /// Delete all observers
    virtual void deleteObservers();
    /// Set event mask for observer
    virtual void maskObserver(IYazSocketObserver *observer, int mask);
    /// Set timeout
    virtual void timeoutObserver(IYazSocketObserver *observer,
				 int timeout);
    /// Process one event. return > 0 if event could be processed;
    int processEvent();
    Yaz_SocketManager();
    virtual ~Yaz_SocketManager();
};

};

#endif
