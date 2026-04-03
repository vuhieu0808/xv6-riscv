/*
 * sysinfotest.c — verify chính xác cho system call sysinfo trên xv6-riscv
 *
 * Cách dùng: thêm file này vào thư mục user/, thêm $U/_sysinfotest vào
 * UPROGS trong Makefile, rồi chạy "sysinfotest" trong xv6 shell.
 *
 * Các test được kiểm tra:
 *   1. freemem — phản ánh đúng khi dùng sbrk()
 *   2. nproc   — phản ánh đúng khi fork() / wait()
 *   3. nopenfiles — phản ánh đúng khi open() / close()
 *   4. combined   — fork + open + sbrk đồng thời
 *   5. badptr     — trả về -1 khi pointer không hợp lệ
 */

#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

/* ------------------------------------------------------------------ */
/* Tiện ích in kết quả                                                  */
/* ------------------------------------------------------------------ */

static int tests_run   = 0;
static int tests_passed = 0;

static void
pass(const char *name)
{
    printf("[PASS] %s\n", name);
    tests_run++;
    tests_passed++;
}

static void
fail(const char *name, const char *reason)
{
    printf("[FAIL] %s — %s\n", name, reason);
    tests_run++;
}

static int
get_info(struct sysinfo *si)
{
    if (sysinfo(si) < 0) {
        printf("sysinfo() returned error\n");
        return -1;
    }
    return 0;
}

/* In ra su khac nhau giua hai lan goi sysinfo.
 * Chi dung %d, %s -- cac format xv6 printf ho tro.
 */
static void
print_diff(const char *label_before, const struct sysinfo *before,
           const char *label_after,  const struct sysinfo *after)
{
    int d_mem   = (int)after->freemem    - (int)before->freemem;
    int d_proc  = (int)after->nproc      - (int)before->nproc;
    int d_files = (int)after->nopenfiles - (int)before->nopenfiles;

    printf("  [%s] -> [%s]\n", label_before, label_after);
    printf("    freemem:    %d -> %d  (delta: %d)\n",
           (int)before->freemem,    (int)after->freemem,    d_mem);
    printf("    nproc:      %d -> %d  (delta: %d)\n",
           (int)before->nproc,      (int)after->nproc,      d_proc);
    printf("    nopenfiles: %d -> %d  (delta: %d)\n",
           (int)before->nopenfiles, (int)after->nopenfiles, d_files);
}

/* ------------------------------------------------------------------ */
/* Test 1: freemem giảm khi cấp phát bộ nhớ qua sbrk()                */
/* ------------------------------------------------------------------ */
static void
test_freemem_decreases(void)
{
    const char *name = "freemem_decreases";
    struct sysinfo before, after;

    if (get_info(&before) < 0) { fail(name, "sysinfo failed"); return; }

    /* Cấp phát 1 page (4096 bytes) */
    char *p = sbrk(4096);
    if (p == (char *)-1) { fail(name, "sbrk failed"); return; }

    if (get_info(&after) < 0) { fail(name, "sysinfo failed"); return; }

    /* Giải phóng lại để không ảnh hưởng test sau */
    sbrk(-4096);

    print_diff("before sbrk", &before, "after sbrk(4096)", &after);

    if (after.freemem >= before.freemem) {
        fail(name, "freemem did not decrease after sbrk(4096)");
        return;
    }
    if (before.freemem - after.freemem < 4096) {
        printf("  drop=%d (expected >=4096)\n",
               (int)(before.freemem - after.freemem));
        fail(name, "freemem decreased by less than 4096");
        return;
    }
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 2: freemem tăng lại sau khi giải phóng                         */
/* ------------------------------------------------------------------ */
static void
test_freemem_increases_after_free(void)
{
    const char *name = "freemem_increases_after_free";
    struct sysinfo before, after_alloc, after_free;
    const int PAGES = 8;
    const int SZ    = PAGES * 4096;

    if (get_info(&before) < 0) { fail(name, "sysinfo failed"); return; }

    char *p = sbrk(SZ);
    if (p == (char *)-1) { fail(name, "sbrk alloc failed"); return; }

    if (get_info(&after_alloc) < 0) { fail(name, "sysinfo failed"); return; }

    sbrk(-SZ);

    if (get_info(&after_free) < 0) { fail(name, "sysinfo failed"); return; }

    print_diff("before", &before, "after alloc", &after_alloc);
    print_diff("after alloc", &after_alloc, "after free", &after_free);

    if (after_free.freemem <= after_alloc.freemem) {
        fail(name, "freemem did not increase after freeing memory");
        return;
    }
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 3: nproc tăng khi fork()                                        */
/* ------------------------------------------------------------------ */
static void
test_nproc_increases_on_fork(void)
{
    const char *name = "nproc_increases_on_fork";
    struct sysinfo before, during;
    int pid;

    if (get_info(&before) < 0) { fail(name, "sysinfo failed"); return; }

    pid = fork();
    if (pid < 0) { fail(name, "fork failed"); return; }

    if (pid == 0) {
        /* Child: chỉ pause rồi thoát — không cần test ở đây */
        pause(10);
        exit(0);
    }

    /* Parent: lấy thông tin ngay sau khi fork */
    pause(1); /* Chờ một chút để child bắt đầu chạy */
    if (get_info(&during) < 0) {
        kill(pid);
        wait(0);
        fail(name, "sysinfo failed");
        return;
    }

    kill(pid);
    wait(0);

    if (during.nproc <= before.nproc) {
        print_diff("before fork", &before, "after fork", &during);
        fail(name, "nproc did not increase after fork");
        return;
    }
    print_diff("before fork", &before, "after fork", &during);
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 4: nproc giảm sau khi child exit + wait()                      */
/* ------------------------------------------------------------------ */
static void
test_nproc_decreases_after_wait(void)
{
    const char *name = "nproc_decreases_after_wait";
    struct sysinfo before, after;
    int pid;

    pid = fork();
    if (pid < 0) { fail(name, "fork failed"); return; }

    if (pid == 0) {
        exit(0);
    }

    wait(0); /* Thu hồi child xong mới lấy thông tin */

    if (get_info(&before) < 0) { fail(name, "sysinfo after wait failed"); return; }

    /* Spawn thêm một child, đợi nó thoát, rồi so sánh */
    pid = fork();
    if (pid < 0) { fail(name, "second fork failed"); return; }
    if (pid == 0) { exit(0); }

    /* Lấy thông tin KHI child đang chạy */
    struct sysinfo during;
    pause(1);
    if (get_info(&during) < 0) { wait(0); fail(name, "sysinfo during failed"); return; }

    wait(0);

    /* after_wait */
    if (get_info(&after) < 0) { fail(name, "sysinfo after 2nd wait failed"); return; }

    if (after.nproc >= during.nproc) {
        print_diff("child running", &during, "after wait", &after);
        fail(name, "nproc did not decrease after child exited");
        return;
    }
    print_diff("child running", &during, "after wait", &after);
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 5: nopenfiles tăng khi open()                                   */
/* ------------------------------------------------------------------ */
static void
test_nopenfiles_increases_on_open(void)
{
    const char *name = "nopenfiles_increases_on_open";
    struct sysinfo before, after;
    int fd;

    if (get_info(&before) < 0) { fail(name, "sysinfo failed"); return; }

    /* Mở file hệ thống (console luôn tồn tại trên xv6) */
    fd = open("console", 0 /* O_RDONLY */);
    if (fd < 0) {
        /* Fallback: thử tạo file tạm */
        fd = open("__tmp_sysinfo_test__", 0x200 /* O_CREATE|O_RDWR */);
        if (fd < 0) { fail(name, "open failed"); return; }
    }

    if (get_info(&after) < 0) {
        close(fd);
        fail(name, "sysinfo after open failed");
        return;
    }

    close(fd);
    unlink("__tmp_sysinfo_test__");

    if (after.nopenfiles <= before.nopenfiles) {
        print_diff("before open", &before, "after open", &after);
        fail(name, "nopenfiles did not increase after open()");
        return;
    }
    print_diff("before open", &before, "after open", &after);
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 6: nopenfiles giảm khi close()                                  */
/* ------------------------------------------------------------------ */
static void
test_nopenfiles_decreases_on_close(void)
{
    const char *name = "nopenfiles_decreases_on_close";
    struct sysinfo open_si, closed_si;
    int fd;

    /* Mở file trước rồi mới lấy baseline */
    fd = open("console", 0);
    if (fd < 0) {
        fd = open("__tmp_close_test__", 0x200);
        if (fd < 0) { fail(name, "open failed"); return; }
    }

    if (get_info(&open_si) < 0) {
        close(fd);
        fail(name, "sysinfo while open failed");
        return;
    }

    close(fd);
    unlink("__tmp_close_test__");

    if (get_info(&closed_si) < 0) { fail(name, "sysinfo after close failed"); return; }

    if (closed_si.nopenfiles >= open_si.nopenfiles) {
        print_diff("while open", &open_si, "after close", &closed_si);
        fail(name, "nopenfiles did not decrease after close()");
        return;
    }
    print_diff("while open", &open_si, "after close", &closed_si);
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 7: Mở nhiều file — nopenfiles tăng đúng N                      */
/* ------------------------------------------------------------------ */
static void
test_nopenfiles_batch(void)
{
    const char *name = "nopenfiles_batch";
    const int N = 5;
    int fds[N];
    struct sysinfo before, after;
    char fname[32];
    int opened = 0;

    if (get_info(&before) < 0) { fail(name, "sysinfo failed"); return; }

    for (int i = 0; i < N; i++) {
        fname[0] = '_'; fname[1] = '_'; fname[2] = 'b';
        fname[3] = '0' + i; fname[4] = '_'; fname[5] = '_'; fname[6] = '\0';
        fds[i] = open(fname, 0x200 /* O_CREATE|O_RDWR */);
        if (fds[i] >= 0) opened++;
    }

    if (get_info(&after) < 0) {
        for (int i = 0; i < N; i++) if (fds[i] >= 0) close(fds[i]);
        fail(name, "sysinfo failed");
        return;
    }

    for (int i = 0; i < N; i++) {
        if (fds[i] >= 0) {
            close(fds[i]);
            fname[3] = '0' + i;
            unlink(fname);
        }
    }

    if ((int)(after.nopenfiles - before.nopenfiles) != opened) {
        print_diff("before open x5", &before, "after open x5", &after);
        printf("  expected +%d, got +%d (opened=%d)\n",
               opened, (int)(after.nopenfiles - before.nopenfiles), opened);
        fail(name, "nopenfiles did not increase by expected amount");
        return;
    }
    print_diff("before open x5", &before, "after open x5", &after);
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 8: Bad pointer — sysinfo phải trả về -1                        */
/* ------------------------------------------------------------------ */
static void
test_bad_pointer(void)
{
    const char *name = "bad_pointer";

    /* Địa chỉ kernel space — user không được phép truy cập */
    uint64 bad = 0xFFFFFFFFFFFF0000ULL;
    if (sysinfo((struct sysinfo *)bad) != -1) {
        fail(name, "sysinfo with kernel-space pointer did not return -1");
        return;
    }
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 9: NULL pointer — sysinfo phải trả về -1                       */
/* ------------------------------------------------------------------ */
static void
test_null_pointer(void)
{
    const char *name = "null_pointer";
    if (sysinfo(0) != -1) {
        fail(name, "sysinfo(NULL) did not return -1");
        return;
    }
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 10: freemem luôn là bội số của page size (4096)                */
/* ------------------------------------------------------------------ */
static void
test_freemem_page_aligned(void)
{
    const char *name = "freemem_page_aligned";
    struct sysinfo si;
    if (get_info(&si) < 0) { fail(name, "sysinfo failed"); return; }
    if (si.freemem % 4096 != 0) {
        printf("  freemem=%d (not multiple of 4096)\n", (int)si.freemem);
        fail(name, "freemem is not a multiple of PGSIZE");
        return;
    }
    pass(name);
}

/* ------------------------------------------------------------------ */
/* Test 11: Combined — fork + open + sbrk cùng lúc                     */
/* ------------------------------------------------------------------ */
static void
test_combined(void)
{
    const char *name = "combined_stress";
    struct sysinfo s0, s1;
    int pid, fd;

    if (get_info(&s0) < 0) { fail(name, "sysinfo s0 failed"); return; }

    /* Cấp phát thêm memory */
    char *mem = sbrk(8192);
    if (mem == (char *)-1) { fail(name, "sbrk failed"); return; }

    /* Mở thêm một file */
    fd = open("console", 0);
    int fd_ok = (fd >= 0);

    /* Fork một child */
    pid = fork();
    if (pid < 0) {
        sbrk(-8192);
        if (fd_ok) close(fd);
        fail(name, "fork failed");
        return;
    }

    if (pid == 0) {
        pause(5);
        exit(0);
    }

    pause(1);
    if (get_info(&s1) < 0) {
        kill(pid); wait(0);
        sbrk(-8192);
        if (fd_ok) close(fd);
        fail(name, "sysinfo s1 failed");
        return;
    }

    kill(pid); wait(0);
    sbrk(-8192);
    if (fd_ok) close(fd);

    print_diff("before", &s0, "after fork+open+sbrk", &s1);

    int ok = 1;
    if (s1.freemem >= s0.freemem) {
        ok = 0;
    }
    if (s1.nproc <= s0.nproc) {
        ok = 0;
    }
    if (fd_ok && s1.nopenfiles <= s0.nopenfiles) {
        ok = 0;
    }

    if (ok) pass(name);
    else    fail(name, "one or more fields incorrect in combined test");
}

/* ------------------------------------------------------------------ */
/* main                                                                  */
/* ------------------------------------------------------------------ */
int
main(void)
{
    printf("\n=== sysinfotest ===\n\n");

    /* --- freemem tests --- */
    printf("-- freemem --\n");
    test_freemem_decreases();
    test_freemem_increases_after_free();
    test_freemem_page_aligned();

    /* --- nproc tests --- */
    printf("\n-- nproc --\n");
    test_nproc_increases_on_fork();
    test_nproc_decreases_after_wait();

    /* --- nopenfiles tests --- */
    printf("\n-- nopenfiles --\n");
    test_nopenfiles_increases_on_open();
    test_nopenfiles_decreases_on_close();
    test_nopenfiles_batch();

    /* --- error handling --- */
    printf("\n-- error handling --\n");
    test_bad_pointer();
    test_null_pointer();

    /* --- combined --- */
    printf("\n-- combined --\n");
    test_combined();

    /* --- summary --- */
    printf("\n=== result: %d/%d passed ===\n\n", tests_passed, tests_run);

    if (tests_passed == tests_run)
        exit(0);
    else
        exit(1);
}