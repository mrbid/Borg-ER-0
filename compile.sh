mkdir release
rm release/borg0
rm release/borg0.exe
rm release/borg0.AppImage
rm release/borg0.deb
rm release/borg0_win.zip
clang main.c -Ofast -lSDL2 -lm -o release/borg0
i686-w64-mingw32-gcc -std=c17 main.c -ISDL2/include -LSDL2/lib -Ofast -Wall -lmingw32 -lSDL2main -lSDL2 -o release/borg0.exe
strip --strip-unneeded release/borg0
strip --strip-unneeded release/borg0.exe
upx --lzma --best release/borg0
upx --lzma --best release/borg0.exe
cp release/borg0 borg0.AppDir/usr/bin/borg0
./appimagetool-x86_64.AppImage borg0.AppDir release/borg0.AppImage
cp release/borg0 deb/usr/bin/borg0
dpkg-deb --build deb release/borg0.deb
cd release
zip borg0_win.zip SDL2.dll borg0.exe
