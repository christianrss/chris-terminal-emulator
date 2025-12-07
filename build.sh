mkdir -p bin
gcc -o bin/chris-terminal chris_terminal.c -I/usr/include/freetype2 -lleif -lrunara -lm -lGL -lXrender -lfreetype -lfontconfig -lharfbuzz -lX11 -lglfw -DLF_GLFW