#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <cstddef>
#include <ctime>
#include <string>
#include <sys/types.h>

// The parent-owned ends of the CGI pipes; -1 means no open descriptor.
class CgiProcess
{
public:
	pid_t pid;
	int input_fd;                    // Parent writes request body to CGI stdin.
	int output_fd;                   // Parent reads CGI stdout here.
	std::time_t start_time;          // 0 means the process has not started.
	std::string output_buffer;
	// Bytes of the request body already written to input_fd. A non-blocking
	// write() may accept only part of the body, so the next write resumes
	// from this offset; input_fd can be closed once it equals the body size.
	std::size_t body_bytes_written;

	// Constructor:
	CgiProcess()
		: pid(-1), input_fd(-1), output_fd(-1), start_time(0),
		  body_bytes_written(0) {}
};

#endif
