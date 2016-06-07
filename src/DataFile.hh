#ifndef DATAFILE_HH_
#define DATAFILE_HH_

#include "DataBlock.hh"

namespace eagel {

class DataFile {
	char _magic[2];
	bool _mounted;
	int _pid;
	unsigned long _free;
	unsigned long _used;
	unsigned long _size;
	unsigned char _data[];
public:
	bool mounted();
	void mounted(bool m);

	int pid();
	void pid(int p);

	unsigned long size();
	void size(unsigned long s);

	unsigned long length();
};

} /* namespace eagel */

#endif /* DATAFILE_HH_ */
