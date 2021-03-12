## Files

LevelDB的实现借鉴了[Bigtable tablet (section 5.3)](http://research.google.com/archive/bigtable.html) 的思想，
但文件的组织形式还有所不同，下面进行相关说明。

每个leveldb数据库都是由存储在相同目录下的一组文件组成，下面是这几种文件的说明:

### Log files

leveldb的写操作并不是直接写入磁盘，而是首先写入内存，然后由内存刷到磁盘进行持久化，为了防止为刷到磁盘的数据在断电、进程异常或宕机
的时候丢失数据，在写之前会将写操作首先记录在WAL的log文件中，当异常情况发生后，可以通过重放log文件中的数据来进行恢复。

```c++

// Add to log and apply to memtable.  We can release the lock
// during this phase since &w is currently responsible for logging
// and protects against concurrent loggers and concurrent writes
// into mem_.
{
  // 加锁  
  mutex_.Unlock();
  // step1: 记录数据到日志文件
  status = log_->AddRecord(WriteBatchInternal::Contents(write_batch));
  bool sync_error = false;
  if (status.ok() && options.sync) {
      // step2: 将数据从磁盘缓存刷盘
    status = logfile_->Sync();
    if (!status.ok()) {
      sync_error = true;
    }
  }
  if (status.ok()) {
    // step3: 将数据写到内存中的memtable中  
    status = WriteBatchInternal::InsertInto(write_batch, mem_);
  }
  // 释放锁
  mutex_.Lock();
  if (sync_error) {
    // The state of the log file is indeterminate: the log record we
    // just added may or may not show up when the DB is re-opened.
    // So we force the DB into a mode where all future writes fail.
    RecordBackgroundError(status);
  }
}
```
从上面的代码可以看到step1、step2、step3的执行顺序，并且将这个三个步骤包装在一个锁内，避免并发访问的问题。

leveldb中.log文件记录了最近一段时间的更新操作，每次更新操作都会顺序的追加在当前.log文件的末尾，
当log文件达到预设的大小（默认大约4MB，通过write_buffer_size控制）时，会将log文件中记录的内容转化为sorted table进行存储，并
创建新的log文件来记录新的操作。

在内存中保存了一份当前log文件内容的副本，称之为memtable，由于每次更新操作都会记录在log文件中，
而log文件在内存有一份副本，每次读取的时候，都会从内存的副本中读取，因此，读取到的都是更新之后的记录信息。

write_buffer_size：表示的即是一个memtable最大的大小，默认为4MB，最小64KB，最大1GB，
对应磁盘上未转为sorted table的log文件的大小。
  - 1、增大该值可以提升性能，尤其是批量加载的时候，可以通过该参数控制内存使用
  - 2、但是也会导致数据库在open的时候的恢复时间增长（因为对应的log文件也会更大）

## Sorted tables

sorted table(即.ldb文件)中存储的是根据key排序后的<key, value>，每个<key, value>中的value可以是key真正对应的value值，
也可以是删除的标记，表示该key被删除了，即ValueType { kTypeDeletion = 0x0, kTypeValue = 0x1 }。

删除Key的时候，之所以进行标记，而不是直接删除ldb文件中的<key, value>的原因在于，如果直接删除当前ldb文件中的值，该<key, value>
还有可能存在于其他更老层级的ldb文件中，除非删除的时候遍历所有ldb文件进行删除，这样在读取已删除的Key的时候，也必须遍历所有文件才能知道。

如果采用标记的方式，在删除的时候，直接记录一条<key, {delete marker}>，即可以表示删除完成，直接返回。然后读取的时候，如果读取到
<key, {delete marker}>，表示该值已经删除，可以直接返回，省去遍历所有ldb文件的开销。 然后采用定时合并的方式（Compaction）
来从所有的ldb文件中去除已经删除了的值。

所有的ldb文件会在逻辑上组织成一系列的level，从.log文件中生成的ldb文件称为young level，也称为level-0。当level-0的文件数量达到阈值之后，
就会将所有level-0的文件合并成一个level-1，该阈值当前是4（可以通过kL0_CompactionTrigger参数查看）。所有的level-0的文件和所有level-1
(和level-0的数据可能存储重叠的情况)合并成一系列新的level-1文件，每个level-1的文件

在level-0文件中的keys可能和其他level的keys有重叠，但是，其他level的文件中没有重复的keys。假如level的层级为L（L>=1）,当level-L所有文件
的总大小超过了(10^L)MB，例如level-1就是10MB, level-2就是100MB等，此时会将level-L中的某个文件和level-L+1的所有文件合并成一系列新的
level-L+1的文件列表，在合并的过程中，会通过批量的读、写操作逐步地将更新的数据从低等级level中迁移到高等级的level，从而将寻址的代价最小化。

Note: 由上面的分析也可以看出来，所有的sst文件(也就是ldb文件)创建了之后，文件中的内容就不会改变了，每次的Compaction的时候，都是创建新的文件
来存储合并之后的数据，这就可以将数据的离散写入进行顺序化，从而有效的提升写数据的性能。

### Manifest（重点）

一个Manifest文件中记录了每一个level中所有sst文件（也就是ldb文件）的元数据信息，包含有：
1、sst文件的大小
2、sst文件中key的范围（最大值和最小值）
3、还有一些compaction的统计值，用来控制compaction的过程
4、等

当数据库重新打开的时候，会创建新的Manifest文件，文件名中会有新的编号。Manifest文件是log的文件格式，当有sst文件（ldb文件）创建或删除的时候
Manifest中会追加改动信息。

### Current

CURRENT文件中只记录当前的MANIFEST文件的文件名，表示当前正在使用的MANIFEST文件，因为每次leveldb启动的时候，都会创建一个新的Manifest文件，
因在当前数据库的目录下可能存在多个Manifest文件，因此需要用CURRENT来说明当前使用哪一个MANIFEST文件。

### Info logs

主要是LOG文件或者LOG.old文件，里面记录的是leveldb在进行文件的创建和删除的一些日志信息，例如：
```c++
cat LOG

2021/03/09-11:16:46.747140 0x119fa2dc0 Recovering log #3
2021/03/09-11:16:46.748156 0x119fa2dc0 Level-0 table #5: started
2021/03/09-11:16:46.761671 0x119fa2dc0 Level-0 table #5: 7579 bytes OK
2021/03/09-11:16:46.783852 0x119fa2dc0 Delete type=0 #3
2021/03/09-11:16:46.783884 0x119fa2dc0 Delete type=3 #2
```

 - 1、表示开始恢复log文件，后面的#3表示log文件的ID，即恢复000003.log文件
 - 2、表示开始创建L-0层级的文件，#5表示的是创建的L-0层级的文件名称，即：000005.ldb
 - 3、表示L-0层级的文件创建完成，#5表示的是创建完成的文件ID，后面的7579表示文件的大小，最后的OK表述创建的状态
 - 4、表示删除type=0的文件，根据代码中查看，如下，type=0表示.log文件，后面的#3表示文件的名字为000003.log
 - 5、表示删除type=3的文件，即删除MANIFEST-000002文件

```c++
enum FileType {
  kLogFile,         // 0 LogFileName .log文件
  kDBLockFile,      // 1 LockFileName LOCK文件
  kTableFile,       // 2 TableFileName .sst文件或.ldb文件
  kDescriptorFile,  // 3 DescriptorFileName MANIFEST文件
  kCurrentFile,     // 4 CurrentFileName CURRENT文件
  kTempFile,        // 5 TempFileName .dbtmp文件
  kInfoLogFile      // 6 InfoLogFileName LOG或者LOG.old文件
};
```

### Others

其他的文件用来进行一些标识判断之类的作用，例如：
 - LOCK文件，用来表示当前数据库是否被占用，如果LOCK文件被锁住了，表示已经有进程占用了当前的数据库，否则没有
   可以通过lslocks查看，每个leveldb只能同时被一个客户端打开
 - 还有*.dbtmp等文件

## Level 0

当log文件增长到一定的大小之后（默认是4MB）,会创建新的memtable和log文件用来存放新的数据更新操作，后台会有如下操作：

 - 1、旧的memtable成为immutable memtable
 - 2、将immutable memtable的数据dump写到本地盘sst文件中（或者ldb文件）
 - 3、然后释放immutable memtable中的数据
 - 4、删除旧得log文件和旧的memtable(也就是immutable memtable)
 - 5、将新创建的sst文件（或者ldb文件）添加到young level中（也就是level-0）

## Compactions（最重要且复杂）

Compaction是leveldb最为复杂的过程之一，同样也是leveldb的性能瓶颈，本质上是内部的数据重新整合的机制。

当level-L的文件大小达到上限的时候，会在后台启动一个线程去进行compaction操作，流程是：
 - 1、从L层中选出一个文件，从L+1层中选出所有的文件，其中L层和L+1层的文件存在重合
 - 2、如果L层的文件和L+1层的部分文件存在重叠，此时会将整个L+1层的入口文件和L层选出来的文件进行合并
 - 3、合并完成之后会创建新的sst文件存放合并之后的数据，旧的L+1层的文件会丢弃

另外：
  - 1、当level-L中L >=1的时候，L层内的sst文件之间是不存在重复的key
  - 2、但是level-0是特例，因为level-0内的sst文件之间是会存在key的重叠的
  - 3、因此，在将level-0合并到level-1的时候，会取level-0层的多个文件（而不是一个文件）和level-1层的文件进行合并
  - 4、合并之后的文件产生新的level-1层的文件列表

合并的时候，将L层选出来的文件的内容和L+1中的所有文件的内容进行整合，输出产生新的L+1层的文件，如果当前输出的L+1层的文件达到
目标大小（例如2MB）的时候，就会创建新的L+1层的文件继续存放合并之后的数据。如果当前新产生的文件的key的范围覆盖了L+2层10个
以上的文件，则此时也会产生新的输出文件来存放compaction之后的输出文件，旧的文件将会丢弃掉，然后新的文件开始提供服务。

Compaction触发条件：
  - 1、当level-0层的文件数量超过预订上限时（默认是4个）
  - 2、当level i层文件的总大小超过（10^i）MB的时候
  - 3、当某个文件无效读取次数过多的时候

说明：
  - 1、如果level-0层文件数过多，会导致key重叠的文件数量会很多，因此每次读取数据的时候都会遍历读取所有文件，此时效率比较低， 
      因此为了提高读取的效率，会限制0层文件的个数。
  - 2、因为在level-i层的文件没有key重叠的情况，因此某个key只会在一个sst文件中（可以通过bloom、文件尾部索引等方式判断在哪个文件），
      因此，sst文件本身对读取数据的效率影响不会太大，但是，在compaction的时候，会有较大的IO开销。在0层和1层合并的时候，通常情况下
      0层的key的范围可能会比较大，导致合并成1层的时候，需要和1层中更多的文件进行合并，涉及到更多文件的IO，因此有必要降低每一层的文件
      总数据量的大小。
  - 3、有一种特殊情况，当0层合并到1层的时候，此时1层正好满足compaction要求，此时需要继续将1层的合并到2层，以此类推，由此提出错峰合并：
     - 如何确定错峰的文件？如果某个文件查询次数过多，且查询该文件并不命中key，则是无效查询，即需要错峰合并的文件
     - 什么时候开始错峰？当一个1MB文件无效查询超过25次的时候，便可以开始合并
   
### Timing

Level-0 compactions will read up to four 1MB files from level-0, and at worst
all the level-1 files (10MB). I.e., we will read 14MB and write 14MB.

Other than the special level-0 compactions, we will pick one 2MB file from level
L. In the worst case, this will overlap ~ 12 files from level L+1 (10 because
level-(L+1) is ten times the size of level-L, and another two at the boundaries
since the file ranges at level-L will usually not be aligned with the file
ranges at level-L+1). The compaction will therefore read 26MB and write 26MB.
Assuming a disk IO rate of 100MB/s (ballpark range for modern drives), the worst
compaction cost will be approximately 0.5 second.

If we throttle the background writing to something small, say 10% of the full
100MB/s speed, a compaction may take up to 5 seconds. If the user is writing at
10MB/s, we might build up lots of level-0 files (~50 to hold the 5*10MB). This
may significantly increase the cost of reads due to the overhead of merging more
files together on every read.

Solution 1: To reduce this problem, we might want to increase the log switching
threshold when the number of level-0 files is large. Though the downside is that
the larger this threshold, the more memory we will need to hold the
corresponding memtable.

Solution 2: We might want to decrease write rate artificially when the number of
level-0 files goes up.

Solution 3: We work on reducing the cost of very wide merges. Perhaps most of
the level-0 files will have their blocks sitting uncompressed in the cache and
we will only need to worry about the O(N) complexity in the merging iterator.

### Number of files

Instead of always making 2MB files, we could make larger files for larger levels
to reduce the total file count, though at the expense of more bursty
compactions.  Alternatively, we could shard the set of files into multiple
directories.

An experiment on an ext3 filesystem on Feb 04, 2011 shows the following timings
to do 100K file opens in directories with varying number of files:


| Files in directory | Microseconds to open a file |
|-------------------:|----------------------------:|
|               1000 |                           9 |
|              10000 |                          10 |
|             100000 |                          16 |

So maybe even the sharding is not necessary on modern filesystems?

## Recovery

* Read CURRENT to find name of the latest committed MANIFEST
* Read the named MANIFEST file
* Clean up stale files
* We could open all sstables here, but it is probably better to be lazy...
* Convert log chunk to a new level-0 sstable
* Start directing new writes to a new log file with recovered sequence#

## Garbage collection of files

`RemoveObsoleteFiles()` is called at the end of every compaction and at the end
of recovery. It finds the names of all files in the database. It deletes all log
files that are not the current log file. It deletes all table files that are not
referenced from some level and are not the output of an active compaction.
