# rampin

A small C program to try keep a file or few in Windows RAM cache.

Takes one or more filenames as arguments, opens them as read only and memory
maps them whole, then accesses each 4 KiB page of it to bring them into memory
and then sleeps for 30 seconds and accesses them again to keep them in memory.

**By default it never quits so you'll have to somehow kill it once you are done.**

A sole `-h` will print full help to stdout. Options `-0`, `-1`, etc. up to `-9` will
make it quit after that many sleep + touch loops, not counting the initial one
so `-0` will map the file, touch all pages once, and quit right after, `-1`
will map, touch once, and sleep once and touch again and then quit, etc.

This program only makes sense if you have RAM to spare and/or are on 64-bit. I
wrote it to speed up level loading in an old game that has a total of 5 GiB of
assets and a 32-bit exe, so the game, OS and all the assets fit into my PC's RAM.

**This program might easily backfire if used too greedily and your OS might start paging to disk!**

To make it easier to rampin entire dir tree I use this help script named `rampintree`,
but mapping and touching many files one after another has been added recently to the
`rampin` program itself so just `rampin *` or `rampin` + list of files works too.
```
#!/bin/bash

if [ "$#" -eq 1 ]
then
    find "$1" -type f | xargs -P 99000 -I @ rampin @
fi
```

Go to releases to download a Windows exe compiled with Pelles C with no `-O2`
to avoid running into any `-O2` optimizer bug similar to this one that affected
`stb_image`: [Pelles C forum bug report](https://forum.pellesc.de/index.php?topic=7837.0)
