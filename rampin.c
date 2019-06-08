#include <Windows.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

typedef long long int s64;

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

static void touchbytes(void * ptr, s64 size, const wchar_t * fname)
{
    unsigned char * p = ptr;
    s64 i;
    unsigned ret = 0u;
    int firstrun = 1;

    while(1)
    {
        for(i = 0; i < size; i += 0x0100)
            ret += p[i];

        if(firstrun)
            wprintf(L"%ls: touched all %lld pages once, sleeping 9999ms and looping...\n", fname, i / 0x0100);

        Sleep(9999);
        firstrun = 0;
    }
}

int wmain(int argc, wchar_t ** argv)
{
    s64 s = 0;
    void * ptr = 0x0;

    if(argc != 2)
    {
        wprintf(L"Usage: %ls file\n", argv[0]);
        return 1;
    }

    ptr = memorymapfile(argv[1], &s);
    if(ptr)
    {
        wprintf(L"%ls: mapped %lld bytes (%lld MiB) at 0x%p, touching all pages...\n", argv[1], s, s / (1024 * 1024), ptr);
        touchbytes(ptr, s, argv[1]);
    }
    else
    {
        wprintf(L"%ls: failed to map, quitting!\n", argv[1]);
    }

    return 0;
}
