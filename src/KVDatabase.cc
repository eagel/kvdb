#include "KVDatabase.hh"

#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

namespace eagel {

KVDatabase::KVDatabase(const char *host, unsigned short port, const char *file) :
		_host(host), _port(port), _file(file), _file_descriptor(0), _buffer(
				nullptr), _listen_descriptors(), _read_buffers(), _write_buffers() {

}

KVDatabase::~KVDatabase() {
	// close opened ports
	std::set<int> opened_descriptors(_opened_descriptors);
	for (int opened_descriptor : opened_descriptors) {
		close(opened_descriptor);
		_opened_descriptors.erase(opened_descriptor);
		_read_buffers.erase(opened_descriptor);
		_write_buffers.erase(opened_descriptor);
	}

	// close listened ports
	std::set<int> listen_descriptors(_listen_descriptors);
	for (int listened_descriptor : listen_descriptors) {
		close(listened_descriptor);
		_listen_descriptors.erase(listened_descriptor);
	}

	// umount data file
	if (nullptr != _buffer) {
		reinterpret_cast<DataFile *>(_buffer)->pid(0);
		reinterpret_cast<DataFile *>(_buffer)->mounted(false);

		munmap(_buffer, reinterpret_cast<DataFile *>(_buffer)->length());
		_buffer = nullptr;
	}

	// close file
	if (0 != _file_descriptor) {
		close(_file_descriptor);
		_file_descriptor = 0;
	}
}

int KVDatabase::execute() {
	mount_data_file();
	listen_port();
	main_loop();
	// TODO
	return EXIT_SUCCESS;
}

void KVDatabase::mount_data_file() {
	// create data file
	_file_descriptor = open(_file, O_RDWR | O_CREAT, 00600);
	if (-1 == _file_descriptor) {
		throw std::string(strerror(errno));
	}

	// verify file
	bool is_new_file = false;
	unsigned char magic[2];
	ssize_t readed = read(_file_descriptor, magic, 2);
	if (0 == readed) {
		is_new_file = true;
	} else if ('e' != magic[0] || 'a' != magic[1]) {
		throw std::string("file: ") + _file + " is not a data file";
	}

	// initialize new file
	if (is_new_file) {
		// write magic number
		if (-1 == write(_file_descriptor, "ea", 2)) {
			throw std::string(strerror(errno));
		}

		// write mount flat
		if (-1 == write(_file_descriptor, "\x0", 1)) {
			throw std::string(strerror(errno));
		}

		// write padding
		if (-1 == write(_file_descriptor, "\x0", 1)) {
			throw std::string(strerror(errno));
		}

		// write mount pid
		if (-1 == write(_file_descriptor, "\x0\x0\x0\x0", 4)) {
			throw std::string(strerror(errno));
		}

		// write free index
		unsigned long index = htonll(sizeof(DataFile));
		if (-1 == write(_file_descriptor, &index, 8)) {
			throw std::string(strerror(errno));
		}

		// write used index
		if (-1 == write(_file_descriptor, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
			throw std::string(strerror(errno));
		}

		// write data block size, the default is 1024 bytes
		unsigned long size = htonll(1024);
		if (-1 == write(_file_descriptor, &size, 8)) {
			throw std::string(strerror(errno));
		}

		// write free data block
		{
			// next
			if (-1 == write(_file_descriptor, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
				throw std::string(strerror(errno));
			}
			// prev
			if (-1 == write(_file_descriptor, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
				throw std::string(strerror(errno));
			}
			// size
			unsigned long size = htonll(1024 - sizeof(DataFile) - 8);
			if (-1 == write(_file_descriptor, &size, 8)) {
				throw std::string(strerror(errno));
			}

			// block
			if (-1 == lseek(_file_descriptor, 992, SEEK_CUR)) {
				throw std::string(strerror(errno));
			}

			// tail size
			if (-1 == write(_file_descriptor, &size, 8)) {
				throw std::string(strerror(errno));
			}
		}

	}

	// check data file is OK;
	if (-1 == lseek(_file_descriptor, 2, SEEK_SET)) {
		throw std::string(
				"the data file is corrupted: can not read mount status");
	}
	bool is_mounted = 0;
	int pid = 0;

	readed = read(_file_descriptor, &is_mounted, 1);

	if (1 != readed) {
		throw std::string(
				"the data file is corrupted: can not read mount status");
	}

	// skip padding
	if (-1 == lseek(_file_descriptor, 1, SEEK_CUR)) {
		throw std::string(strerror(errno));
	}

	readed = read(_file_descriptor, &pid, 4);

	if (4 != readed) {
		throw std::string(
				"the data file is corrupted: can not read mount process.");
	} else {
		pid = ntohl(pid);
	}

	if (is_mounted) {
		std::stringstream ss;
		ss << "the data file is mounted by ";
		ss << pid;
		throw std::string(ss.str());
	}

	// load size
	if (-1 == lseek(_file_descriptor, 16, SEEK_CUR)) {
		throw std::string(strerror(errno));
	}

	unsigned long size;
	readed = read(_file_descriptor, &size, 8);
	if (8 != readed) {
		throw std::string(
				"the data file is corrupted: can not read data file size.");
	} else {
		size = ntohll(size);
	}

	// map file to memory
	_buffer = mmap(nullptr, size + sizeof(DataFile), PROT_READ | PROT_WRITE,
	MAP_SHARED, _file_descriptor, 0);

	reinterpret_cast<DataFile *>(_buffer)->mounted(true);
	reinterpret_cast<DataFile *>(_buffer)->pid(getpid());

	msync(_buffer, reinterpret_cast<DataFile *>(_buffer)->length(), MS_SYNC);
}

void KVDatabase::listen_port() {
	//	resolve socket address
	addrinfo * ai = nullptr;
	addrinfo hint;

	memset(&hint, 0, sizeof(hint));

	hint.ai_flags = AI_PASSIVE;
	hint.ai_protocol = IPPROTO_TCP;
	hint.ai_socktype = SOCK_STREAM;

	std::stringstream ss;

	ss << _port;

	int rst;

	if (0 != (rst = getaddrinfo(_host, ss.str().c_str(), &hint, &ai))) {
		throw std::string(gai_strerror(rst));
	}

	addrinfo * cai = ai;

	while (nullptr != cai) {
		int s = socket(cai->ai_family, cai->ai_socktype, cai->ai_protocol);

		if (-1 == s) {
			throw std::string(strerror(errno));
		}

		if (bind(s, cai->ai_addr, cai->ai_addrlen)) {
			throw std::string(strerror(errno));
		}

		if (listen(s, 100)) {
			throw std::string(strerror(errno));
		}

		_listen_descriptors.insert(s);

		cai = cai->ai_next;
	}

	freeaddrinfo(ai);
}

void KVDatabase::main_loop() {

	fd_set read_fds;
	fd_set write_fds;
	fd_set error_fds;
	int nfds;

	while (true) {

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&error_fds);
		nfds = 0;

		// monitor listen
		for (int i : _listen_descriptors) {
			FD_SET(i, &read_fds);
			if (i > nfds) {
				nfds = i;
			}
		}

		// monitor opened
		for (int i : _opened_descriptors) {
			FD_SET(i, &read_fds);

			if (_write_buffers[i].str().length()) {
				FD_SET(i, &write_fds);
			}

			FD_SET(i, &error_fds);
			if (i > nfds) {
				nfds = i;
			}
		}

		if (-1
				== select(nfds + 1, &read_fds, &write_fds, &error_fds,
						nullptr)) {
			throw std::string(strerror(errno));
		}

		// process listen port
		for (int i : _listen_descriptors) {
			if (FD_ISSET(i, &read_fds)) {
				int s = accept(i, nullptr, nullptr);
				if (-1 == s) {
					throw std::string(strerror(errno));
				}

				_opened_descriptors.insert(s);
				_read_buffers.insert(
						std::map<int, std::stringstream>::value_type(s,
								std::stringstream()));
				_write_buffers.insert(
						std::map<int, std::stringstream>::value_type(s,
								std::stringstream()));
			}
		}

		// process opened port
		for (int i : std::set<int>(_opened_descriptors)) {
			// process read
			if (FD_ISSET(i, &read_fds)) {
				char buffer[1024];
				int read = recv(i, buffer, 1024, 0);
				if (read <= 0) {
					close(i);
					_read_buffers.erase(i);
					_write_buffers.erase(i);
					_opened_descriptors.erase(i);
				} else {
					_read_buffers[i].write(buffer, read);
					received(i);
				}
			}
			// process write
			if (FD_ISSET(i, &write_fds)) {
				std::string data = _write_buffers[i].str();

				int sent = send(i, data.c_str(), data.length(), 0);
				if (sent <= 0) {
					close(i);
					_read_buffers.erase(i);
					_write_buffers.erase(i);
					_opened_descriptors.erase(i);
				} else {
					_write_buffers[i].str(data.substr(sent, data.length()));
					_write_buffers[i].seekp(0, std::stringstream::end);
				}
			}
			// process error
			if (FD_ISSET(i, &error_fds)) {
				close(i);
				_read_buffers.erase(i);
				_write_buffers.erase(i);
				_opened_descriptors.erase(i);
			}
		}
	}

}

void KVDatabase::received(int i) {
	// TODO assert is a completed commad.
	std::cout << _read_buffers[i].str() << std::endl;
	// TODO call command handler.


}

} /* namespace eagel */
