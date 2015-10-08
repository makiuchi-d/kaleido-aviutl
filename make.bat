cl /Ox /LD /Fekaleidoscope.auf kaleidoscope.cpp /link /EXPORT:GetFilterTable user32.lib
del *.exp *.lib *.obj