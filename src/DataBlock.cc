#include "DataBlock.hh"

#include <arpa/inet.h>

namespace eagel {

DataBlock::DataBlock(DataFile *file, DataBlock *next, DataBlock *prev,
		unsigned long size) :
		_next(
				htonll(
						nullptr == next ?
								0 :
								reinterpret_cast<unsigned long>(next)
										- reinterpret_cast<unsigned long>(file))), _prev(
				htonll(
						nullptr == prev ?
								0 :
								reinterpret_cast<unsigned long>(prev)
										- reinterpret_cast<unsigned long>(file))), _size(
				htonll(size)) {
	// write tail size value
	(*reinterpret_cast<unsigned long *>(_data + size)) = htonll(size);
}

DataBlock *DataBlock::next(DataFile *file) {
	return 0 == ntohll(_next) ?
			nullptr :
			reinterpret_cast<DataBlock *>(ntohll(_next)
					+ reinterpret_cast<unsigned long>(file));
}

void DataBlock::next(DataFile *file, DataBlock *next) {
	_next = htonll(
			nullptr == next ?
					0 :
					reinterpret_cast<unsigned long>(next)
							- reinterpret_cast<unsigned long>(file));
}

DataBlock *DataBlock::prev(DataFile *file) {
	return 0 == ntohll(_prev) ?
			nullptr :
			reinterpret_cast<DataBlock *>(ntohll(_prev)
					+ reinterpret_cast<unsigned long>(file));
}

void DataBlock::prev(DataFile *file, DataBlock *prev) {
	_prev = htonll(
			nullptr == prev ?
					0 :
					reinterpret_cast<unsigned long>(prev)
							- reinterpret_cast<unsigned long>(file));
}

int DataBlock::size() {
	return ntohll(_size);
}

int DataBlock::length() {
	return ntohll(_size) + sizeof(_next) + sizeof(_prev) + sizeof(_size) * 2;
}

unsigned char * DataBlock::data() {
	return _data;
}

} /* namespace eagel */
