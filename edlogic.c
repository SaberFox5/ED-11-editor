#include <stdio.h>
#include <string.h>
#include "edlogic.h"

void buf_init(Buffer *b) {
    memset(b, 0, sizeof *b);
    b->numLines = 1;
}

void buf_insertChar(Buffer *b, int c) {
    int len = (int)strlen(b->lines[b->curRow]);
    if (len >= LINE_LEN - 1 || c < 32 || c > 126) return;
    memmove(&b->lines[b->curRow][b->curCol + 1],
            &b->lines[b->curRow][b->curCol],
            len - b->curCol + 1);
    b->lines[b->curRow][b->curCol] = (char)c;
    b->curCol++;
}

void buf_newLine(Buffer *b) {
    if (b->numLines >= MAX_LINES) return;
    for (int i = b->numLines; i > b->curRow + 1; i--)
        strcpy(b->lines[i], b->lines[i - 1]);
    b->numLines++;
    strcpy(b->lines[b->curRow + 1], &b->lines[b->curRow][b->curCol]);
    b->lines[b->curRow][b->curCol] = '\0';
    b->curRow++;
    b->curCol = 0;
}

void buf_backspace(Buffer *b) {
    if (b->curCol > 0) {
        int len = (int)strlen(b->lines[b->curRow]);
        memmove(&b->lines[b->curRow][b->curCol - 1],
                &b->lines[b->curRow][b->curCol],
                len - b->curCol + 1);
        b->curCol--;
    } else if (b->curRow > 0) {
        int prevLen = (int)strlen(b->lines[b->curRow - 1]);
        int len = (int)strlen(b->lines[b->curRow]);
        if (prevLen + len < LINE_LEN) {
            strcat(b->lines[b->curRow - 1], b->lines[b->curRow]);
            for (int i = b->curRow; i < b->numLines - 1; i++)
                strcpy(b->lines[i], b->lines[i + 1]);
            b->numLines--;
            b->curRow--;
            b->curCol = prevLen;
        }
    }
}

void buf_deleteChar(Buffer *b) {
    int len = (int)strlen(b->lines[b->curRow]);
    if (b->curCol < len) {
        memmove(&b->lines[b->curRow][b->curCol],
                &b->lines[b->curRow][b->curCol + 1],
                len - b->curCol);
    } else if (b->curRow < b->numLines - 1 &&
               len + (int)strlen(b->lines[b->curRow + 1]) < LINE_LEN) {
        strcat(b->lines[b->curRow], b->lines[b->curRow + 1]);
        for (int i = b->curRow + 1; i < b->numLines - 1; i++)
            strcpy(b->lines[i], b->lines[i + 1]);
        b->numLines--;
    }
}

int buf_load(Buffer *b, const char *path) {
    FILE *f = fopen(path, "r");
    b->numLines = 0;
    if (!f) { buf_init(b); return 0; }
    while (b->numLines < MAX_LINES && fgets(b->lines[b->numLines], LINE_LEN, f)) {
        b->lines[b->numLines][strcspn(b->lines[b->numLines], "\r\n")] = '\0';
        b->numLines++;
    }
    fclose(f);
    if (b->numLines == 0) buf_init(b);
    return 1;
}

int buf_save(const Buffer *b, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < b->numLines; i++)
        fprintf(f, "%s\n", b->lines[i]);
    fclose(f);
    return 1;
}