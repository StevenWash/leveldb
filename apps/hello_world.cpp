//
// Created by stevenhua on 2020/12/22.
//

#include "leveldb/db.h"
#include <cassert>
#include <iostream>
#include <unistd.h>

using namespace std;
using namespace leveldb;

char* EncodeVarint32b(char* dst, uint32_t v) {
  // Operate on characters as unsigneds
  uint8_t* ptr = reinterpret_cast<uint8_t*>(dst);
  static const int B = 128;
  if (v < (1 << 7)) {
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    *(ptr++) = v | B;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = v >> 21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = (v >> 21) | B;
    *(ptr++) = v >> 28;
  }
  return reinterpret_cast<char*>(ptr);
}

int main() {

  char * codeString;
  uint32_t num = 127;
  char* ptr = EncodeVarint32b(codeString, num);
//  printf("ptr: %s\n", ptr);

  leveldb::DB *db;
  leveldb::Options options;

  options.write_buffer_size = 60 * 1024;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, "newdb", &db);
  assert(status.ok());

  for (int i = 0; i < 100; i++) {
    status = db->Put(WriteOptions(), "name" + i, "someone" + i);
    assert(status.ok());
//    sleep(1);
  }

  for (int i = 0; i < 1; i++) {
    int r = rand_r(reinterpret_cast<unsigned int*>(1));
    status = db->Delete(WriteOptions(), "name" + r);
    assert(status.ok());
  }

  delete db;
  return 0;
}

