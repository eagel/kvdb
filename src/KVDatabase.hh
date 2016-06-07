#ifndef KVDATABASE_HH_
#define KVDATABASE_HH_

#include "DataFile.hh"

namespace eagel {

class KVDatabase {
	const char * _host;
	unsigned short _port;
	const char * _file;

	int _file_handler;
	void * _buffer;
	int _port_handler;
public:
	KVDatabase(const char *host, unsigned short port, const char *file);
	~KVDatabase();

	int execute();
private:
	void mount_data_file();
	void listen_port();
	void main_loop();
private:
	KVDatabase(const KVDatabase&) = delete;
	KVDatabase & operator=(const KVDatabase&) = delete;
};

} /* namespace eagel */

#endif /* KVDATABASE_HH_ */
