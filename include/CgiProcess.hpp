#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// One running CGI child. Owned by Client through std::unique_ptr and destroyed
// when the client disconnects or the request finishes.
//
// A CgiProcess owns the parent-side pipe fds and the child process:
// the destructor closes any open fd, then kills and reaps a child that has not
// been reaped yet. Code that closes an fd early must set it to -1, and code
// that reaps the child with waitpid() must set pid to -1, so the destructor
// does not close a reused fd or signal a reused pid.
class CgiProcess
{
public:
	using Clock = std::chrono::steady_clock;

	pid_t pid;                       // -1 when there is no child to reap.
	int input_fd;                    // Parent writes request body to CGI stdin.
	int output_fd;                   // Parent reads CGI stdout here.
	Clock::time_point start_time;    // Same clock as Client::lastActivity.
	std::string output_buffer;
	// Bytes of the request body already written to input_fd. A non-blocking
	// write() may accept only part of the body, so the next write resumes
	// from this offset; input_fd can be closed once it equals the body size.
	std::size_t body_bytes_written;

	CgiProcess()
		: pid(-1), input_fd(-1), output_fd(-1), start_time(),
		  body_bytes_written(0) {}

	~CgiProcess()
	{
		if (input_fd != -1)
			close(input_fd);
		if (output_fd != -1)
			close(output_fd);
		if (pid > 0)
		{
			kill(pid, SIGKILL);
			while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
				;
		}
	}

	// One owner for the fds and the pid. A copy would close the pipes twice
	// and try to reap the same child twice.
	CgiProcess(const CgiProcess &) = delete;
	CgiProcess &operator=(const CgiProcess &) = delete;
};

#endif
