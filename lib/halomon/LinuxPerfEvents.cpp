#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
#include <atomic>
#include <thread>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signal.h>

#include <asm/unistd.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Don't include the Linux perf_event.h header because we're using the
// libpfm-provided header instead.
// #include <linux/perf_event.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#include <linux/version.h>

#include "halo/MonitorState.h"
#include "halo/Error.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
  #error "Kernel versions older than 3.4 are incompatible."
#endif

#define IS_POW_TWO(num)  (num) != 0 && (((num) & ((num) - 1)) == 0)


namespace asio = boost::asio;


namespace halo {

void handle_perf_event(Profiler *Prof, perf_event_header *EvtHeader) {

  if (EvtHeader->type == PERF_RECORD_SAMPLE) {
    struct SInfo {
      perf_event_header header;
      uint64_t sample_id;           // PERF_SAMPLE_IDENTIFIER
      uint64_t ip;                  // PERF_SAMPLE_IP
      uint32_t pid, tid;            // PERF_SAMPLE_TID
      uint64_t time;                // PERF_SAMPLE_TIME
      uint64_t addr;                // PERF_SAMPLE_ADDR
      uint64_t stream_id;           // PERF_SAMPLE_STREAM_ID

      // PERF_SAMPLE_CALLCHAIN
      uint64_t nr;
      uint64_t ips[1];        // ips[nr] length array.
    };

    struct SInfo2 {
      // PERF_SAMPLE_BRANCH_STACK
      uint64_t bnr;
      perf_branch_entry lbr[1];     // lbr[bnr] length array.
    };

    // struct SInfo3 {
    //   uint64_t weight;              // PERF_SAMPLE_WEIGHT
    //   perf_mem_data_src data_src;   // PERF_SAMPLE_DATA_SRC
    // };

    SInfo *SI = (SInfo *) EvtHeader;

    // SInfo2 *SI2 = (SInfo2 *) &SI->ips[SI->nr];
    // SInfo3 *SI3 = (SInfo3 *) &SI2->lbr[SI2->bnr];

    auto SampleID = Prof->newSample();

    Prof->recordData1(SampleID, DataKind::InstrPtr, SI->ip);
    Prof->recordData1(SampleID, DataKind::TimeStamp, SI->time);

  } else {
    // std::cout << "some other perf event was encountered.\n";
  }

}

// reads the ring-buffer of perf data from perf_events.
void process_new_samples(Profiler *Prof, uint8_t *EventBuf, size_t EventBufSz, const size_t PageSz) {
  perf_event_mmap_page *Header = (perf_event_mmap_page *) EventBuf;
  uint8_t *DataPtr = EventBuf + PageSz;
  const size_t NumEventBufPages = EventBufSz / PageSz;


  /////////////////////
  // This points to the head of the data section.  The value continu‐
  // ously  increases, it does not wrap.  The value needs to be manu‐
  // ally wrapped by the size of the mmap buffer before accessing the
  // samples.
  //
  // On  SMP-capable  platforms,  after  reading the data_head value,
  // user space should issue an rmb().
  //
  // NOTE -- an rmb is a memory synchronization operation.
  // source: https://community.arm.com/developer/ip-products/processors/b/processors-ip-blog/posts/memory-access-ordering-part-2---barriers-and-the-linux-kernel
  const uint64_t DataHead = Header->data_head;
  __sync_synchronize();

  const uint64_t TailStart = Header->data_tail;



  // Run through the ring buffer and handle the new perf event samples.
  // It's read from Tail --> Head.

  // a contiguous buffer to hold the event data.
  std::vector<uint8_t> TmpBuffer;

  // It's always a power of two size, so we use & instead of % to wrap
  const uint64_t DataPagesSize = (NumEventBufPages - 1)*PageSz;
  const uint64_t DataPagesSizeMask = DataPagesSize - 1;
  assert(IS_POW_TWO(DataPagesSize));

  uint64_t TailProgress = 0;
  while (TailStart + TailProgress != DataHead) {
    uint64_t Offset = (TailStart + TailProgress) & DataPagesSizeMask;
    perf_event_header *BEvtHeader = (perf_event_header*) (DataPtr + Offset);

    uint16_t EvtSz = BEvtHeader->size;
    if (EvtSz == 0)
        break;

    // we copy the data out whether it wraps around or not.
    // TODO: an optimization would be to only copy if wrapping happened.
    TmpBuffer.resize(EvtSz);

    // copy this event's data, stopping at the end of the ring buffer if needed.
    std::copy(DataPtr + Offset,
              DataPtr + std::min(Offset + EvtSz, DataPagesSize),
              TmpBuffer.begin());

    // if the rest of the event's data wrapped around, copy the data
    // from the start of the ring buff onto the end of our temp buffer.
    if (Offset + EvtSz > DataPagesSize) {
      uint64_t ODiff = (Offset + EvtSz) - DataPagesSize;
      std::copy(DataPtr, DataPtr + ODiff,
                TmpBuffer.begin() + (DataPagesSize - Offset));
    }

    handle_perf_event(Prof, (perf_event_header*) TmpBuffer.data());

    TailProgress += EvtSz;

  } // end of ring buffer processing loop

  // done reading the ring buffer.
  // issue a smp_store_release(header.data_tail, current_tail_position)
  __sync_synchronize();
  Header->data_tail = TailStart + TailProgress;
}




////////////////////
// This function enables Linux's perf_events monitoring on the given
// "process/thread" and "cpu" as defined by perf_event_open.
// Please see the manpage for perf_event_open for more details on those args.
//
// The Name corresponds to a string describing the type of event
// to track. The EventPeriod describes how many of that event should occur
// before a sample is provided.
// libpfm allows you to use any valid name as defined in
// `perf list -v` utility itself.
//
// More info:
// - run `perf list -v` in your terminal to get a list of events.
// - http://web.eece.maine.edu/~vweaver/projects/perf_events/generalized_events/
//
// Based on code by Hal Finkel (hfinkel@anl.gov).
// Modified by Kavon Farvardin.
//
int get_perf_events_fd(const std::string &Name,
                       const uint64_t EventPeriod,
                       const pid_t TID,
                       const int CPU,
                       const int NumEventBufPages, // Must be set to (2^n)+1 for n >= 1
                       const int PageSz) {           // the system's page size

  assert(NumEventBufPages >= 3);
  assert(IS_POW_TWO(NumEventBufPages - 1));
  assert(IS_POW_TWO(PageSz));

  perf_event_attr Attr;
  memset(&Attr, 0, sizeof(perf_event_attr));
  Attr.size = sizeof(perf_event_attr);

  pfm_perf_encode_arg_t Arg;
  memset(&Arg, 0, sizeof(pfm_perf_encode_arg_t));
  Arg.size = sizeof(pfm_perf_encode_arg_t);
  Arg.attr = &Attr; // hand the perf_event_attr to libpfm for initalization

  int Ret = pfm_get_os_event_encoding(Name.c_str(), PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &Arg);
  if (Ret != PFM_SUCCESS) {
    std::cerr << "Unable to get event encoding for " << Name << ": " <<
                 pfm_strerror(Ret) << "\n";
    return -1;
  }

  // The disabled bit specifies whether the counter starts  out  dis‐
  // abled  or  enabled.  If disabled, the event can later be enabled
  // by ioctl(2), prctl(2), or enable_on_exec.
  Attr.disabled = 1;

  // These must be set, or else this process would require sudo
  // We only want to know about this process's code.
  Attr.exclude_kernel = 1;
  Attr.exclude_hv = 1;

  // NOTE: A flag to consider -- don't count when CPU is idle.
  // Attr.exclude_idle = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
  // If  use_clockid  is  set, then this field selects which internal
  // Linux timer to use for timestamps.   The  available  timers  are
  // defined   in  linux/time.h,  with  CLOCK_MONOTONIC,  CLOCK_MONO‐
  // TONIC_RAW, CLOCK_REALTIME, CLOCK_BOOTTIME,  and  CLOCK_TAI  cur‐
  // rently supported.
  Attr.use_clockid = 1;
  Attr.clockid = CLOCK_MONOTONIC_RAW;
#endif

  // If this bit is set, then fork/exit notifications are included in
  // the ring buffer.
  Attr.task = 1;

  // The comm bit enables tracking of process command name  as  modi‐
  // fied  by the exec(2) and prctl(PR_SET_NAME) system calls as well
  // as writing to /proc/self/comm.  If the comm_exec  flag  is  also
  // successfully set (possible since Linux 3.16), then the misc flag
  // PERF_RECORD_MISC_COMM_EXEC can  be  used  to  differentiate  the
  // exec(2) case from the others.
  Attr.comm = 1;
  #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
    Attr.comm_exec = 1;
  #endif

  // The mmap bit enables generation of PERF_RECORD_MMAP samples  for
  // every mmap(2) call that has PROT_EXEC set.  This allows tools to
  // notice new executable code being mapped into a program  (dynamic
  // shared  libraries  for  example) so that addresses can be mapped
  // back to the original code.
  Attr.mmap = 1;

  // the period indicates how many events of kind "Name" should happen until
  // a sample is provided.
  Attr.sample_period = EventPeriod;
  Attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_DATA_SRC | PERF_SAMPLE_WEIGHT |
                     PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME |
                     PERF_SAMPLE_TID | PERF_SAMPLE_IDENTIFIER |
                     PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_BRANCH_STACK |
                     PERF_SAMPLE_CALLCHAIN;

  // Note: The callchain is collected in kernel space (and must be
  // collected there, as the context might have changed by the time we see
  // the sample). It is not tied to each sample, however, but collected at
  // interrupt time. If the application was not compiled to preserve frame
  // pointers, etc., then the information might not be complete.
  // Also, if the callchain is truncated, consider increasing:
  // /proc/sys/kernel/perf_event_max_stack
  Attr.exclude_callchain_kernel = 1;

  Attr.wakeup_watermark = (NumEventBufPages-1)*PageSz/2;
  Attr.watermark = 1;

  // 2 = Request no-skid (CPU-sampled events), 1 = Request constant skid.
  Attr.precise_ip = 2;

  // Note: For Intel hardware, these LBR records are only really associated
  // with the PEBS samples starting with Ice Lake, etc.
  Attr.branch_sample_type = PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_ANY;

  // FIXME: For Intel hardware at least, we could also include
  // PERF_SAMPLE_BRANCH_ANY_RETURN along with the calls. For newer Intel
  // hardware, we can use PERF_SAMPLE_BRANCH_CALL_STACK.


  int NewPerfFD = syscall(__NR_perf_event_open, &Attr, TID, CPU, -1, 0);
  if (NewPerfFD == -1) {
    std::cerr << "Unable to open perf events for this process: " << strerror(errno) << "\n";
    return -1;
  }

  return NewPerfFD;
}



///////////
// initializer for Linux perf_events api and related for the monitor.
// returns whether setup was successful.
bool setup_perf_events(int &PerfFD, uint8_t* &EventBuf, size_t &EventBufSz, size_t &PageSz) {
  pid_t MyPID = getpid();
  PageSz = sysconf(_SC_PAGESIZE);
  const int NumBufPages = 8+1;

  int Ret = pfm_initialize();
  if (Ret != PFM_SUCCESS) {
    std::cerr << "Failed to initialize PFM library: " << pfm_strerror(Ret) << "\n";
    return false;
  }

  //////////////
  // open the perf_events file descriptor

  // Here are some large prime numbers to help deter periodicity:
  //
  //   https://primes.utm.edu/lists/small/millions/
  //
  // We want to avoid having as many divisors as possible in case of
  // repetitive behavior, e.g., a long-running loop executing exactly 323
  // instructions per iteration. There's a (slim) chance we sample the
  // same instruction every time because our period is a multiple of 323.
  // In reality, CPUs have noticable non-constant skid, but we don't want to
  // rely on that for good samples.

  std::string EventName = "instructions";
  uint64_t EventPeriod = 67'867'967;

  PerfFD = get_perf_events_fd(EventName, EventPeriod,
                                  MyPID, -1, NumBufPages, PageSz);
  if (PerfFD == -1)
    return false;

  // The mmap size should be 1+2^n pages, where the first page is a metadata
  // page (struct perf_event_mmap_page) that contains various bits of infor‐
  // mation such as where the ring-buffer head is.
  EventBufSz = NumBufPages*PageSz;
  EventBuf = (uint8_t *) mmap(NULL, EventBufSz,
                           PROT_READ|PROT_WRITE, MAP_SHARED, PerfFD, 0);
  if (EventBuf == MAP_FAILED) {
    if (errno == EPERM)
      std::cerr << "Consider increasing /proc/sys/kernel/perf_event_mlock_kb or "
                   "allocating less memory for events buffer.\n";
    std::cerr << "Unable to map perf events pages: " << strerror(errno) << "\n";
    return false;
  }

  // configure the file descriptor
  (void) fcntl(PerfFD, F_SETFL, O_RDWR|O_NONBLOCK|O_ASYNC);
  (void) fcntl(PerfFD, F_SETSIG, SIGIO);
  (void) fcntl(PerfFD, F_SETOWN, MyPID);

  return true;
}



// Since perf_events sends SIGIO signals periodically to notify us
// of new profile data, we need to service these notifications.
// This function directs such signals to the file descriptor.
bool setup_sigio_fd(asio::io_service &PerfSignalService, asio::posix::stream_descriptor &SigSD, int &SigFD) {
  // make SIGIO signals available through a file descriptor instead of interrupts.
  sigset_t SigMask;
  sigemptyset(&SigMask);
  sigaddset(&SigMask, SIGIO);

  if (sigprocmask(SIG_BLOCK, &SigMask, NULL) == -1) {
    std::cerr << "Unable to block signals: " << strerror(errno) << "\n";
    return false;
  }

  SigFD = signalfd(-1, &SigMask, 0);
  if (SigFD == -1) {
    std::cerr << "Unable create signal file handle: " << strerror(errno) << "\n";
    return false;
  }

  // setup to read from the fd.
  SigSD = asio::posix::stream_descriptor(PerfSignalService, SigFD);
  return true;
}



////////////////////////
// Implementation of MonitorState members.



MonitorState::MonitorState() : SigSD(PerfSignalService) {

  // get the path to this process's executable.
  std::vector<char> buf(PATH_MAX);
  ssize_t len = readlink("/proc/self/exe", buf.data(), buf.size()-1);
  if (len == -1) {
    std::cerr << strerror(errno) << "\n";
    fatal_error("path to process's executable not found.");
  }
  buf[len] = '\0'; // null terminate

  Prof = new Profiler(buf.data());

  // setup the monitor's initial state.
  if (!setup_perf_events(PerfFD, EventBuf, EventBufSz, PageSz) ||
      !setup_sigio_fd(PerfSignalService, SigSD, SigFD) ) {
    exit(EXIT_FAILURE);
  }

  ////////////////////
  // finally, send commands to perf_event system to enable sampling
  ioctl(PerfFD, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(PerfFD, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

  // kick-off the chain of async read jobs for the signal file descriptor.
  schedule_signalfd_read();
}

MonitorState::~MonitorState() {
  // clean-up
  delete Prof;

  int ret = munmap(EventBuf, EventBufSz);
  if (ret) {
    std::cerr << "Failed to unmap event buffer: " << strerror(errno) << "\n";
    exit(EXIT_FAILURE);
  }

  ret = close(PerfFD);
  if (ret) {
    std::cerr << "Failed to close perf_event file descriptor: " << strerror(errno) << "\n";
    exit(EXIT_FAILURE);
  }
}

void MonitorState::handle_signalfd_read(const boost::system::error_code &Error, size_t BytesTransferred) {
  bool IOError = false;
  if (Error) {
    std::cerr << "Error reading from signal file handle: " << Error.message() << "\n";
    IOError = true;
  }

  if (BytesTransferred != sizeof(SigFDInfo)) {
    std::cerr << "Read the wrong the number of bytes from the signal file handle: "
                 "read " << BytesTransferred << " bytes\n";
    IOError = true;
  }

  // TODO: convert this into a debug-mode assert.
  if (SigFDInfo.ssi_signo != SIGIO) {
    std::cerr << "Unexpected signal recieved on signal file handle: "
              << SigFDInfo.ssi_signo << "\n";
    IOError = true;
  }


  // SIGIO/SIGPOLL  (the two names are synonyms on Linux) fills in si_band
  //  and si_fd.  The si_band event is a bit mask containing the same  val‐
  //  ues  as  are filled in the revents field by poll(2).  The si_fd field
  //  indicates the file descriptor for which the I/O event  occurred;  for
  //  further details, see the description of F_SETSIG in fcntl(2).
  //
  //  See 'sigaction' man page for more information.



  // TODO: is it actually an error if we get a SIGIO for a different FD?
  // What if the process is doing IO? How do we forward the interrupt to
  // the right place? What should we do?
  if (SigFDInfo.ssi_fd != PerfFD) {
    std::cerr << "Unexpected file descriptor associated with SIGIO interrupt.\n";
    IOError = true;
  }

  // TODO: it's possibly worth checking ssi_code field to find out what in particular
  // is going on in this SIGIO.
  //  The following values can be placed in si_code for a SIGIO/SIGPOLL  signal:
  //
  // POLL_IN
  //        Data input available.
  // .....
  // see 'sigaction' man page

  if (IOError) {
    // stop the service and don't enqueue another read.
    PerfSignalService.stop(); // TODO: is a stop command right if we only poll?
    return;
  }

  process_new_samples(Prof, EventBuf, EventBufSz, PageSz);

  // schedule another read.
  schedule_signalfd_read();
}

void MonitorState::schedule_signalfd_read() {
  // read a signalfd_siginfo from the file descriptor
  asio::async_read(SigSD, asio::buffer(&SigFDInfo, sizeof(SigFDInfo)),
    [&](const boost::system::error_code &Error, size_t BytesTransferred) {
      handle_signalfd_read(Error, BytesTransferred);
    });
}

void MonitorState::poll_for_perf_data() {
  PerfSignalService.poll(); // check for new data
}

} // end namespace halo
