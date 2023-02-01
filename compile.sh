clang main.c -Ofast -lSDL2 -lm -o borg0
strip --strip-unneeded borg0
upx --lzma --best borg0
