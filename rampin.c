#include <Windows.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

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

int wmain(int argc, wchar_t ** argv)
{
    struct MappedFile * files;
    int i;
    int goodcount;

    if(argc < 2)
    {
        const wchar_t * fname = filepath_to_filename(argv[0]);
        fwprintf(stderr, L"%ls - memory map a file and touch all pages periodically\n", fname);
        fwprintf(stderr, L"Usage: %ls file\n", fname);
        return 1;
    }

    files = (struct MappedFile*)calloc(argc, sizeof(struct MappedFile));
    if(!files)
    {
        fwprintf(stderr, L"calloc(%d, %d) failed\n", argc, (int)sizeof(struct MappedFile));
        return 2;
    }

    goodcount = 0;
    for(i = 1; i < argc; ++i)
    {
        files[i].ptr = (unsigned char*)memorymapfile(argv[i], &files[i].size);
        if(!files[i].ptr)
            fwprintf(stderr, L"%ls: failed to map!\n", argv[i]);
        else
            ++goodcount;
    }

    if(goodcount == 0)
    {
        fwprintf(stderr, L"failed to map any of the given %d files, quitting!\n", argc - 1);
        free(files);
        return 3;
    }

    for(i = 1; i < argc; ++i)
        if(files[i].ptr)
            touchfirsttime(&files[i], argv[i]);

    while(1)
    {
        Sleep(30 * 1000);
        for(i = 1; i < argc; ++i)
            if(files[i].ptr)
                touchbytesonce(&files[i]);
    }

    free(files);
    return 0;
}
