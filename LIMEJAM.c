#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void version(void)
{
    puts("LIMEJAM Version 1.4 by katahiromz");
}

void usage(void)
{
    puts("LIMEJAM --- Shuffle lines in ASCII/UTF-8 text file\n"
         "\n"
         "Usage: LIMEJAM [OPTIONS] your_file.txt\n"
         "\n"
         "Options:\n"
         "   -k         Keep first line\n"
         "   -s         Just sort, no shuffle\n"
         "   --help     Display this message\n"
         "   --version  Display version information\n"
         "\n"
         "Contact: katayama.hirofumi.mz@gmail.com");
}

char **LIMEJAM_free(size_t *count, char **pptr)
{
    size_t i;
    for (i = 0; i < *count; ++i)
    {
        free(pptr[i]);
    }
    free(pptr);
    return NULL;
}

char **LIMEJAM_add(size_t *count, char **pptr, char *ptr)
{
    char **new_pptr;
    char *new_ptr;

    new_pptr = (char **)realloc(pptr, (*count + 1) * sizeof(char*));
    if (!new_pptr)
    {
        LIMEJAM_free(count, pptr);
        return NULL;
    }
    new_ptr = (char *)malloc(strlen(ptr) + 1);
    if (!new_ptr)
    {
        LIMEJAM_free(count, pptr);
        free(new_pptr);
        return NULL;
    }
    strcpy(new_ptr, ptr);
    new_pptr[*count] = new_ptr;
    ++(*count);
    return new_pptr;
}

int LIMEJAM_compare_line(const void *x, const void *y)
{
    const char **a = (const char **)x;
    const char **b = (const char **)y;
    return strcmp(*a, *b);
}

int LIMEJAM_output(const char *filename, size_t count, char **pptr, int has_bom, size_t *jammer)
{
    FILE *fout;
    size_t i;
    int ret;

    fout = fopen(filename, "w");
    if (!fout)
    {
        fprintf(stderr, "LIMEJAM Error: Cannot write file '%s'\n", filename);
        return 1;
    }

    if (has_bom)
        fprintf(fout, "\xEF\xBB\xBF");

    if (count == 0 || pptr == NULL)
    {
        fclose(fout);
        return 0;
    }

    if (jammer)
    {
        for (i = 0; i < count; ++i)
        {
            fputs(pptr[jammer[i]], fout);
        }
    }
    else
    {
        for (i = 0; i < count; ++i)
        {
            fputs(pptr[i], fout);
        }
    }

    ret = !ferror(fout);
    fclose(fout);
    return ret;
}

int LIMEJAM_main(const char *filename, int just_sort, int keep_first_line)
{
    static char s_buf[1024];
    char bom[3];
    FILE *fin;
    char **pptr = NULL;
    char **new_pptr;
    size_t count = 0;
    int ret, has_bom;
    size_t i, j, n, tmp, *jammer, seed = 0xDEADFACE;

    fin = fopen(filename, "r");
    if (!fin)
    {
        fprintf(stderr, "LIMEJAM Error: Cannot open file '%s'\n", filename);
        return 1;
    }

    if (fread(bom, 3, 1, fin) == 1)
    {
        has_bom = (memcmp(&bom, "\xEF\xBB\xBF", 3) == 0);
        if (!has_bom)
        {
            if (!bom[0] || !bom[1] || !bom[2])
            {
                fclose(fin);
                fprintf(stderr, "LIMEJAM Error: UTF-16 and UTF-32 are not supported yet: '%s'\n", filename);
                return 1;
            }
            fseek(fin, 0, SEEK_SET);
        }
    }

    while (fgets(s_buf, sizeof(s_buf), fin))
    {
        for (i = 0; s_buf[i]; ++i)
        {
            seed += s_buf[i];
            seed = (seed >> 16) | (seed << 16);
        }
        new_pptr = LIMEJAM_add(&count, pptr, s_buf);
        if (!new_pptr)
        {
            fprintf(stderr, "LIMEJAM Error: Out of memory\n");
            fclose(fin);
            return 1;
        }
        pptr = new_pptr;
    }

    fclose(fin);

    if (just_sort)
    {
        if (count > 0)
            qsort(&pptr[keep_first_line], count - keep_first_line, sizeof(char *), LIMEJAM_compare_line);
        ret = LIMEJAM_output(filename, count, pptr, has_bom, NULL);
        LIMEJAM_free(&count, pptr);
        return ret ? 0 : 1;
    }

    jammer = (size_t *)malloc(count * sizeof(size_t));
    if (!jammer)
    {
        fprintf(stderr, "LIMEJAM Error: Out of memory\n");
        LIMEJAM_free(&count, pptr);
        return 1;
    }

    for (i = 0; i < count; ++i)
    {
        jammer[i] = i;
    }

    /* Random shuffle */
    if (count > 0)
    {
        srand(time(NULL) + count + seed);
        for (i = count - 1; i > (size_t)keep_first_line; --i)
        {
            j = rand() % (i + 1 - keep_first_line) + keep_first_line;
            tmp = jammer[i];
            jammer[i] = jammer[j];
            jammer[j] = tmp;
        }
    }

    ret = LIMEJAM_output(filename, count, pptr, has_bom, jammer);
    free(jammer);
    LIMEJAM_free(&count, pptr);
    return ret ? 0 : 1;
}

int main(int argc, char **argv)
{
    int iarg, keep_first_line = 0, just_sort = 0;
    const char *filename = NULL;

    if (argc <= 1)
    {
        usage();
        return 0;
    }

    for (iarg = 1; iarg < argc; ++iarg)
    {
        char *arg = argv[iarg];
        if (!strcmp(arg, "--help") || !strcmp(arg, "/?"))
        {
            usage();
            return 0;
        }
        if (!strcmp(arg, "--version"))
        {
            version();
            return 0;
        }
        if (!strcmp(arg, "-k"))
        {
            keep_first_line = 1;
            continue;
        }
        if (!strcmp(arg, "-s"))
        {
            just_sort = 1;
            continue;
        }
        if (arg[0] == '-')
        {
            fprintf(stderr, "LIMEJAM Error: Invalid option '%s'\n", arg);
            return 1;
        }
        if (!filename)
        {
            filename = arg;
            continue;
        }
        fprintf(stderr, "LIMEJAM Error: Too many arguments\n");
        return 1;
    }

    return LIMEJAM_main(filename, just_sort, keep_first_line);
}
