// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <node.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <v8.h>
#include <vector>

using namespace v8;

bool write_response(int fd) {
	// This is what Nginx does.
	char buf[2048];
	time_t now = time(0);
	struct tm gm = *gmtime(&now);
	if (strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &gm) <= 0) {
		fprintf(stderr, "strftime's output didn't fit in 1024 bytes\n");
		exit(1);
	}
	std::ostringstream response;
	response << "HTTP/1.0 200 OK\n" <<
	"Date: " << buf << "\n" <<
	"Content-Length: 23\n" <<
	"Content-Type: text/plain; charset=utf-8\n\n" <<
	"User function is ready\n";
	std::string res = response.str();

	while (true) {
		ssize_t result = write(fd, res.c_str(), res.length());
		if (result == 0) {
			// Socket is writable, but EOF.
			return true;
		}
		if (result != -1) {
			fprintf(stderr, "wrote %zd of %lu byte(s) to FD %d, ", result, res.length(), fd);
			res = res.substr(static_cast<unsigned long>(result));
			fprintf(stderr, "%lu byte(s) left\n", res.length());
			if (res.length() == 0) {
				return true;
			}
		} else if (errno != EINTR) {
			return false;
		}
	}
}

void init(Handle<Object> target) {
	const char bin[] = "./bin/mainapp";

	// Clear CLOEXEC for STDOUT and STDERR.
	if (fcntl(STDOUT_FILENO, F_SETFD, 0) == -1) {
		fprintf(stderr, "fcntl(STDOUT_FILENO, F_SETFD, 0) %d\n", errno);
		exit(1);
	}
	if (fcntl(STDERR_FILENO, F_SETFD, 0) == -1) {
		fprintf(stderr, "fcntl(STDERR_FILENO, F_SETFD, 0) %d\n", errno);
		exit(1);
	}

	DIR *dir = opendir(
		#ifdef __APPLE__
		"/dev/fd"
		#else
		"/proc/self/fd"
		#endif
	);
	if (dir == NULL) {
		fprintf(stderr, "opendir failed\n");
		exit(1);
	}

	std::vector<std::string> fds;

	for (struct dirent *ent = readdir(dir); ent != NULL; ent = readdir(dir)) {
		int fd = atoi(ent->d_name);

		struct sockaddr_storage addr;
		socklen_t addrlen = sizeof(addr);
		if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrlen) == -1) {
			continue;
		}

		// Clear NONBLOCK for the socket FD.
		if (fcntl(fd, F_SETFD, O_CLOEXEC) == -1) {
			fprintf(stderr, "fcntl(%d, F_SETFD, 0) %d\n", fd, errno);
			exit(1);
		}

		if (write_response(fd)) {
			// Socket was writable, so it isn't listening.
			continue;
		}

		// Clear CLOEXEC and reset NONBLOCK.
		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
			// This is unexpected, but it should be fine to continue on.
			fprintf(stderr, "Setting listening FD %d to O_NONBLOCK failed.\n", fd);
		}

		fds.push_back(ent->d_name);
	}

	// Convert the vector to a comma separated list.
	std::ostringstream flag;
	for(size_t i = 0; i < fds.size(); ++i) {
		if(i > 0) flag << ",";
		flag << fds[i];
	}

	std::vector<const char*> args;
	args.push_back(bin);
	args.push_back("--fds");
	args.push_back(flag.str().c_str());
	args.push_back(NULL);

	fprintf(stderr, "execing replacement binary...\n");
//	execv(bin, const_cast<char* const*>(&args[0]));
	char *envp[] =
	{              
			"LD_LIBRARY_PATH=/user_code/lib:$LD_LIBRARY_PATH",
			NULL
	};

	execve(bin, const_cast<char* const*>(&args[0]), envp);  

	fprintf(stderr, "execve %d\n", errno);
	exit(1);
}
NODE_MODULE(execer, init)