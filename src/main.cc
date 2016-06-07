#include <iostream>
#include <cstdlib>
#include <string>

#include "KVDatabase.hh"

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cout << "Usage: cpp host port [data-file]" << std::endl;
		return EXIT_FAILURE;
	}

	const char *host = argv[1];
	unsigned short port = atoi(argv[2]);
	const char *file = argc < 4 ? "data" : argv[3];

	eagel::KVDatabase db(host, port, file);
	try {
		return db.execute();
	} catch (std::string &e) {
		std::cout << e << std::endl;
		return EXIT_FAILURE;
	}
}
