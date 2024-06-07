/**
 * MIT License
 *
 * Copyright (C) 2023-2024 Carsten Janssen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#include <io.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* strtol() with error checks */
long xstrtol(const char *str, int base)
{
    char *endptr = NULL;

    errno = 0;
    long l = strtol(str, &endptr, base);

    if (errno != 0) {
        perror("strtol()");
        exit(1);
    }

    if (*endptr != 0) {
        fprintf(stderr, "error: argument with invalid characters: ");

        if (isprint(*endptr)) {
            fprintf(stderr, "`%c'\n", *endptr);
        } else {
            fprintf(stderr, "%x\n", *endptr);
        }
        exit(1);
    }

    return l;
}

/* string to unsigned char */
uint8_t strtouchar(const char *str, int base)
{
    long l = xstrtol(str, base);

    if (l < 0 || l > 255) {
        fprintf(stderr, "error: argument value outside of 8 bit range: %ld\n", l);
        exit(1);
    }

    return (uint8_t)l;
}

/* xstrtol() wrapper */
long get_long(const char *str)
{
    const size_t len = strlen(str);

    if (len > 2 && (*str == '0' || *str == '\\') &&
        (str[1] == 'x' || str[1] == 'X'))
    {
        /* hexadecimal number */
        char *copy = strdup(str);
        copy[0] = '0'; /* turn "\x" prefix into "0x" */

        long l = xstrtol(copy, 16);
        free(copy);

        return l;
    } else if (len > 1 && str[0] == '0') {
        /* octal number */
        return xstrtol(str, 8);
    }

    /* decimal number */
    return xstrtol(str, 10);
}

/* read data and print as hex digits on screen */
void read_data(const char *arg_offset, const char *arg_length, const char *file)
{
    long offset, len;

    /* open file */
    FILE *fp = fopen(file, "rb");

    if (!fp) {
        perror("fopen()");
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);

    if (strcasecmp(arg_offset, "append") == 0) {
        offset = fsize;
    } else {
        offset = get_long(arg_offset);
    }

    /* check offset and filesize */
    if (offset >= fsize) {
        fprintf(stderr, "error: offset equals or exceeds filesize\n");
        fclose(fp);
        exit(1);
    }

    /* seek to offset */
    if (fseek(fp, offset, SEEK_SET) == -1) {
        perror("fseek()");
        fclose(fp);
        exit(1);
    }

    /* get length to read */
    if (strcasecmp(arg_length, "all") == 0) {
        len = 0;
    } else {
        len = get_long(arg_length);
    }

    if (len < 1) {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp) - offset;
        fseek(fp, offset, SEEK_SET);
    }

    /* read and print bytes */
    for (long i = 0; i < len; i++) {
        int c = fgetc(fp);

        if (c == EOF) {
            putchar('\n');
            break;
        }

        int j = i + 1;

        if (i > 0 && j%4 == 0 && j%16 != 0) {
            printf(" %02X ", (uint8_t)c);
        } else {
            printf(" %02X", (uint8_t)c);
        }

        if (j == len || (i > 0 && j%16 == 0)) {
            putchar('\n');
        }
    }

    fflush(stdout);
    fclose(fp);
}

/* write data to file */
void write_to_file(const char *file, uint8_t *data, uint8_t *byte, const char *arg_offset, long num_bytes)
{
    int sk;

    if (!data && !byte) {
        abort();
    }

    /* open or create file */
    int fd = open(file, O_RDWR | O_CREAT, 0664);

    if (fd == -1) {
        perror("open()");
        exit(1);
    }

    /* seek to offset */
    if (strcasecmp(arg_offset, "append") == 0) {
        sk = lseek(fd, 0, SEEK_END);
    } else {
        sk = lseek(fd, get_long(arg_offset), SEEK_SET);
    }

    if (sk == -1) {
        perror("lseek()");
        close(fd);
        exit(1);
    }

    /* write data */
    if (byte) {
        for (long i = 0; i < num_bytes; i++) {
            if (write(fd, byte, 1) != 1) {
                perror("write()");
                close(fd);
                exit(1);
            }
        }
    } else if (write(fd, data, num_bytes) != num_bytes) {
        perror("write()");
        close(fd);
        exit(1);
    }

    printf("%ld bytes successfully written to `%s'\n", num_bytes, file);
    close(fd);
}

/* write arg_data at arg_offset to file */
void write_data(const char *arg_offset, const char *arg_data, const char *file)
{
    if (*arg_data == 0) {
        fprintf(stderr, "error: empty argument\n");
        exit(1);
    }

    uint8_t *data = malloc((strlen(arg_data) / 2) + 2);

    if (!data) {
        perror("malloc()");
        exit(1);
    }

    uint8_t *pdata = data;
    size_t num_bytes = 0;
    char buf[5] = { '0','x', 0, 0, 0 };

    /* convert hex string to bytes */
    for (const char *p = arg_data; *p != 0; p++) {
        if (isspace(*p)) {
            /* ignore space */
            continue;
        }

        /* error if *p is not a hex digit */
        if (!isxdigit(*p)) {
            if (isprint(*p)) {
                fprintf(stderr, "error: `%c' ", *p);
            } else {
                fprintf(stderr, "error: %x ", *p);
            }
            fprintf(stderr, "is not a hexadecimal digit\n");
            free(data);
            exit(1);
        }

        if (buf[2] == 0) {
            /* save char */
            buf[2] = *p;
        } else if (buf[3] == 0) {
            /* save char, convert to byte, reset buffer */
            buf[3] = *p;
            *pdata++ = strtouchar(buf, 16);
            buf[2] = buf[3] = 0;
            num_bytes++;
        }
    }

    /* trailing single-digit hex number */
    if (buf[2] != 0) {
        *pdata = strtouchar(buf, 16);
        num_bytes++;
    }

    /* save data */
    write_to_file(file, data, NULL, arg_offset, num_bytes);
    free(data);
}

/* create a data set of arg_length and write it to file */
void memset_write_data(const char *arg_offset, const char *arg_length, const char *arg_char, const char *file)
{
    long len = get_long(arg_length);
    long chrlen = strlen(arg_char);
    uint8_t c = 0;

    if (len < 1) {
        fprintf(stderr, "error: length must be 1 or more: %s\n", arg_length);
        exit(1);
    }

    if (chrlen == 1) {
        /* literal character */
        c = (uint8_t)arg_char[0];
    } else if (chrlen > 2 && (*arg_char == '0' || *arg_char == '\\') &&
                (arg_char[1] == 'x' || arg_char[1] == 'X'))
    {
        /* hex number (0x.. or \x..) */
        char *copy = strdup(arg_char);
        copy[0] = '0';
        c = strtouchar(copy, 16);
        free(copy);
    } else if (arg_char[0] == '\\') {
        /* control character */
        if (chrlen == 2) {
            switch(arg_char[1]) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'a': c = '\a'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'v': c = '\v'; break;
                case 'e': c = 0x1B; break;
                default:
                    break;
            }
        }

        /* escaped decimal number */
        if (c == 0) {
            c = strtouchar(arg_char + 1, 10);
        }
    } else {
        fprintf(stderr, "error: invalid argument: %s\n", arg_char);
        exit(1);
    }

    write_to_file(file, NULL, &c, arg_offset, len);
}

void print_usage(const char *self)
{
    printf("usage:\n"
           "  %s --help\n", self);
    //printf("  %s r[ead] <file>\n", self);
    printf("  %s r[ead] [<offset> <length>] <file>\n", self);
    printf("  %s w[rite] <offset> <data> <file>\n", self);
    printf("  %s m[emset] <offset> <length> <char> <file>\n", self);
}

void show_help(const char *self)
{
    print_usage(self);

    printf("\n\n"
           "  read, write, memset: <offset> and <length> may be hexadecimal prefixed with\n"
           "    `0x' or `\\x', an octal number prefixed with `0' or decimal\n"
           "\n"
           "  read: <length> set to 0 or `all' will print all bytes\n"
           "\n"
           "  write, memset: <offset> set to `append' will write data directly after the\n"
           "    end of the file\n"
           "\n"
           "  write: <data> must be hexadecimal without prefixes (whitespaces are ignored)\n"
           "\n"
           "  write: <char> can be a literal character, escaped control character,\n"
           "    hexadecimal value prefixed with `0x' or `\\x', an octal number prefixed\n"
           "    with `0' or a decimal number prefixed with `\\'\n"
           "\n");
}

int is_cmd(const char *arg, const char *cmd)
{
    return ((tolower(*arg) == tolower(*cmd) && arg[1] == 0) ||
        strcasecmp(arg, cmd) == 0);
}

int main(int argc, char **argv)
{
    for (int i=0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            show_help(argv[0]);
            return 0;
        }
    }

    switch (argc)
    {
    case 3:
        if (is_cmd(argv[1], "read")) {
            read_data("0", "all", argv[2]);
            return 0;
        }
        break;

    case 5:
        if (is_cmd(argv[1], "read")) {
            read_data(argv[2], argv[3], argv[4]);
            return 0;
        } else if (is_cmd(argv[1], "write")) {
            write_data(argv[2], argv[3], argv[4]);
            return 0;
        }
        break;

    case 6:
        if (is_cmd(argv[1], "memset")) {
            memset_write_data(argv[2], argv[3], argv[4], argv[5]);
            return 0;
        }
        break;

    default:
        break;
    }

    print_usage(argv[0]);

    return 1;
}
