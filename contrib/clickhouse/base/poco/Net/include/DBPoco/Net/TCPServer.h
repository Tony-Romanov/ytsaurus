//
// TCPServer.h
//
// Library: Net
// Package: TCPServer
// Module:  TCPServer
//
// Definition of the TCPServer class.
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Net_TCPServer_INCLUDED
#define DB_Net_TCPServer_INCLUDED


#include "DBPoco/AutoPtr.h"
#include "DBPoco/Net/Net.h"
#include "DBPoco/Net/ServerSocket.h"
#include "DBPoco/Net/TCPServerConnectionFactory.h"
#include "DBPoco/Net/TCPServerParams.h"
#include "DBPoco/RefCountedObject.h"
#include "DBPoco/Runnable.h"
#include "DBPoco/Thread.h"
#include "DBPoco/ThreadPool.h"

#include <atomic>


namespace DBPoco
{
namespace Net
{


    class TCPServerDispatcher;
    class StreamSocket;


    class Net_API TCPServerConnectionFilter : public DBPoco::RefCountedObject
    /// A TCPServerConnectionFilter can be used to reject incoming connections
    /// before passing them on to the TCPServerDispatcher and
    /// starting a thread to handle them.
    ///
    /// An example use case is white-list or black-list IP address filtering.
    ///
    /// Subclasses must override the accept() method.
    {
    public:
        typedef DBPoco::AutoPtr<TCPServerConnectionFilter> Ptr;

        virtual bool accept(const StreamSocket & socket) = 0;
        /// Returns true if the given StreamSocket connection should
        /// be handled, and passed on to the TCPServerDispatcher.
        ///
        /// Returns false if the socket should be closed immediately.
        ///
        /// The socket can be prevented from being closed immediately
        /// if false is returned by creating a copy of the socket.
        /// This can be used to handle certain socket connections in
        /// a special way, outside the TCPServer framework.

    protected:
        virtual ~TCPServerConnectionFilter();
    };


    class Net_API TCPServer : public DBPoco::Runnable
    /// This class implements a multithreaded TCP server.
    ///
    /// The server uses a ServerSocket to listen for incoming
    /// connections. The ServerSocket must have been bound to
    /// an address before it is passed to the TCPServer constructor.
    /// Additionally, the ServerSocket must be put into listening
    /// state before the TCPServer is started by calling the start()
    /// method.
    ///
    /// The server uses a thread pool to assign threads to incoming
    /// connections. Before incoming connections are assigned to
    /// a connection thread, they are put into a queue.
    /// Connection threads fetch new connections from the queue as soon
    /// as they become free. Thus, a connection thread may serve more
    /// than one connection.
    ///
    /// As soon as a connection thread fetches the next connection from
    /// the queue, it creates a TCPServerConnection object for it
    /// (using the TCPServerConnectionFactory passed to the constructor)
    /// and calls the TCPServerConnection's start() method. When the
    /// start() method returns, the connection object is deleted.
    ///
    /// The number of connection threads is adjusted dynamically, depending
    /// on the number of connections waiting to be served.
    ///
    /// It is possible to specify a maximum number of queued connections.
    /// This prevents the connection queue from overflowing in the
    /// case of an extreme server load. In such a case, connections that
    /// cannot be queued are silently and immediately closed.
    ///
    /// TCPServer uses a separate thread to accept incoming connections.
    /// Thus, the call to start() returns immediately, and the server
    /// continues to run in the background.
    ///
    /// To stop the server from accepting new connections, call stop().
    ///
    /// After calling stop(), no new connections will be accepted and
    /// all queued connections will be discarded.
    /// Already served connections, however, will continue being served.
    {
    public:
        TCPServer(TCPServerConnectionFactory::Ptr pFactory, DBPoco::UInt16 portNumber = 0, TCPServerParams::Ptr pParams = 0);
        /// Creates the TCPServer, with ServerSocket listening on the given port.
        /// Default port is zero, allowing any available port. The port number
        /// can be queried through TCPServer::port() member.
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        ///
        /// New threads are taken from the default thread pool.

        TCPServer(TCPServerConnectionFactory::Ptr pFactory, const ServerSocket & socket, TCPServerParams::Ptr pParams = 0);
        /// Creates the TCPServer, using the given ServerSocket.
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        ///
        /// New threads are taken from the default thread pool.

        TCPServer(
            TCPServerConnectionFactory::Ptr pFactory,
            DBPoco::ThreadPool & threadPool,
            const ServerSocket & socket,
            TCPServerParams::Ptr pParams = 0);
        /// Creates the TCPServer, using the given ServerSocket.
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        ///
        /// New threads are taken from the given thread pool.

        virtual ~TCPServer();
        /// Destroys the TCPServer and its TCPServerConnectionFactory.

        const TCPServerParams & params() const;
        /// Returns a const reference to the TCPServerParam object
        /// used by the server's TCPServerDispatcher.

        void start();
        /// Starts the server. A new thread will be
        /// created that waits for and accepts incoming
        /// connections.
        ///
        /// Before start() is called, the ServerSocket passed to
        /// TCPServer must have been bound and put into listening state.

        void stop();
        /// Stops the server.
        ///
        /// No new connections will be accepted.
        /// Already handled connections will continue to be served.
        ///
        /// Once the server has been stopped, it cannot be restarted.

        int currentThreads() const;
        /// Returns the number of currently used connection threads.

        int maxThreads() const;
        /// Returns the maximum number of threads available.

        int totalConnections() const;
        /// Returns the total number of handled connections.

        int currentConnections() const;
        /// Returns the number of currently handled connections.

        int maxConcurrentConnections() const;
        /// Returns the maximum number of concurrently handled connections.

        int queuedConnections() const;
        /// Returns the number of queued connections.

        int refusedConnections() const;
        /// Returns the number of refused connections.

        const ServerSocket & socket() const;
        /// Returns the underlying server socket.

        DBPoco::UInt16 port() const;
        /// Returns the port the server socket listens on.

        void setConnectionFilter(const TCPServerConnectionFilter::Ptr & pFilter);
        /// Sets a TCPServerConnectionFilter. Can also be used to remove
        /// a filter by passing a null pointer.
        ///
        /// To avoid a potential race condition, the filter must
        /// be set before the TCPServer is started. Trying to set
        /// the filter after start() has been called will trigger
        /// an assertion.

        TCPServerConnectionFilter::Ptr getConnectionFilter() const;
        /// Returns the TCPServerConnectionFilter set with setConnectionFilter(),
        /// or null pointer if no filter has been set.

    protected:
        void run();
        /// Runs the server. The server will run until
        /// the stop() method is called, or the server
        /// object is destroyed, which implicitly calls
        /// the stop() method.

        static std::string threadName(const ServerSocket & socket);
        /// Returns a thread name for the server thread.

    private:
        TCPServer();
        TCPServer(const TCPServer &);
        TCPServer & operator=(const TCPServer &);

        ServerSocket _socket;
        TCPServerDispatcher * _pDispatcher;
        TCPServerConnectionFilter::Ptr _pConnectionFilter;
        DBPoco::Thread _thread;
        std::atomic<bool> _stopped;
    };


    //
    // inlines
    //
    inline const ServerSocket & TCPServer::socket() const
    {
        return _socket;
    }


    inline DBPoco::UInt16 TCPServer::port() const
    {
        return _socket.address().port();
    }


    inline TCPServerConnectionFilter::Ptr TCPServer::getConnectionFilter() const
    {
        return _pConnectionFilter;
    }


}
} // namespace DBPoco::Net


#endif // DB_Net_TCPServer_INCLUDED
