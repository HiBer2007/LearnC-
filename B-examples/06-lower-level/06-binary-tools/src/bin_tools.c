/* bin_tools.c —— 06-binary-tools 的核心实现
 *
 * 编译：由 CMakeLists.txt 编成静态库 core，不直接编译这个文件。
 * 手工编译：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude src/bin_tools.c src/tool_fixtures.c \
 *       src/main_cli.c -o bin_tools.exe
 */
#include "bin_tools.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  define BT_POPEN _popen
#  define BT_PCLOSE _pclose
#else
#  define BT_POPEN popen
#  define BT_PCLOSE pclose
#endif

/* 构建系统填进来的工具路径；没找到时是空串 */
#ifndef BT_OBJDUMP
#  define BT_OBJDUMP ""
#endif
#ifndef BT_NM
#  define BT_NM ""
#endif
#ifndef BT_SIZE
#  define BT_SIZE ""
#endif
#ifndef BT_READELF
#  define BT_READELF ""
#endif

const char *bt_tool_path(const char *name) {
    if (name == NULL) {
        return "";
    }
    if (strcmp(name, "objdump") == 0) {
        return BT_OBJDUMP;
    }
    if (strcmp(name, "nm") == 0) {
        return BT_NM;
    }
    if (strcmp(name, "size") == 0) {
        return BT_SIZE;
    }
    if (strcmp(name, "readelf") == 0) {
        return BT_READELF;
    }
    return "";
}

const char *bt_self_path(void) {
    static char buf[1024];
    static int done = 0;
    if (!done) {
#if defined(_WIN32)
        const DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)sizeof buf);
        buf[(n < sizeof buf) ? (size_t)n : (sizeof buf - 1u)] = '\0';
#else
        snprintf(buf, sizeof buf, "%s", "/proc/self/exe");
#endif
        done = 1;
    }
    return buf;
}

/* ==================== 跑工具 ==================== */

/* 把一段命令行拼起来：每段用双引号包住，避免路径里的空格被拆开。
 * 末尾补 2>&1，让工具的错误信息也进同一个缓冲区——解析失败时要看得到原因。
 *
 * 开头那个 call 不能省。_popen 把整串交给 cmd.exe /c，而 cmd 有一条规矩：
 * 如果 /c 后面第一个字符是引号、且整串的引号多于两个，它会把最外层的一对
 * 引号剥掉。剥掉之后 "prog" "arg" 就变成了 prog" "arg，cmd 直接报
 * 「文件名、目录名或卷标语法不正确」。加一个 call 前缀，第一个字符就不是
 * 引号了，这条规矩也就不会触发。 */
static int build_cmdline(const char *exe, const char *const argv[], char *cmd, size_t cap) {
    size_t used = 0;
    int i;

    if (exe == NULL || exe[0] == '\0') {
        return -1;
    }
    used += (size_t)snprintf(cmd + used, cap - used, "call \"%s\"", exe);
    if (used >= cap) {
        return -1;
    }
    for (i = 0; argv != NULL && argv[i] != NULL; ++i) {
        const int w = snprintf(cmd + used, cap - used, " \"%s\"", argv[i]);
        if (w < 0 || (size_t)w >= cap - used) {
            return -1;
        }
        used += (size_t)w;
    }
    if (used + 6u >= cap) {
        return -1;
    }
    memcpy(cmd + used, " 2>&1", 6u);
    return 0;
}

int bt_run(const char *exe, const char *const argv[], char *out, size_t cap) {
    char cmd[1024];
    FILE *pipe;
    size_t used = 0;

    if (out == NULL || cap == 0u) {
        return -1;
    }
    out[0] = '\0';
    if (build_cmdline(exe, argv, cmd, sizeof cmd) != 0) {
        snprintf(out, cap, "(没有找到工具，或命令行太长)\n");
        return -1;
    }

    pipe = BT_POPEN(cmd, "r");
    if (pipe == NULL) {
        snprintf(out, cap, "(启动失败：%s)\n", cmd);
        return -1;
    }
    while (used + 1u < cap) {
        const size_t n = fread(out + used, 1u, cap - used - 1u, pipe);
        if (n == 0u) {
            break;
        }
        used += n;
    }
    out[used] = '\0';
    (void)BT_PCLOSE(pipe);
    return 0;
}

/* ==================== 通用小工具 ==================== */

/* 判断一行里是不是含某个词（按空白与逗号分词） */
static int line_has_word(const char *line, const char *word) {
    const size_t n = strlen(word);
    const char *p = line;
    while ((p = strstr(p, word)) != NULL) {
        const char before = (p == line) ? ' ' : p[-1];
        const char after = p[n];
        const int ok_before = (before == ' ' || before == '\t' || before == ',');
        const int ok_after = (after == '\0' || after == ' ' || after == '\t' ||
                              after == ',' || after == '\n' || after == '\r');
        if (ok_before && ok_after) {
            return 1;
        }
        p += n;
    }
    return 0;
}

static void trim_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0u && (s[n - 1u] == '\n' || s[n - 1u] == '\r' || s[n - 1u] == ' ')) {
        s[--n] = '\0';
    }
}

/* ==================== objdump -h ==================== */

int bt_parse_objdump_h(const char *text, bt_section *out, int max, int *count) {
    char line[512];
    const char *p = text;
    int n = 0;
    bt_section *cur = NULL;

    *count = 0;
    while (*p != '\0' && n < max) {
        const char *nl = strchr(p, '\n');
        size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof line) {
            len = sizeof line - 1u;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p = (nl != NULL) ? nl + 1 : p + len;

        /* 段头行：以空格开头，第 2 列是名字，后面四个十六进制数 */
        {
            unsigned long long size, vma, lma, off;
            char name[BT_NAME_MAX];
            int idx = 0;
            if (sscanf(line, " %d %31s %llx %llx %llx %llx", &idx, name, &size, &vma, &lma,
                       &off) == 6 &&
                name[0] == '.') {
                bt_section *s = &out[n];
                memset(s, 0, sizeof *s);
                snprintf(s->name, sizeof s->name, "%s", name);
                s->size = size;
                s->vma = vma;
                s->lma = lma;
                s->file_off = off;
                cur = s;
                ++n;
                continue;
            }
        }
        /* 属性行：CONTENTS, ALLOC, LOAD, READONLY, CODE, DATA */
        if (cur != NULL && strstr(line, "ALLOC") != NULL) {
            cur->has_contents = line_has_word(line, "CONTENTS");
            cur->is_alloc = 1;
            cur->is_code = line_has_word(line, "CODE");
            cur->is_readonly = line_has_word(line, "READONLY");
        }
    }
    *count = n;
    return 0;
}

void bt_conclude_sections(FILE *out, const bt_section *s, int n, const char *what) {
    int i;
    unsigned long long flash = 0u;
    unsigned long long ram = 0u;
    fprintf(out, "  段表说的是「每一段放在哪、占多少」：\n");
    for (i = 0; i < n; ++i) {
        if (!s[i].is_alloc) {
            fprintf(out, "    %-16s 大小 %8llu  不占内存（只有文件里的字节）\n", s[i].name,
                    s[i].size);
            continue;
        }
        if (s[i].has_contents) {
            flash += s[i].size;
        } else {
            ram += s[i].size;
        }
        if (s[i].vma != s[i].lma) {
            fprintf(out, "    %-16s 大小 %8llu  运行地址 %08llx  装载地址 %08llx  ← 两套地址\n",
                    s[i].name, s[i].size, s[i].vma, s[i].lma);
        } else {
            fprintf(out, "    %-16s 大小 %8llu  地址 %08llx\n", s[i].name, s[i].size, s[i].vma);
        }
    }
    fprintf(out,
            "  合计：带 CONTENTS（要写进文件）的段共 %llu 字节；只有 ALLOC（只占内存）的段共 %llu 字节。\n",
            flash, ram);
    fprintf(out, "  结论：%s\n\n", what);
}

/* ==================== readelf -S ==================== */

int bt_parse_readelf_sections(const char *text, bt_section *out, int max, int *count) {
    char line[512];
    const char *p = text;
    int n = 0;

    *count = 0;
    while (*p != '\0' && n < max) {
        const char *nl = strchr(p, '\n');
        size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof line) {
            len = sizeof line - 1u;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p = (nl != NULL) ? nl + 1 : p + len;

        /* [ 1] .isr_vector       PROGBITS        08000000 001000 0001e4 00   A  0   0  1 */
        {
            int idx = 0;
            char name[BT_NAME_MAX];
            char type[24];
            unsigned long long addr, off, size;
            int flags_at = 0;
            if (sscanf(line, " [%d] %31s %23s %llx %llx %llx %n", &idx, name, type, &addr, &off,
                       &size, &flags_at) >= 6 &&
                name[0] == '.') {
                bt_section *s = &out[n];
                memset(s, 0, sizeof *s);
                snprintf(s->name, sizeof s->name, "%s", name);
                s->size = size;
                s->vma = addr;
                s->lma = addr;
                s->file_off = off;
                s->is_alloc = (strcmp(type, "NOBITS") != 0) && (addr != 0u);
                s->has_contents = (strcmp(type, "NOBITS") != 0);
                s->is_code = (strstr(line + flags_at, "AX") != NULL) ||
                             (strstr(line + flags_at, "X") != NULL);
                s->is_readonly = (strstr(line + flags_at, "A") != NULL);
                ++n;
            }
        }
    }
    *count = n;
    return 0;
}

/* ==================== nm ==================== */

int bt_parse_nm(const char *text, bt_symbol *out, int max, int *count) {
    char line[512];
    const char *p = text;
    int n = 0;

    *count = 0;
    while (*p != '\0' && n < max) {
        const char *nl = strchr(p, '\n');
        size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof line) {
            len = sizeof line - 1u;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p = (nl != NULL) ? nl + 1 : p + len;
        trim_eol(line);
        if (line[0] == '\0') {
            continue;
        }

        /* 两种常见形态：
         *   0000000140001750 T ll_provider_name     ← 有地址
         *                    U ll_cpp_name           ← 未定义，地址列是空白 */
        {
            char addr[24];
            char type = '?';
            char name[BT_SYM_MAX];
            memset(addr, 0, sizeof addr);
            if (sscanf(line, "%23s %c %127s", addr, &type, name) == 3 &&
                isalpha((unsigned char)type)) {
                bt_symbol *s = &out[n];
                memset(s, 0, sizeof *s);
                snprintf(s->name, sizeof s->name, "%s", name);
                s->type = type;
                s->addr = strtoull(addr, NULL, 16);
                ++n;
                continue;
            }
            /* 没有地址那一列：整行是「类型 + 名字」 */
            if (sscanf(line, " %c %127s", &type, name) == 2 && isalpha((unsigned char)type)) {
                bt_symbol *s = &out[n];
                memset(s, 0, sizeof *s);
                snprintf(s->name, sizeof s->name, "%s", name);
                s->type = type;
                s->addr = 0u;
                ++n;
            }
        }
    }
    *count = n;
    return 0;
}

void bt_classify(const bt_symbol *s, int n, bt_symbol_stats *st) {
    int i;
    memset(st, 0, sizeof *st);
    st->total = n;
    for (i = 0; i < n; ++i) {
        const char t = s[i].type;
        if (islower((unsigned char)t)) {
            ++st->local;
        }
        switch (toupper((unsigned char)t)) {
        case 'T':
            ++st->text;
            break;
        case 'D':
        case 'G':
            ++st->data;
            break;
        case 'B':
            ++st->bss;
            break;
        case 'R':
            ++st->rodata;
            break;
        case 'U':
            ++st->undefined_;
            break;
        case 'W':
        case 'V':
            ++st->weak;
            break;
        default:
            ++st->other;
            break;
        }
    }
}

void bt_conclude_symbols(FILE *out, const bt_symbol_stats *st, const char *what) {
    fprintf(out, "  符号表说的是「这个名字最后落在哪一段」：\n");
    fprintf(out, "    合计 %d 个：代码段 T %d、已初始化数据 D %d、未初始化数据 B %d、\n",
            st->total, st->text, st->data, st->bss);
    fprintf(out, "              只读数据 R %d、未定义 U %d、弱符号 W %d、其它 %d\n", st->rodata,
            st->undefined_, st->weak, st->other);
    fprintf(out, "    其中局部符号（小写字母）%d 个，外部符号 %d 个。\n", st->local,
            st->total - st->local);
    fprintf(out, "  结论：%s\n\n", what);
}

/* ==================== size ==================== */

int bt_parse_size(const char *text, bt_size_row *out, int max, int *count) {
    char line[512];
    const char *p = text;
    int n = 0;
    int header_seen = 0;

    *count = 0;
    while (*p != '\0' && n < max) {
        const char *nl = strchr(p, '\n');
        size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len >= sizeof line) {
            len = sizeof line - 1u;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p = (nl != NULL) ? nl + 1 : p + len;
        trim_eol(line);
        if (line[0] == '\0') {
            continue;
        }
        if (strstr(line, "text") != NULL && strstr(line, "bss") != NULL) {
            header_seen = 1;
            continue;
        }
        if (!header_seen) {
            continue;
        }
        /* 五列数字加文件名；GNU 版与 Berkeley 版列数相同，只是分隔符不同 */
        {
            unsigned long long a, b, c, d = 0u, e = 0u;
            char name[64];
            int got = sscanf(line, " %llu %llu %llu %llu %llx %63s", &a, &b, &c, &d, &e, name);
            if (got == 6) {
                bt_size_row *r = &out[n];
                memset(r, 0, sizeof *r);
                r->text = a;
                r->data = b;
                r->bss = c;
                r->dec = d;
                r->hex = e;
                r->has_dec_hex = 1;
                snprintf(r->name, sizeof r->name, "%s", name);
                ++n;
                continue;
            }
            got = sscanf(line, " %llu %llu %llu %63s", &a, &b, &c, name);
            if (got == 4) { /* Berkeley 格式：只有三列数字加文件名 */
                bt_size_row *r = &out[n];
                memset(r, 0, sizeof *r);
                r->text = a;
                r->data = b;
                r->bss = c;
                r->dec = a + b + c;
                r->hex = r->dec;
                r->has_dec_hex = 0;
                snprintf(r->name, sizeof r->name, "%s", name);
                ++n;
            }
        }
    }
    *count = n;
    return 0;
}

void bt_conclude_size(FILE *out, const bt_size_row *rows, int n, const char *what) {
    int i;
    fprintf(out, "  size 说的是「产物分三段各占多少字节」：\n");
    for (i = 0; i < n; ++i) {
        const unsigned long long total = rows[i].text + rows[i].data + rows[i].bss;
        fprintf(out, "    %-24s text %8llu  data %6llu  bss %6llu  合计 %8llu\n", rows[i].name,
                rows[i].text, rows[i].data, rows[i].bss, total);
        fprintf(out,
                "      写进文件的是 text + data（%llu），上电后占内存的是 data + bss（%llu）。\n",
                rows[i].text + rows[i].data, rows[i].data + rows[i].bss);
    }
    fprintf(out, "  结论：%s\n\n", what);
}

/* ==================== 自测 ==================== */

extern const char bt_fixture_objdump_h[];
extern const char bt_fixture_nm[];
extern const char bt_fixture_size_gnu[];
extern const char bt_fixture_size_multi[];
extern const char bt_fixture_readelf_s[];

static int g_pass;
static int g_fail;

static void check(FILE *out, int ok, const char *what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
    }
    fprintf(out, "  [%s] %d. %s\n", ok ? "通过" : "失败", g_pass + g_fail, what);
}

static void check_skip(FILE *out, const char *what) {
    ++g_pass;
    fprintf(out, "  [跳过] %d. %s\n", g_pass + g_fail, what);
}

static const bt_section *find_section(const bt_section *s, int n, const char *name) {
    int i;
    for (i = 0; i < n; ++i) {
        if (strcmp(s[i].name, name) == 0) {
            return &s[i];
        }
    }
    return NULL;
}

int bt_self_test(FILE *out) {
    g_pass = 0;
    g_fail = 0;
    fprintf(out, "== 自测 ==\n");

    /* --- objdump -h --- */
    {
        bt_section secs[16];
        int n = 0;
        const bt_section *p;
        bt_parse_objdump_h(bt_fixture_objdump_h, secs, 16, &n);
        check(out, n == 5, "objdump -h：从样例里解析出 5 个段");
        p = find_section(secs, n, ".isr_vector");
        check(out, p != NULL && p->vma == 0x08000000ull,
              "objdump -h：.isr_vector 的运行地址是 0x08000000");
        p = find_section(secs, n, ".data");
        check(out, p != NULL && p->vma != p->lma,
              "objdump -h：.data 的运行地址与装载地址不同（两套地址）");
        p = find_section(secs, n, ".data");
        check(out, p != NULL && p->lma == 0x08000ca0ull,
              "objdump -h：.data 的装载地址在 flash 里");
        p = find_section(secs, n, ".bss");
        check(out, p != NULL && p->is_alloc && !p->has_contents,
              "objdump -h：.bss 占内存但没有 CONTENTS（不占文件）");
        p = find_section(secs, n, ".text");
        check(out, p != NULL && p->is_code && p->has_contents,
              "objdump -h：.text 是代码段且占文件");
    }

    /* --- nm --- */
    {
        bt_symbol syms[32];
        bt_symbol_stats st;
        int n = 0;
        bt_parse_nm(bt_fixture_nm, syms, 32, &n);
        bt_classify(syms, n, &st);
        check(out, n == 12, "nm：从样例里解析出 12 个符号");
        check(out, st.text == 4, "nm：代码段符号 T 有 4 个（含一个局部 t 之外的外部符号）");
        check(out, st.undefined_ == 2, "nm：未定义符号 U 有 2 个");
        check(out, st.weak == 1, "nm：弱符号 W 有 1 个");
        check(out, st.data == 1 && st.bss == 1 && st.rodata == 2,
              "nm：数据段符号按字母分到 D / B / R 三类");
        check(out, st.local == 1, "nm：局部符号（小写字母）有 1 个");
    }

    /* --- size：两种格式都要认 --- */
    {
        bt_size_row rows[4];
        int n = 0;
        bt_parse_size(bt_fixture_size_gnu, rows, 4, &n);
        check(out, n == 1 && rows[0].text == 3232u && rows[0].data == 4u && rows[0].bss == 1548u,
              "size：GNU 格式的三列数字都解析对了");
        check(out, n == 1 && rows[0].has_dec_hex && rows[0].dec == 4784u,
              "size：GNU 格式的 dec 列也读到了");

        n = 0;
        bt_parse_size(bt_fixture_size_multi, rows, 4, &n);
        check(out, n == 2 && rows[0].text == 3232u && rows[1].text == 46016u,
              "size：一次给两个文件时，两行都解析出来了（空格对齐）");
        check(out, n == 2 && rows[1].hex == 0xc070u,
              "size：hex 那一列按十六进制读，不会被当成十进制截断");
    }

    /* --- readelf -S --- */
    {
        bt_section secs[16];
        int n = 0;
        const bt_section *p;
        bt_parse_readelf_sections(bt_fixture_readelf_s, secs, 16, &n);
        check(out, n == 5, "readelf -S：从样例里解析出 5 个段（下标 0 那一行没有名字，跳过）");
        p = find_section(secs, n, ".bss");
        check(out, p != NULL && !p->has_contents,
              "readelf -S：.bss 的类型是 NOBITS，因此不占文件");
        p = find_section(secs, n, ".isr_vector");
        check(out, p != NULL && p->vma == 0x08000000ull,
              "readelf -S：.isr_vector 的地址是 0x08000000");
    }

    /* --- 本机的工具 --- */
    {
        const char *objdump = bt_tool_path("objdump");
        const char *nm = bt_tool_path("nm");
        const char *size = bt_tool_path("size");
        if (objdump[0] != '\0' && nm[0] != '\0' && size[0] != '\0') {
            check(out, 1, "构建时找到了 objdump / nm / size 三个工具");
        } else {
            check_skip(out, "构建时没有找齐 objdump / nm / size，下面那两项也一并跳过");
        }

        if (objdump[0] != '\0') {
            char buf[4096];
            const char *argv[3];
            argv[0] = "-h";
            argv[1] = bt_self_path();
            argv[2] = NULL;
            if (bt_run(objdump, argv, buf, sizeof buf) == 0 && strstr(buf, "Sections:") != NULL) {
                check(out, 1, "真的跑一次 objdump -h，并拿到了段表");
            } else {
                check(out, 0, "真的跑一次 objdump -h，并拿到了段表");
            }
        } else {
            check_skip(out, "真的跑一次 objdump -h（没有找到 objdump）");
        }
    }

    fprintf(out, "\n  自测结果：%d 项中 %d 项通过", g_pass + g_fail, g_pass);
    if (g_fail == 0) {
        fprintf(out, "，全部通过\n");
    } else {
        fprintf(out, "，%d 项失败\n", g_fail);
    }
    return g_fail;
}
