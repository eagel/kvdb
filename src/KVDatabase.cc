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

namespace eagel {

KVDatabase::KVDatabase(const char *host, unsigned short port, const char *file) :
		_host(host), _port(port), _file(file), _file_handler(0), _buffer(
				nullptr), _port_handler(0) {

}

KVDatabase::~KVDatabase() {
	if (0 != _port_handler) {
		close(_port_handler);
		_port_handler = 0;
	}
	if (nullptr != _buffer) {
		reinterpret_cast<DataFile *>(_buffer)->pid(0);
		reinterpret_cast<DataFile *>(_buffer)->mounted(false);

		munmap(_buffer, reinterpret_cast<DataFile *>(_buffer)->length());
		_buffer = nullptr;
	}
	if (0 != _file_handler) {
		close(_file_handler);
		_file_handler = 0;
	}
}

int KVDatabase::execute() {
	mount_data_file();
	listen_port();
	// TODO
	return EXIT_SUCCESS;
}

void KVDatabase::mount_data_file() {
	// create data file
	_file_handler = open(_file, O_RDWR | O_CREAT, 00600);
	if (-1 == _file_handler) {
		throw std::string(strerror(errno));
	}

	// verify file
	bool is_new_file = false;
	unsigned char magic[2];
	ssize_t readed = read(_file_handler, magic, 2);
	if (0 == readed) {
		is_new_file = true;
	} else if ('e' != magic[0] || 'a' != magic[1]) {
		throw std::string("file: ") + _file + " is not a data file";
	}

	// initialize new file
	if (is_new_file) {
		// write magic number
		if (-1 == write(_file_handler, "ea", 2)) {
			throw std::string(strerror(errno));
		}

		// write mount flat
		if (-1 == write(_file_handler, "\x0", 1)) {
			throw std::string(strerror(errno));
		}

		// write padding
		if (-1 == write(_file_handler, "\x0", 1)) {
			throw std::string(strerror(errno));
		}

		// write mount pid
		if (-1 == write(_file_handler, "\x0\x0\x0\x0", 4)) {
			throw std::string(strerror(errno));
		}

		// write free index
		unsigned long index = htonll(sizeof(DataFile));
		if (-1 == write(_file_handler, &index, 8)) {
			throw std::string(strerror(errno));
		}

		// write used index
		if (-1 == write(_file_handler, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
			throw std::string(strerror(errno));
		}

		// write data block size, the default is 1024 bytes
		unsigned long size = htonll(1024);
		if (-1 == write(_file_handler, &size, 8)) {
			throw std::string(strerror(errno));
		}

		// write free data block
		{
			// next
			if (-1 == write(_file_handler, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
				throw std::string(strerror(errno));
			}
			// prev
			if (-1 == write(_file_handler, "\x0\x0\x0\x0\x0\x0\x0\x0", 8)) {
				throw std::string(strerror(errno));
			}
			// size
			unsigned long size = htonll(1024 - sizeof(DataFile) - 8);
			if (-1 == write(_file_handler, &size, 8)) {
				throw std::string(strerror(errno));
			}

			// block
			if (-1 == lseek(_file_handler, 992, SEEK_CUR)) {
				throw std::string(strerror(errno));
			}

			// tail size
			if (-1 == write(_file_handler, &size, 8)) {
				throw std::string(strerror(errno));
			}
		}

	}

	// check data file is OK;
	if (-1 == lseek(_file_handler, 2, SEEK_SET)) {
		throw std::string(
				"the data file is corrupted: can not read mount status");
	}
	bool is_mounted = 0;
	int pid = 0;

	readed = read(_file_handler, &is_mounted, 1);

	if (1 != readed) {
		throw std::string(
				"the data file is corrupted: can not read mount status");
	}

	// skip padding
	if (-1 == lseek(_file_handler, 1, SEEK_CUR)) {
		throw std::string(strerror(errno));
	}

	readed = read(_file_handler, &pid, 4);

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
	if (-1 == lseek(_file_handler, 16, SEEK_CUR)) {
		throw std::string(strerror(errno));
	}

	unsigned long size;
	readed = read(_file_handler, &size, 8);
	if (8 != readed) {
		throw std::string(
				"the data file is corrupted: can not read data file size.");
	} else {
		size = ntohll(size);
	}

	// map file to memory
	_buffer = mmap(nullptr, size + sizeof(DataFile), PROT_READ | PROT_WRITE,
	MAP_SHARED, _file_handler, 0);

	reinterpret_cast<DataFile *>(_buffer)->mounted(true);
	reinterpret_cast<DataFile *>(_buffer)->pid(getpid());

	msync(_buffer, reinterpret_cast<DataFile *>(_buffer)->length(), MS_SYNC);
}

void KVDatabase::listen_port() {
	// TODO
}

} /* namespace eagel */
