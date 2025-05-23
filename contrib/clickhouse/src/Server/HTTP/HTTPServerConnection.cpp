#include <Server/HTTP/HTTPServerConnection.h>
#include <Server/TCPServer.h>

#include <DBPoco/Net/NetException.h>
#include <Common/logger_useful.h>

namespace DB
{

HTTPServerConnection::HTTPServerConnection(
    HTTPContextPtr context_,
    TCPServer & tcp_server_,
    const DBPoco::Net::StreamSocket & socket,
    DBPoco::Net::HTTPServerParams::Ptr params_,
    HTTPRequestHandlerFactoryPtr factory_,
    const ProfileEvents::Event & read_event_,
    const ProfileEvents::Event & write_event_)
    : TCPServerConnection(socket), context(std::move(context_)), tcp_server(tcp_server_), params(params_), factory(factory_), read_event(read_event_), write_event(write_event_), stopped(false)
{
    DB_poco_check_ptr(factory);
}

void HTTPServerConnection::run()
{
    std::string server = params->getSoftwareVersion();
    DBPoco::Net::HTTPServerSession session(socket(), params);

    while (!stopped && tcp_server.isOpen() && session.hasMoreRequests() && session.connected())
    {
        try
        {
            std::lock_guard lock(mutex);
            if (!stopped && tcp_server.isOpen() && session.connected())
            {
                HTTPServerResponse response(session);
                HTTPServerRequest request(context, response, session, read_event);

                DBPoco::Timestamp now;

                if (!forwarded_for.empty())
                    request.set("X-Forwarded-For", forwarded_for);

                if (request.isSecure())
                {
                    size_t hsts_max_age = context->getMaxHstsAge();

                    if (hsts_max_age > 0)
                        response.add("Strict-Transport-Security", "max-age=" + std::to_string(hsts_max_age));

                }

                response.setDate(now);
                response.setVersion(request.getVersion());
                response.setKeepAlive(params->getKeepAlive() && request.getKeepAlive() && session.canKeepAlive());
                if (!server.empty())
                    response.set("Server", server);
                try
                {
                    if (!tcp_server.isOpen())
                    {
                        sendErrorResponse(session, DBPoco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
                        break;
                    }
                    std::unique_ptr<HTTPRequestHandler> handler(factory->createRequestHandler(request));

                    if (handler)
                    {
                        if (request.getExpectContinue() && response.getStatus() == DBPoco::Net::HTTPResponse::HTTP_OK)
                            response.sendContinue();

                        handler->handleRequest(request, response, write_event);
                        session.setKeepAlive(params->getKeepAlive() && response.getKeepAlive() && session.canKeepAlive());
                    }
                    else
                        sendErrorResponse(session, DBPoco::Net::HTTPResponse::HTTP_NOT_IMPLEMENTED);
                }
                catch (DBPoco::Exception &)
                {
                    if (!response.sent())
                    {
                        try
                        {
                            sendErrorResponse(session, DBPoco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                        }
                        catch (...) // NOLINT(bugprone-empty-catch)
                        {
                        }
                    }
                    throw;
                }
            }
        }
        catch (const DBPoco::Net::NoMessageException &)
        {
            break;
        }
        catch (const DBPoco::Net::MessageException &)
        {
            sendErrorResponse(session, DBPoco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        }
        catch (const DBPoco::Net::NetException & e)
        {
            /// Do not spam logs with messages related to connection reset by peer.
            if (e.code() == DB_POCO_ENOTCONN)
            {
                LOG_DEBUG(LogFrequencyLimiter(getLogger("HTTPServerConnection"), 10), "Connection reset by peer while processing HTTP request: {}", e.message());
                break;
            }

            if (session.networkException())
                session.networkException()->rethrow();
            else
                throw;
        }

        catch (const DBPoco::Exception &)
        {
            if (session.networkException())
            {
                session.networkException()->rethrow();
            }
            else
                throw;
        }
    }
}

// static
void HTTPServerConnection::sendErrorResponse(DBPoco::Net::HTTPServerSession & session, DBPoco::Net::HTTPResponse::HTTPStatus status)
{
    HTTPServerResponse response(session);
    response.setVersion(DBPoco::Net::HTTPMessage::HTTP_1_1);
    response.setStatusAndReason(status);
    response.setKeepAlive(false);
    response.send();
    session.setKeepAlive(false);
}

}
