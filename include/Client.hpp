#ifndef CLIENT_HPP
# define CLIENT_HPP

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include "CgiProcess.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include "ServerConfig.hpp"

// Where a client is in its life. Each client is in exactly one state, and the
// poll() loop looks only at the state to know what to wait for.
// See "Client states" in the sprint plan.
enum class ClientState
{
	ReadingRequest,		// Wait for POLLIN until the request is complete.	Next: Processing
	Processing,			// B builds the Response. No socket I/O.			Next: WritingResponse or WaitingCgi
	WaitingCgi,			// Wait on the CGI pipes, not on this socket.		Next: WritingResponse
	WritingResponse,	// Wait for POLLOUT until every byte is sent.		Next: Closing, or ReadingRequest (keep-alive)
	Closing				// Nothing to wait for: the loop removes the client.
};

// One connected client. Part A owns the loop; part B reads and writes
// request and response.
//
// A Client owns its socket fd and closes it when destroyed. Copying is
// forbidden, because two copies would close the same fd twice. Store clients
// where they never move, and build them in place:
// 	std::map<int, Client> clients;
// 	clients.try_emplace(fd, fd, server);
class Client
{
public:
	using Clock = std::chrono::steady_clock;

	// server: the config of the listening socket that accepted this client.
	Client(int fd, const ServerConfig* server);
	~Client();

	// One fd, one owner. A copy would close the socket twice, and the second close could hit another client's fd.
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;

	// The poll() events this client needs now, from its state:
	// POLLIN while ReadingRequest, POLLOUT while WritingResponse, 0 otherwise.
	// poll() reports POLLHUP and POLLERR whatever we ask for.
	short pollEvents() const;

	// Call after every recv() or send() on this socket that returned more than 0.
	void touch();
	// For the idle timeout (A6, 408).
	bool isIdleLongerThan(std::chrono::seconds limit) const;

	// Keep-alive: get ready for the next request on the same connection.
	// Clears request, response, writeBuffer, bytesSent, cgi and closeAfterWrite,
	// and sets the state to ReadingRequest. Keeps readBuffer: the bytes left in
	// it may already be the start of the next request.
	void resetForNextRequest();

	const int			fd;
	const ServerConfig*	server;				// Not owned. Set at accept(), never changes.
	ClientState			state = ClientState::ReadingRequest;

	std::string			readBuffer;			// Bytes received, not yet used by the parser.
	std::string			writeBuffer;		// Response bytes to send.
	std::size_t			bytesSent = 0;		// Bytes of writeBuffer already sent.
	Clock::time_point	lastActivity;		// Set by the constructor and touch().

	Request				request;
	Response			response;

	std::unique_ptr<CgiProcess> cgi;		// Empty when no CGI is running.
	bool                closeAfterWrite = false;	// true: Closing after the response.
													// false: keep-alive.
};

#endif