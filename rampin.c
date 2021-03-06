#include <Windows.h>
#include <string.h>
#include <stdlib.h>
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

struct MappedFile
{
    const unsigned char * ptr;
    s64 size;
    HANDLE file;
    HANDLE mapping;
    const wchar_t * name;

    /* instead of volatile, += touch returns into this var to prevent optimization */
    unsigned optimizationpreventer;
};

static void memorymapfile(const wchar_t * fname, struct MappedFile * file)
{
    HANDLE f, m;
    void * ptr = NULL;
    LARGE_INTEGER li;

    f = CreateFileW(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(f == INVALID_HANDLE_VALUE)
        return;

    if(GetFileSizeEx(f, &li) == 0)
    {
        CloseHandle(f);
        return;
    }

    m = CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL);
    if(m == NULL)
    {
        CloseHandle(f);
        return;
    }

    ptr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    if(!ptr)
    {
        CloseHandle(f);
        CloseHandle(m);
        return;
    }

    file->ptr = (const unsigned char*)ptr;
    file->file = f;
    file->mapping = m;
    file->name = fname;
    file->size = li.QuadPart;
}

static void unmapfile(struct MappedFile * file)
{
    if(file->ptr)
    {
        UnmapViewOfFile(file->ptr);
        CloseHandle(file->mapping);
        CloseHandle(file->file);
    }

    file->ptr = NULL;
    file->size = 0;
    file->file = NULL;
    file->mapping = NULL;
    file->name = NULL;
}

static void unmapall(struct MappedFile * files, int count)
{
    int i;

    for(i = 0; i < count; ++i)
        unmapfile(&files[i]);
}

static double mytime(void)
{
    LARGE_INTEGER freq, ret;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ret);
    return ((double)ret.QuadPart) / ((double)freq.QuadPart);
}

static unsigned touchbytesonce(const struct MappedFile * file)
{
    unsigned ret = 0u;
    s64 i;

    for(i = 0; i < file->size; i += 0x1000)
        ret += file->ptr[i];

    return ret;
}

static void print_file_info(const struct MappedFile * file)
{
    wprintf(L"%ls: mapped,  %lld bytes, %.3f MiB, 0x%p\n",
        file->name, file->size, file->size / (1024.0 * 1024.0), file->ptr);
}

static unsigned touchfirsttime(const struct MappedFile * file, int printinfo)
{
    double starttime, elapsedtime;
    unsigned ret;

    if(printinfo)
        wprintf(L"%ls: mapped,  %lld bytes, %.3f MiB, 0x%p, touching all pages...\n",
            file->name, file->size, file->size / (1024.0 * 1024.0), file->ptr);

    starttime = mytime();
    ret = touchbytesonce(file);
    elapsedtime = mytime() - starttime;

    if(printinfo)
        wprintf(L"%ls: touched, %lld bytes, %.3f MiB, 0x%p, %.3fs, %.3f MiB/s\n",
            file->name, file->size, file->size / (1024.0 * 1024.0), file->ptr,
            elapsedtime, file->size / (elapsedtime * 1024.0 * 1024.0)
        );

    return ret;
}

static void print_usage(const wchar_t * argv0, FILE * f)
{
    const wchar_t * fname = filepath_to_filename(argv0);
    fwprintf(f, L"%ls - memory map a file and touch all pages periodically\n", fname);
    fwprintf(f, L"Help:  %ls -h #only valid way to use -h\n", fname);
    fwprintf(f, L"Usage: %ls [options...] [--] files...\n", fname);
    fwprintf(f, L"Options:\n");
    fwprintf(f, L"    -h #print this help to stdout (bad invocation prints it to stderr)\n");
    fwprintf(f, L"    -0, -1, ..., -9 #loop 0-9 times after initial mapping and touch, then quit\n");
    fwprintf(f, L"    -t #total, after initial touch print total bytes and speed and time\n");
    fwprintf(f, L"    -q #quiet, don't print the mapped and touched info lines to stdout\n");
    fwprintf(f, L"    -T #TOTAL only, like -t and -q together\n");
    fwprintf(f, L"    -s #sort in  ascending order before initial touch\n");
    fwprintf(f, L"    -S #SORT in descending order before initial touch\n");
    fwprintf(f, L"    -p #pedantic, quit if any file fails to open or map\n");
    fwprintf(f, L"    -m #map only, try map all given files with no touch or sleep\n");
}

#define BITOPT_HELP 0
#define BITOPT_TOTAL 1
#define BITOPT_QUIET 2
#define BITOPT_SORT_ASC 3
#define BITOPT_SORT_DESC 4
#define BITOPT_PEDANTIC 5
#define BITOPT_MAPONLY 6

static void doBitSet(unsigned * flags, int bit)
{
    assert(flags);
    *flags |= 1u << bit;
}

static void doBitClr(unsigned * flags, int bit)
{
    assert(flags);
    *flags &= ~(1u << bit);
}

static int isBitSet(unsigned flags, int bit)
{
    return !!(flags & (1u << bit));
}

static int parse_options(int argc, wchar_t ** argv, int * firstfile, unsigned * flags, int * loops)
{
    int i, j;

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
            for(j = 1; argv[i][j]; ++j)
            {
                const wchar_t c = argv[i][j];
                switch(c)
                {
                    case L'0': case L'1': case L'2': case L'3': case L'4':
                    case L'5': case L'6': case L'7': case L'8': case L'9':
                        *loops = c - L'0';
                        break;
                    case L'h':
                        fwprintf(stderr, L"wrong use of h in %ls (arg #%d), use '%ls -h' to print help to stdout\n",
                            argv[i], i, argv[0]);
                        return 0;
                    case L't':
                        doBitSet(flags, BITOPT_TOTAL);
                        break;
                    case L'q':
                        doBitSet(flags, BITOPT_QUIET);
                        break;
                    case L'T':
                        doBitSet(flags, BITOPT_TOTAL);
                        doBitSet(flags, BITOPT_QUIET);
                        break;
                    case L's':
                        doBitClr(flags, BITOPT_SORT_DESC);
                        doBitSet(flags, BITOPT_SORT_ASC);
                        break;
                    case L'S':
                        doBitClr(flags, BITOPT_SORT_ASC);
                        doBitSet(flags, BITOPT_SORT_DESC);
                        break;
                    case L'p':
                        doBitSet(flags, BITOPT_PEDANTIC);
                        break;
                    case L'm':
                        doBitSet(flags, BITOPT_MAPONLY);
                        break;
                    default:
                        fwprintf(
                            stderr,
                            L"unknown option %lc in %ls (arg #%d), for filenames starting with - use -- to end options first\n",
                            c, argv[i], i
                        );
                        return 0;
                } /* switch c */
            } /* for j */

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

static int mycmp64(s64 a, s64 b)
{
    return (a > b) - (a < b);
}

static int smallerfile(const void * a, const void * b)
{
    const struct MappedFile * aa = (const struct MappedFile *)a;
    const struct MappedFile * bb = (const struct MappedFile *)b;
    return mycmp64(aa->size, bb->size);
}

static int biggerfile(const void * a, const void * b)
{
    const struct MappedFile * aa = (const struct MappedFile *)a;
    const struct MappedFile * bb = (const struct MappedFile *)b;
    return -mycmp64(aa->size, bb->size);
}

int wmain(int argc, wchar_t ** argv)
{
    struct MappedFile * files;
    int i;
    unsigned flags;
    int firstfile;
    int goodcount;
    int loops;
    double totalstarttime;

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
    totalstarttime = mytime();
    for(i = firstfile; i < argc; ++i)
    {
        memorymapfile(argv[i], &files[i]);
        if(!files[i].ptr)
        {
            fwprintf(stderr, L"%ls: failed to map!\n", argv[i]);
            if(isBitSet(flags, BITOPT_PEDANTIC))
            {
                fwprintf(stderr, L"failed to map a file while running with pedantic (-p), quitting!\n");
                unmapall(files, argc);
                free(files);
                return 4;
            }
        }
        else
        {
            ++goodcount;
        }
    }

    if(goodcount == 0)
    {
        fwprintf(stderr, L"failed to map any of the given %d files, quitting!\n", argc - firstfile);
        unmapall(files, argc);
        free(files);
        return 3;
    }

    /* 0 sized elements just end up at end/start and don't affect anything due to if ptr checks */
    if(isBitSet(flags, BITOPT_SORT_ASC))
        qsort(files, argc, sizeof(struct MappedFile), &smallerfile);

    if(isBitSet(flags, BITOPT_SORT_DESC))
        qsort(files, argc, sizeof(struct MappedFile), &biggerfile);

    for(i = 0; i < argc; ++i)
    {
        if(!files[i].ptr)
            continue;

        if(isBitSet(flags, BITOPT_MAPONLY) && !isBitSet(flags, BITOPT_QUIET))
            print_file_info(&files[i]);

        if(!isBitSet(flags, BITOPT_MAPONLY))
            files[i].optimizationpreventer += touchfirsttime(&files[i], !isBitSet(flags, BITOPT_QUIET));
    }

    if(isBitSet(flags, BITOPT_TOTAL))
    {
        double totalelapsedtime;
        s64 totalbytes;

        totalelapsedtime = mytime() - totalstarttime;
        totalbytes = 0;
        for(i = 0; i < argc; ++i)
            if(files[i].ptr)
                totalbytes += files[i].size;

        if(isBitSet(flags, BITOPT_MAPONLY))
        {
            wprintf(L"mapped %d files, %lld bytes, %.3f MiB, %.3fs\n",
                goodcount, totalbytes, totalbytes / (1024.0 * 1024.0), totalelapsedtime);
        }
        else
        {
            wprintf(
                L"touched %d files, %lld bytes, %.3f MiB, %.3fs, %.3f MiB/s\n",
                goodcount, totalbytes, totalbytes / (1024.0 * 1024.0),
                totalelapsedtime, totalbytes / (totalelapsedtime * 1024.0 * 1024.0)
            );
        }
    }

    /* to instantly quit the below while 1 loop and never touch bytes */
    if(isBitSet(flags, BITOPT_MAPONLY))
        loops = 0;

    while(1)
    {
        if(loops == 0)
            break;

        if(loops > 0)
            --loops;

        Sleep(30 * 1000);
        for(i = 0; i < argc; ++i)
            if(files[i].ptr)
                files[i].optimizationpreventer += touchbytesonce(&files[i]);
    }

    unmapall(files, argc);
    free(files);
    return 0;
}
