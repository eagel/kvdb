#ifndef DATABLOCK_HH_
#define DATABLOCK_HH_

namespace eagel {

class DataFile;

class DataBlock {
	unsigned long _next;
	unsigned long _prev;
	unsigned long _size;
	unsigned char _data[];
public:
	DataBlock(DataFile *file, DataBlock *next, DataBlock *prev, unsigned long size);

	DataBlock *next(DataFile *file);
	void next(DataFile *file, DataBlock *next);
	DataBlock *prev(DataFile *file);
	void prev(DataFile *file, DataBlock *prev);
	int size();
	int length();
	unsigned char * data();
};

} /* namespace eagel */

#endif /* DATABLOCK_HH_ */
