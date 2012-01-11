/* This file is part of the yazpp toolkit.
 * Copyright (C) 1998-2012 Index Data and Mike Taylor
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Index Data nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef YAZ_SOCKET_MANAGER_INCLUDED
#define YAZ_SOCKET_MANAGER_INCLUDED

#include <yazpp/socket-observer.h>
#include <time.h>

struct yaz_poll_fd;
namespace yazpp_1 {

/** Simple Socket Manager.
    Implements a stand-alone simple model that uses select(2) to
    observe socket events.
*/
class YAZ_EXPORT SocketManager : public ISocketObservable {
 private:
    struct SocketEntry {
        ISocketObserver *observer;
        int fd;
        unsigned mask;
        int timeout;
        int timeout_this;
        time_t last_activity;
        SocketEntry *next;
    };
    SocketEntry *m_observers;       // all registered observers
    struct SocketEvent {
        ISocketObserver *observer;
        int event;
        SocketEvent *next;          // front in queue
        SocketEvent *prev;          // back in queue
    };
    SocketEvent *m_queue_front;
    SocketEvent *m_queue_back;
    
    SocketEntry **lookupObserver
        (ISocketObserver *observer);
    SocketEvent *getEvent();
    void putEvent(SocketEvent *event);
    void removeEvent(ISocketObserver *observer);
    int m_log;
    void inspect_poll_result(int res, struct yaz_poll_fd *fds, int no_fds,
                             int timeout);
 public:
    /// Add an observer
    virtual void addObserver(int fd, ISocketObserver *observer);
    /// Delete an observer
    virtual void deleteObserver(ISocketObserver *observer);
    /// Delete all observers
    virtual void deleteObservers();
    /// Set event mask for observer
    virtual void maskObserver(ISocketObserver *observer, int mask);
    /// Set timeout
    virtual void timeoutObserver(ISocketObserver *observer,
                                 int timeout);
    /// Process one event. return > 0 if event could be processed;
    int processEvent();
    SocketManager();
    virtual ~SocketManager();
};

};

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

