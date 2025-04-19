// KEY_EVENT is defined as well in
// mxe/usr/x86_64-w64-mingw32.static/include/wincon.h:105
// since we don't use either, we disable them all and avoid the warning

#undef KEY_EVENT
#include <ncurses.h>
#undef KEY_EVENT
