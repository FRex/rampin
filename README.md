# rampin

A small C program to try keep a single file in Windows RAM cache.

Takes a single filename as argument, opens the file as read only and memory
maps it whole, then accesses each 4 KiB page of it to bring the file into memory
and then sleeps for 30 seconds and accesses them again to keep them in memory.

**It never quits so you'll have to somehow kill it once you are done.**

This program only makes sense if you have RAM to spare and/or are on 64-bit. I
wrote it to speed up level loading in an old game that has a total of 5 GiB of
assets and a 32-bit exe, so the game, OS and all the assets fit into my PC's RAM.

**This program might easily backfire if used too greedily and your OS might start paging to disk!**

Since it takes only a single argument the best way to use it is to use
another command/tool to invoke many copies of it, e.g.:
```
$ find /d/GOG\ Games/SomeGameName/ -type f | xargs -P 100000 -I @ rampin @
```

Go to releases to download a Windows exe compiled with Pelles C with no `-O2`
to avoid running into any `-O2` optimizer bug similar to this one that affected
`stb_image`: [Pelles C forum bug report](https://forum.pellesc.de/index.php?topic=7837.0)
