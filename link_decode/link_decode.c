/*
 * Copyright (c) 2020 Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Windows shell link decoder.
 */

#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>

#include <shlobj.h>

#define HEADER_LEN	0x0000004cLU

#define HAS_LINK_TARGET_ID_LIST	0x00000001
#define HAS_LINK_INFO		0x00000002
#define HAS_NAME		0x00000004
#define HAS_RELATIVE_PATH	0x00000008
#define HAS_WORKING_DIR		0x00000010
#define HAS_ARGUMENTS		0x00000020
#define HAS_ICON_LOCATION	0x00000040
#define IS_UNICODE		0x00000080

typedef struct {
    unsigned long flag;
    const char *name;
} flag_t;

char *me;
bool verbose = false;

static flag_t link_flags[] = {
    { HAS_LINK_TARGET_ID_LIST, "HasLinkTargetIDList" },
    { HAS_LINK_INFO, "HasLinkInfo" },
    { HAS_NAME, "HasName" },
    { HAS_RELATIVE_PATH, "HasRelativePath" },
    { HAS_WORKING_DIR, "HasWorkingDir" },
    { HAS_ARGUMENTS, "HasArguments" },
    { HAS_ICON_LOCATION, "HasIconLocation" },
    { IS_UNICODE, "IsUnicode" },
    { 0x00000100, "ForceNoLinkInfo" },
    { 0x00000200, "HasExpString" },
    { 0x00000400, "RunInSeparateProcess" },
    { 0x00001000, "HasDarwinID" },
    { 0x00002000, "RunAsUser" },
    { 0x00004000, "HasExpIcon" },
    { 0x00008000, "NoPidlAlias" },
    { 0x00020000, "RunWithShimLayer" },
    { 0x00040000, "ForceNoLinkTrack" },
    { 0x00080000, "EnableTargetMetadata" },
    { 0x00100000, "DisableLinkPathTracking" },
    { 0x00200000, "DisableKnownFolderTracking" },
    { 0x00400000, "DisableKnownFolderAlias" },
    { 0x00800000, "AllowLinkToLink" },
    { 0x01000000, "UnaliasOnSave" },
    { 0x02000000, "PreferEnvironmentPath" },
    { 0x04000000, "KeepLocalIDListForUNCTarget" },
    { 0, NULL }
};

static flag_t file_attributes_flags[] = {
    { 0x00000001, "ReadOnly" },
    { 0x00000002, "Hidden" },
    { 0x00000004, "System" },
    { 0x00000010, "Directory" },
    { 0x00000020, "Archive" },
    { 0x00000080, "Normal" },
    { 0x00000100, "Temporary" },
    { 0x00000200, "SparseFile" },
    { 0x00000400, "ReparsePoint" },
    { 0x00000800, "Compressed" },
    { 0x00001000, "Offline" },
    { 0x00002000, "NotContentIndexed" },
    { 0x00004000, "Encrypted" },
    { 0, NULL }
};

static flag_t show_command_enum[] = {
    { 0x00000001, "Normal" },
    { 0x00000003, "Maximized" },
    { 0x00000007, "MinNoActive" },
    { 0, NULL },
};

static flag_t hot_key_flags[] = {
    { 0x00000001, "Shift" },
    { 0x00000002, "Ctrl" },
    { 0x00000004, "Alt" },
    { 0, NULL }
};

static flag_t extra_enum[] = {
    { NT_CONSOLE_PROPS_SIG, "NT_CONSOLE_PROPS" },
    { NT_FE_CONSOLE_PROPS_SIG, "NT_FE_CONSOLE_PROPS" },
    { EXP_DARWIN_ID_SIG, "EXP_DARWIN_ID" },
    { EXP_LOGO3_ID_SIG, "EXP_LOGO3_ID" },
    { EXP_SPECIAL_FOLDER_SIG, "EXP_SPECIAL_FOLDER" },
    { EXP_SZ_LINK_SIG, "EXP_SZ_LINK" },
    { EXP_SZ_ICON_SIG, "EXP_SZ_ICON" },
    { 0xa0000003, "Tracker" },
    { 0xa0000009, "PropertyStore" },
    { 0xa000000c, "VistaAndAboveIdList" },
    { 0, NULL }
};

static unsigned long read_length(int fd);
static void read_data(int fd, void *buf, size_t len);
static void decode_header(int fd);
static void decode_idlist(int fd);
static void decode_link_info(int fd);
static void decode_string(int fd, char *name, bool is_unicode);
static void decode_extra_data(int fd);

static void
usage(void)
{
    fprintf(stderr, "usage: %s [-v] linkfile\n", me);
    exit(1);
}

static void *
Malloc(size_t len)
{
    void *ret = malloc(len);

    if (ret == NULL) {
	fprintf(stderr, "Out of memory\n");
	exit(1);
    }
    return ret;
}

static void
Free(void *buf)
{
    free(buf);
}

int
main(int argc, char *argv[])
{
    int c;
    int fd;
    char *filename;
    unsigned long len;

    /* Parse command-line arguments */
    if ((me = strrchr(argv[0], '\\')) != NULL) {
	    me++;
    } else {
	    me = argv[0];
    }

    while ((c = getopt(argc, argv, "v")) != -1) {
	switch (c) {
	case 'v':
	    verbose = true;
	    break;
	default:
	    usage();
	}
    }

    if (argc - optind != 1) {
	usage();
    }

    /* Open the file. */
    filename = argv[optind];
    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0) {
	perror(filename);
	exit(1);
    }

    if (verbose) {
	printf("[Verbose mode]\n");
    }
    len = read_length(fd);
    if (len != HEADER_LEN) {
	fprintf(stderr, "Wrong header size 0x%08lx, expected 0x%08lx\n",
		len, HEADER_LEN);
	exit(1);
    }
    decode_header(fd);

    close(fd);
    return 0;
}

/* Decode a 32-bit field. */
static unsigned long
decode_long(void *buf)
{
    unsigned char *c = buf;

    return (c[3] << 24) | (c[2] << 16) | (c[1] << 8) | c[0];
}

/* Decode a 16-bit field. */
static unsigned short
decode_short(void *buf)
{
    unsigned char *c = buf;

    return (c[1] << 8) | c[0];
}

/* Decode a 16-bit big-endian field. */
static unsigned short
decode_short_be(void *buf)
{
    unsigned char *c = buf;

    return (c[0] << 8) | c[1];
}

/* Read a length field from the file. */
static unsigned long
read_length(int fd)
{
    unsigned char buf[4];

    read_data(fd, buf, 4);
    return decode_long(buf);
}

static void
decode_flags(unsigned long value, flag_t *flags)
{
    printf("0x%08lx", value);
    while (flags->flag) {
	if (value & flags->flag) {
	    printf(" %s", flags->name);
	    value &= ~flags->flag;
	}
	flags++;
    }
    if (value) {
	printf(" 0x%08lx", value);
    }
}

static void
decode_enum(unsigned long value, flag_t *flags)
{
    printf("0x%08lx", value);
    while (flags->name != NULL) {
	if (value == flags->flag) {
	    printf(" %s", flags->name);
	    return;
	}
	flags++;
    }
}

static void
decode_file_time(void *buf)
{
    SYSTEMTIME sys;

    if (!FileTimeToSystemTime(buf, &sys)) {
	printf("err");
    }
    printf("%04d-%02d-%02d %02d:%02d:%02d.%03d",
	    sys.wYear,
	    sys.wMonth,
	    sys.wDay,
	    sys.wHour,
	    sys.wMinute,
	    sys.wSecond,
	    sys.wMilliseconds);
}

/* Read data from the file. */
static void
read_data(int fd, void *buf, size_t len)
{
    static size_t offset = 0;
    size_t nr;

    if (verbose) {
	printf("[Reading %ld (0x%lx) bytes at offset 0x%lx]\n", (long)len,
		(long)len, (long)offset);
    }
    nr = read(fd, buf, len);
    if (nr < 0) {
	perror("File read");
	exit(1);
    }
    if (nr < len) {
	fprintf(stderr, "Short read, wanted %ld, got %ld\n",
		(long)len, (long)nr);
	exit(1);
    }
    offset += len;
}

/* Decode the header. */
static void
decode_header(int fd)
{
    unsigned char buf[HEADER_LEN - 4];
    int i;
    unsigned long link_flags_value;
    bool is_unicode;
    unsigned long file_attributes_flags_value;
    unsigned long file_size;
    unsigned long icon_index;
    unsigned long show_command;
    unsigned short hot_key;

    printf("ShellLink header:\n");
    read_data(fd, buf, HEADER_LEN - 4);

    /* Decode the fixed-length fields. */
    printf(" CLSID %08lx-%04x-%04x-%04x-",
	    decode_long(buf),
	    decode_short(buf + 4),
	    decode_short(buf + 6),
	    decode_short_be(buf + 8));
    for (i = 0; i < 6; i++) {
	printf("%02x", buf[10 + i]);
    }
    printf("\n");

    link_flags_value = decode_long(buf + 16);
    is_unicode = (link_flags_value & IS_UNICODE) != 0;
    printf(" Link flags ");
    decode_flags(link_flags_value, link_flags);
    printf("\n");
    file_attributes_flags_value = decode_long(buf + 20);
    printf(" File attribute flags ");
    decode_flags(file_attributes_flags_value, file_attributes_flags);
    printf("\n");

    printf(" Creation time ");
    decode_file_time(buf + 24);
    printf("\n Access time ");
    decode_file_time(buf + 32);
    printf("\n Write time ");
    decode_file_time(buf + 40);
    printf("\n");

    file_size = decode_long(buf + 48);
    printf(" File size %ld\n", file_size);
    icon_index = decode_long(buf + 52);
    printf(" Icon index %ld\n", icon_index);

    show_command = decode_long(buf + 56);
    printf(" Show command ");
    decode_enum(show_command, show_command_enum);
    printf("\n");

    hot_key = decode_short(buf + 60);
    printf(" Hot key 0x%02x ", hot_key & 0xff);
    if (hot_key) {
	decode_flags((hot_key & 0xff00) >> 8, hot_key_flags);
    }
    printf("\n");

    /* Decode the target ID list. */
    if (link_flags_value & HAS_LINK_TARGET_ID_LIST) {
	decode_idlist(fd);
    }

    /* Decode the link info. */
    if (link_flags_value & HAS_LINK_INFO) {
	decode_link_info(fd);
    }

    /* Decode the strings. */
    if (link_flags_value & HAS_NAME) {
	decode_string(fd, "Name", is_unicode);
    }
    if (link_flags_value & HAS_RELATIVE_PATH) {
	decode_string(fd, "RelativePath", is_unicode);
    }
    if (link_flags_value & HAS_WORKING_DIR) {
	decode_string(fd, "WorkingDir", is_unicode);
    }
    if (link_flags_value & HAS_ARGUMENTS) {
	decode_string(fd, "Arguments", is_unicode);
    }
    if (link_flags_value & HAS_ICON_LOCATION) {
	decode_string(fd, "IconLocation", is_unicode);
    }

    /* Decode the extra data. */
    decode_extra_data(fd);
}

/* Decode an ID list. */
static void
decode_idlist(int fd)
{
    unsigned short len;
    char len_buf[2];
    unsigned char *buf;
    int offset = 0;
    unsigned short item_len;
    int i;

    printf("Target ID list:\n");
    read_data(fd, len_buf, 2);
    len = decode_short(len_buf);
    buf = Malloc(len);
    read_data(fd, buf, len);

    while (true) {
	item_len = decode_short(buf + offset);
	printf(" Length %u", item_len);
	if (item_len == 0) {
	    printf("\n");
	    break;
	}
	if (verbose) {
	    printf(" data ");
	    for (i = 0; i < item_len; i++) {
		printf("%02x", *(buf + offset + i));
	    }
	    printf(" ");
	    for (i = 0; i < item_len; i++) {
		unsigned char c = *(buf + offset + i);

		if (c > 0x20 && c < 0x7f) {
		    printf("%c", (char)c);
		} else {
		    printf(".");
		}
	    }
	}
	printf("\n");
	offset += item_len;
	if (offset > len) {
	    fprintf(stderr, "Oops\n");
	}
    }

    Free(buf);
}

/* Decode a link info. */
static void
decode_link_info(int fd)
{
    unsigned long len;
    char len_buf[4];
    unsigned char *buf;
    int i;

    printf("Link info:\n");
    read_data(fd, len_buf, 4);
    len = decode_long(len_buf);
    len -= 4; /* length is inclusive */
    buf = Malloc(len);
    read_data(fd, buf, len);

    /* TODO: Actually decode it. */
    if (verbose) {
	printf(" ");
	for (i = 0; i < len; i++) {
	    printf("%02x", *(buf + i));
	}
	printf("\n ");
	for (i = 0; i < len; i++) {
	    unsigned char c = *(buf + i);

	    if (c > 0x20 && c < 0x7f) {
		printf("%c", (char)c);
	    } else {
		printf(".");
	    }
	}
	printf("\n");
    }

    Free(buf);
}

/* Decode a string. */
static void
decode_string(int fd, char *name, bool is_unicode)
{
    unsigned short len;
    unsigned short buf_len;
    char len_buf[2];
    unsigned char *buf;

    read_data(fd, len_buf, 2);
    len = decode_short(len_buf);
    if (verbose) {
	printf("[String len is %d]\n", len);
    }
    if (is_unicode) {
	buf_len = len * 2;
    } else {
	buf_len = len;
    }
    buf = Malloc(buf_len);
    read_data(fd, buf, buf_len);
    printf("%s: ", name);
    if (is_unicode) {
	int nc;
	char *mb = Malloc(len);

	nc = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)buf, len, mb, len, "?",
		NULL);
	if (nc > 0) {
	    printf("%.*s", nc, mb);
	}
	Free(mb);
    } else {
	printf("%.*s", len, buf);
    }
    printf("\n");
    Free(buf);
}

/* Decode the extra data. */
static void
decode_extra_data(int fd)
{
    while (true) {
	unsigned long len;
	char len_buf[4];
	unsigned char *buf;
	unsigned long signature;

	read_data(fd, len_buf, 4);
	len = decode_long(len_buf);
	if (verbose) {
	    printf("[Block size is %ld]\n", len);
	}
	if (len < 0x0000004) {
	    return;
	}
	len -= 4; /* length is inclusive */
	buf = Malloc(len);
	read_data(fd, buf, len);
	signature = decode_long(buf);
	printf("Extra data, type ");
	decode_enum(signature, extra_enum);
	printf("\n");

	Free(buf);
    }
}
