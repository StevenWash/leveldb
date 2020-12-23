//
// Created by stevenhua on 2020/12/22.
//

#include "leveldb/db.h"
#include <cassert>
#include <iostream>

using namespace std;
using namespace leveldb;

int main() {
  leveldb::DB *db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, "newdb", &db);
  assert(status.ok());

//  status = db->Put(WriteOptions(), "hello", "world");
//  assert(status.ok());
  string res;
  status = db->Get(ReadOptions(), "hello", &res);
  assert(status.ok());
  cout << res << endl;

  delete db;
  return 0;
}