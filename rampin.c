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

static void touchbytesonce(void * ptr, s64 size)
{
    unsigned char * p = ptr;
    unsigned ret = 0u;
    s64 i;

    for(i = 0; i < size; i += 0x1000)
        ret += p[i];
}

int wmain(int argc, wchar_t ** argv)
{
    s64 s = 0;
    void * ptr = 0x0;
    double starttime, elapsedtime;

    if(argc != 2)
    {
        const wchar_t * fname = filepath_to_filename(argv[0]);
        fwprintf(stderr, L"%ls - memory map a file and touch all pages periodically\n", fname);
        fwprintf(stderr, L"Usage: %ls file\n", fname);
        return 1;
    }

    ptr = memorymapfile(argv[1], &s);
    if(ptr)
    {
        wprintf(L"%ls: mapped,  %lld bytes, %.3f MiB, 0x%p, touching all pages...\n",
            argv[1], s, s / (1024.0 * 1024.0), ptr);

        starttime = mytime();
        touchbytesonce(ptr, s);
        elapsedtime = mytime() - starttime;
        wprintf(L"%ls: touched, %lld bytes, %.3f MiB, 0x%p, %.3fs, %.3f MiB/s\n",
            argv[1], s, s / (1024.0 * 1024.0), ptr,
            elapsedtime, s / (elapsedtime * 1024.0 * 1024.0)
        );

        while(1)
        {
            Sleep(30 * 1000);
            touchbytesonce(ptr, s);
        }
    }
    else
    {
        wprintf(L"%ls: failed to map, quitting!\n", argv[1]);
    }

    return 0;
}
