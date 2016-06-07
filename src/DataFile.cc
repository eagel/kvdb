#include "DataFile.hh"

#include <arpa/inet.h>

namespace eagel {

bool DataFile::mounted() {
	return _mounted;
}

void DataFile::mounted(bool m) {
	_mounted = m;
}

int DataFile::pid() {
	return ntohl(_pid);
}

void DataFile::pid(int p) {
	_pid = htonl(p);
}

unsigned long DataFile::size() {
	return ntohll(_size);
}

void DataFile::size(unsigned long s) {
	_size = htonll(s);
}

unsigned long DataFile::length() {
	return ntohll(_size) + sizeof(DataFile);
}

} /* namespace eagel */
