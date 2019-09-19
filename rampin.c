#include <Windows.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <assert.h>

typedef long long int s64;

static const wchar_t * filepath_to_filename(const wchar_t * path)
{
    size_t i, len, lastslash;

    len = wcslen(path);
    lastslash = 0;
    for(i = 0; i < len; ++i)
        if(path[i] == L'/' || path[i] == L'\\')
            lastslash = i + 1;

    return path + lastslash;
}

static void * memorymapfile(const wchar_t * fname, s64 * fsize)
{
    HANDLE f, m;
    void * ptr = NULL;
    LARGE_INTEGER li;

    f = CreateFileW(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(f == INVALID_HANDLE_VALUE)
        return NULL;

    if(GetFileSizeEx(f, &li) == 0)
        return NULL;

    m = CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL);
    if(m == NULL)
        return NULL;

    ptr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    *fsize = li.QuadPart;
    return ptr;
}

static double mytime(void)
{
    LARGE_INTEGER freq, ret;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ret);
    return ((double)ret.QuadPart) / ((double)freq.QuadPart);
}

struct MappedFile
{
    unsigned char * ptr;
    s64 size;
};

static void touchbytesonce(const struct MappedFile * file)
{
    unsigned ret = 0u;
    s64 i;

    for(i = 0; i < file->size; i += 0x1000)
        ret += file->ptr[i];
}

static void touchfirsttime(const struct MappedFile * file, const wchar_t * fname)
{
    double starttime, elapsedtime;

    wprintf(L"%ls: mapped,  %lld bytes, %.3f MiB, 0x%p, touching all pages...\n",
        fname, file->size, file->size / (1024.0 * 1024.0), file->ptr);

    starttime = mytime();
    touchbytesonce(file);
    elapsedtime = mytime() - starttime;

    wprintf(L"%ls: touched, %lld bytes, %.3f MiB, 0x%p, %.3fs, %.3f MiB/s\n",
        fname, file->size, file->size / (1024.0 * 1024.0), file->ptr,
        elapsedtime, file->size / (elapsedtime * 1024.0 * 1024.0)
    );
}

static void print_usage(const wchar_t * argv0, FILE * f)
{
    const wchar_t * fname = filepath_to_filename(argv0);
    fwprintf(f, L"%ls - memory map a file and touch all pages periodically\n", fname);
    fwprintf(f, L"Help:  %ls -h #only valid way to use -h\n", fname);
    fwprintf(f, L"Usage: %ls [options...] [--] files...\n", fname);
    fwprintf(f, L"Options:\n", fname);
    fwprintf(f, L"    -h #print this help to stdout (bad invocation print this help to stderr)\n");
    fwprintf(f, L"    -0, -1, ..., -9 #loop 0-9 times after initial mapping and touch, then quit\n", fname);
}

#define BITOPT_HELP 0

static void doBitSet(unsigned * flags, int bit)
{
    assert(flags);
    *flags |= 1 << bit;
}

static int isBitSet(unsigned flags, int bit)
{
    return !!(flags & (1 << bit));
}

static int parse_options(int argc, wchar_t ** argv, int * firstfile, unsigned * flags, int * loops)
{
    int i;

    assert(flags);
    assert(firstfile);

    *flags = 0u;
    *firstfile = 1;
    *loops = -1;

    if(argc == 2 && 0 == wcscmp(argv[1], L"-h"))
    {
        doBitSet(flags, BITOPT_HELP);
        return 1;
    }

    for(i = 1; i < argc; ++i)
    {
        int ff;

        if(0 == wcscmp(argv[i], L"--"))
        {
            ff = i + 1;
            if(ff == argc)
            {
                fwprintf(stderr, L"no files after -- (arg #%d), files must come after options\n", i);
                return 0;
            }

            *firstfile = ff;
            return 1;
        } /* if argv[i] is -- */

        if(argv[i][0] == L'-')
        {
            switch(argv[i][1])
            {
                case L'0': case L'1': case L'2': case L'3': case L'4':
                case L'5': case L'6': case L'7': case L'8': case L'9':
                    *loops = argv[i][1] - L'0';
                    wprintf(L"loops is %d\n", *loops);
                    break;
                case L'h':
                    fwprintf(stderr, L"wrong use of -h (arg #%d), use '%ls -h' to print help to stdout\n", i, argv[0]);
                    return 0;
                default:
                    fwprintf(
                        stderr,
                        L"unknown option %ls (arg #%d), for filenames starting with - use -- to end options first\n",
                        argv[i],
                        i
                    );
                    return 0;
            } /* switch argv i 1 */

            ff = i + 1;
            if(ff == argc)
            {
                fwprintf(stderr, L"no files after last option, use -- to end options to pass filenames starting with -\n");
                return 0;
            }

            *firstfile = ff;
        }
        else
        {
            *firstfile = i;
            return 1;
        }
    } /* for */

    return 1;
}

int wmain(int argc, wchar_t ** argv)
{
    struct MappedFile * files;
    int i;
    unsigned flags;
    int firstfile;
    int goodcount;
    int loops;

    if(argc < 2)
    {
        print_usage(argv[0], stderr);
        return 1;
    }

    if(!parse_options(argc, argv, &firstfile, &flags, &loops))
    {
        print_usage(argv[0], stderr);
        return 1;
    }

    if(isBitSet(flags, BITOPT_HELP))
    {
        /* since it was correctly requested we print help to stdout not stderr */
        print_usage(argv[0], stdout);
        return 0;
    }

    /* just alloc argc elems and not use few first ones to not mess with indices */
    files = (struct MappedFile*)calloc(argc, sizeof(struct MappedFile));
    if(!files)
    {
        fwprintf(stderr, L"calloc(%d, %d) failed\n", argc, (int)sizeof(struct MappedFile));
        return 2;
    }

    goodcount = 0;
    for(i = firstfile; i < argc; ++i)
    {
        files[i].ptr = (unsigned char*)memorymapfile(argv[i], &files[i].size);
        if(!files[i].ptr)
            fwprintf(stderr, L"%ls: failed to map!\n", argv[i]);
        else
            ++goodcount;
    }

    if(goodcount == 0)
    {
        fwprintf(stderr, L"failed to map any of the given %d files, quitting!\n", argc - firstfile);
        free(files);
        return 3;
    }

    for(i = firstfile; i < argc; ++i)
        if(files[i].ptr)
            touchfirsttime(&files[i], argv[i]);

    while(1)
    {
        if(loops == 0)
            break;

        if(loops > 0)
            --loops;

        Sleep(30 * 1000);
        for(i = 1; i < argc; ++i)
            if(files[i].ptr)
                touchbytesonce(&files[i]);
    }

    free(files);
    return 0;
}
