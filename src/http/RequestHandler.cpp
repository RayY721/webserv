#include "RequestHandler.hpp"

#include <string>

// For now every request gets the same small page, so A can test A5 with curl.
// B6 will look for the file of req.path in the server's root.
// The parameters have no name yet because they are not used yet: -Werror would refuse them.
Response handleRequest(const Request&, const ServerConfig&)
{
	Response response;
	response.status = 200;
	response.headers["Content-Type"] = "text/html";
	response.body = "<html><body><h1>webserv is alive</h1></body></html>\n";
	return response;
}

// For now a small default page with the status number.
// B7 will use the error_page of the server's config when it has one.
Response errorResponse(int status, const ServerConfig&)
{
	Response response;
	response.status = status;
	response.headers["Content-Type"] = "text/html";
	response.body = "<html><body><h1>" + std::to_string(status) + "</h1></body></html>\n";
	return response;
}
