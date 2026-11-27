#include "Client.hpp"
#include <unistd.h>
#include <poll.h>

Client::Client(int fd, const ServerConfig* server) : fd(fd), server(server), lastActivity(Clock::now())
{
}

Client::~Client()
{
	close(fd);
}

short Client::pollEvents() const
{
	if (state == ClientState::ReadingRequest)
		return POLLIN;
	else if (state == ClientState::WritingResponse)
		return POLLOUT;
	else
		return 0;
}

void Client::touch()
{
	lastActivity = Clock::now();
}

bool Client::isIdleLongerThan(std::chrono::seconds limit) const
{
	return ((Clock::now() - lastActivity) > limit);
}
void Client::resetForNextRequest()
{
	state = ClientState::ReadingRequest;
	writeBuffer.clear();
	bytesSent = 0;
	request = Request();
	response = Response();
	cgi.reset();
	closeAfterWrite = false;
}