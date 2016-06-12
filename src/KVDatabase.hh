#ifndef KVDATABASE_HH_
#define KVDATABASE_HH_

#include <set>
#include <map>
#include <sstream>
#include "DataFile.hh"

namespace eagel {

class KVDatabase {
	const char * _host;
	unsigned short _port;
	const char * _file;

	int _file_descriptor;
	void * _buffer;
	std::set<int> _listen_descriptors;
	std::set<int> _opened_descriptors;
	std::map<int, std::stringstream> _read_buffers;
	std::map<int, std::stringstream> _write_buffers;
public:
	KVDatabase(const char *host, unsigned short port, const char *file);
	~KVDatabase();

	int execute();
private:
	void mount_data_file();
	void listen_port();
	void main_loop();

	void received(int i);
private:
	KVDatabase(const KVDatabase&) = delete;
	KVDatabase & operator=(const KVDatabase&) = delete;
};

} /* namespace eagel */

#endif /* KVDATABASE_HH_ */
