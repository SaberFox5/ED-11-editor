#ifndef EDLOGIC_H
#define EDLOGIC_H

#ifdef _WIN32
  #ifdef EDLOGIC_BUILD
    #define EDAPI __declspec(dllexport)
  #else
    #define EDAPI __declspec(dllimport)
  #endif
#else
  #define EDAPI __attribute__((visibility("default")))
#endif

#define MAX_LINES 1000
#define LINE_LEN  256

typedef struct {
    char lines[MAX_LINES][LINE_LEN];
    int  numLines;
    int  curRow, curCol, topRow;
} Buffer;

EDAPI void buf_init(Buffer *b);
EDAPI void buf_insertChar(Buffer *b, int c);
EDAPI void buf_newLine(Buffer *b);
EDAPI void buf_backspace(Buffer *b);
EDAPI void buf_deleteChar(Buffer *b);
EDAPI int  buf_load(Buffer *b, const char *path);
EDAPI int  buf_save(const Buffer *b, const char *path);

#endif  