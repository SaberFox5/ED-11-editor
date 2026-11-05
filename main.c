#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <raylib.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

#define SCR_COLS       80
#define SCR_ROWS       25
#define CW             14
#define CH             24
#define EDIT_ROWS      (SCR_ROWS - 2)
#define MAX_LINES      1000
#define LINE_LEN       256
#define MAX_PATH_LEN   512
#define UPPERCASE_ONLY 1

#define PRESSED(k) (IsKeyPressed(k) || IsKeyPressedRepeat(k))

/* posição e tamanho dos botões na barra de cima (em colunas) */
#define BTN_ROW   0
#define BTN_COLS  6

/* ---------- texto e cursor ---------- */

typedef struct {
    char lines[MAX_LINES][LINE_LEN];
    int  numLines;
    int  curRow, curCol, topRow;
} Buffer;

/* é global para não ocupar a pilha */
static Buffer buf;

static void buf_init(Buffer *b) {
    memset(b, 0, sizeof *b);
    b->numLines = 1;
}

static void buf_insertChar(Buffer *b, int c) {
    int len = (int)strlen(b->lines[b->curRow]);
    if (len >= LINE_LEN - 1 || c < 32 || c > 126) return;
    memmove(&b->lines[b->curRow][b->curCol + 1],
            &b->lines[b->curRow][b->curCol],
            len - b->curCol + 1);
    b->lines[b->curRow][b->curCol] = (char)c;
    b->curCol++;
}

static void buf_newLine(Buffer *b) {
    if (b->numLines >= MAX_LINES) return;
    for (int i = b->numLines; i > b->curRow + 1; i--)
        strcpy(b->lines[i], b->lines[i - 1]);
    b->numLines++;
    strcpy(b->lines[b->curRow + 1], &b->lines[b->curRow][b->curCol]);
    b->lines[b->curRow][b->curCol] = '\0';
    b->curRow++;
    b->curCol = 0;
}

static void buf_backspace(Buffer *b) {
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

static void buf_deleteChar(Buffer *b) {
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

static int buf_load(Buffer *b, const char *path) {
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

static int buf_save(const Buffer *b, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    for (int i = 0; i < b->numLines; i++)
        fprintf(f, "%s\n", b->lines[i]);
    fclose(f);
    return 1;
}

/* ---------- temas ---------- */

typedef struct {
    Color bg;       /* fundo do ecrã */
    Color btn;      /* fundo dos botões */
    Color btnText;  /* texto dos botões */
    Color ink;      /* texto das linhas */
    Color bar;      /* barras, cursor e moldura dos botões */
    Color barText;  /* texto dentro das barras e letra do cursor */
} Theme;

static const Theme THEME_DARK = {
    { 0, 0, 0, 255 },        /* fundo */
    { 0, 0, 0, 255 },        /* botões */
    { 51, 255, 102, 255 },   /* texto dos botões verde */
    { 51, 255, 102, 255 },   /* texto das linhas verde fósforo */
    { 51, 255, 102, 255 },   /* barras e cursor verdes */
    { 0, 0, 0, 255 },        /* texto das barras preto */
};

static const Theme THEME_LIGHT = {
    { 0, 0, 0, 255 },        /* fundo preto */
    { 0, 0, 0, 255 },        /* botões pretos */
    { 255, 255, 255, 255 },  /* texto dos botões branco */
    { 255, 255, 255, 255 },  /* texto das linhas branco */
    { 35, 35, 35, 255 },     /* barras e cursor cinzentos */
    { 255, 255, 255, 255 },  /* texto das barras branco */
};

static const Theme *th = &THEME_DARK;
static bool lightTheme = false;

/* ---------- estado ---------- */

static char filename[MAX_PATH_LEN] = "SEMNOME.TXT";
static char status[64] = "PRONTO";

static void setStatus(const char *msg) {
    snprintf(status, sizeof status, "%s", msg);
}

static void clampCol(void) {
    int len = (int)strlen(buf.lines[buf.curRow]);
    if (buf.curCol > len) buf.curCol = len;
}

static Rectangle btnThemeRect(void) {
    return (Rectangle){ 44 * CW, BTN_ROW * CH, BTN_COLS * CW, CH };
}

static Rectangle btnOpenRect(void) {
    return (Rectangle){ 52 * CW, BTN_ROW * CH, BTN_COLS * CW, CH };
}

static Rectangle btnNewRect(void) {
    return (Rectangle){ 59 * CW, BTN_ROW * CH, BTN_COLS * CW, CH };
}

static void toggleTheme(void) {
    lightTheme = !lightTheme;
    th = lightTheme ? &THEME_LIGHT : &THEME_DARK;
    setStatus(lightTheme ? "TEMA CLARO" : "TEMA ESCURO");
}

/* ---------- diálogo do sistema ---------- */

/* save = false: escolher um ficheiro existente
   save = true : escolher o nome de um ficheiro novo */
static bool fileDialog(char *out, size_t size, bool save) {
    char cmd[1024];

#if defined(_WIN32)
    snprintf(cmd, sizeof cmd,
        "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
        "$f = New-Object System.Windows.Forms.%s; "
        "$f.Filter = 'Texto|*.txt|Todos|*.*'; "
        "if ($f.ShowDialog() -eq 'OK') { Write-Output $f.FileName }\"",
        save ? "SaveFileDialog" : "OpenFileDialog");
#elif defined(__APPLE__)
    snprintf(cmd, sizeof cmd,
        "osascript -e 'POSIX path of (%s with prompt \"%s\")' 2>/dev/null",
        save ? "choose file name" : "choose file",
        save ? "Criar ficheiro" : "Abrir ficheiro");
#else
    snprintf(cmd, sizeof cmd,
        "zenity --file-selection %s--title=\"%s\" 2>/dev/null",
        save ? "--save --confirm-overwrite " : "",
        save ? "Criar ficheiro" : "Abrir ficheiro");
#endif

    FILE *p = popen(cmd, "r");
    if (!p) return false;

    bool ok = fgets(out, (int)size, p) != NULL;
    pclose(p);
    if (!ok) return false;

    out[strcspn(out, "\r\n")] = '\0';
    return out[0] != '\0';
}

/* ---------- ficheiros ---------- */

static void loadFile(const char *path) {
    snprintf(filename, sizeof filename, "%s", path);
    if (buf_load(&buf, path)) setStatus("LIDO");
    else                      setStatus("NOVO FICHEIRO");
}

static void saveFile(void) {
    if (buf_save(&buf, filename)) setStatus("GRAVADO");
    else                          setStatus("ERRO AO GRAVAR");
}

static void openAction(void) {
    char path[MAX_PATH_LEN];
    if (fileDialog(path, sizeof path, false)) {
        loadFile(path);
        buf.curRow = buf.curCol = buf.topRow = 0;
    }
}

static void newAction(void) {
    char path[MAX_PATH_LEN];
    if (!fileDialog(path, sizeof path, true)) return;

    /* cria o ficheiro já no disco, vazio */
    FILE *f = fopen(path, "w");
    if (!f) {
        setStatus("ERRO AO CRIAR");
        return;
    }
    fclose(f);

    snprintf(filename, sizeof filename, "%s", path);
    buf_init(&buf);
    setStatus("NOVO FICHEIRO");
}

/* ---------- desenho ---------- */

static void putStr(int col, int row, const char *s, Color c) {
    for (int i = 0; s[i] && col + i < SCR_COLS; i++) {
        char b[2] = { s[i], '\0' };
#if UPPERCASE_ONLY
        b[0] = (char)toupper((unsigned char)b[0]);
#endif
        DrawText(b, (col + i) * CW, row * CH, 20, c);
    }
}

static void drawButton(Rectangle r, const char *label) {
    DrawRectangleRec(r, th->btn);
    DrawRectangleLinesEx(r, 1, th->bar);
    putStr((int)(r.x / CW), BTN_ROW, label, th->btnText);
}

static void drawScreen(bool cursorOn) {
    /* cabeçalho invertido */
    DrawRectangle(0, 0, SCR_COLS * CW, CH, th->bar);
    putStr(1, 0, "ED-11 EDITOR  CTRL+S GRAVAR  ESC SAIR", th->barText);

    /* botões */
    drawButton(btnThemeRect(), " TEMA ");
    drawButton(btnOpenRect(),  " ABRIR ");
    drawButton(btnNewRect(),   " NOVO ");

    /* área de edição */
    for (int r = 0; r < EDIT_ROWS; r++) {
        int li = buf.topRow + r;
        if (li >= buf.numLines) break;
        putStr(0, r + 1, buf.lines[li], th->ink);
    }

    /* cursor */
    if (cursorOn) {
        int sy = buf.curRow - buf.topRow + 1;
        if (sy >= 1 && sy <= EDIT_ROWS) {
            char under = buf.lines[buf.curRow][buf.curCol];
            if (under == '\0') under = ' ';
            DrawRectangle(buf.curCol * CW, sy * CH, CW, CH, th->bar);
            char b[2] = { under, '\0' };
#if UPPERCASE_ONLY
            b[0] = (char)toupper((unsigned char)b[0]);
#endif
            DrawText(b, buf.curCol * CW, sy * CH, 20, th->barText);
        }
    }

    /* barra de estado */
    char st[MAX_PATH_LEN + 128];
    snprintf(st, sizeof st, " %s  LIN %d/%d  COL %d  %s",
             filename, buf.curRow + 1, buf.numLines, buf.curCol + 1, status);
    DrawRectangle(0, (SCR_ROWS - 1) * CH, SCR_COLS * CW, CH, th->bar);
    putStr(0, SCR_ROWS - 1, st, th->barText);

    /* linhas de varrimento */
    for (int y = 0; y < SCR_ROWS * CH; y += 3)
        DrawRectangle(0, y, SCR_COLS * CW, 1, (Color){ 0, 0, 0, 80 });
}

/* ---------- ciclo principal ---------- */

static void editorLoop(void) {
    bool running = true;

    while (!WindowShouldClose() && running) {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        /* cliques nos botões */
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            if (CheckCollisionPointRec(m, btnThemeRect())) toggleTheme();
            if (CheckCollisionPointRec(m, btnOpenRect()))  openAction();
            if (CheckCollisionPointRec(m, btnNewRect()))   newAction();
        }

        /* texto escrito: esvazia sempre a fila, mas só insere se Ctrl não estiver premido */
        int c;
        while ((c = GetCharPressed()) != 0) {
            if (!ctrl) buf_insertChar(&buf, c);
        }

        /* atalhos */
        if (ctrl && IsKeyPressed(KEY_S)) saveFile();
        if (ctrl && IsKeyPressed(KEY_O)) openAction();
        if (ctrl && IsKeyPressed(KEY_N)) newAction();
        if (ctrl && IsKeyPressed(KEY_T)) toggleTheme();
        if (IsKeyPressed(KEY_ESCAPE))    running = false;

        /* edição */
        if (PRESSED(KEY_ENTER))     buf_newLine(&buf);
        if (PRESSED(KEY_BACKSPACE)) buf_backspace(&buf);
        if (PRESSED(KEY_DELETE))    buf_deleteChar(&buf);
        if (PRESSED(KEY_TAB))
            for (int i = 0; i < 4; i++) buf_insertChar(&buf, ' ');

        /* movimento */
        if (PRESSED(KEY_UP) && buf.curRow > 0) {
            buf.curRow--;
            clampCol();
        }
        if (PRESSED(KEY_DOWN) && buf.curRow < buf.numLines - 1) {
            buf.curRow++;
            clampCol();
        }
        if (PRESSED(KEY_LEFT)) {
            if (buf.curCol > 0) buf.curCol--;
            else if (buf.curRow > 0) {
                buf.curRow--;
                buf.curCol = (int)strlen(buf.lines[buf.curRow]);
            }
        }
        if (PRESSED(KEY_RIGHT)) {
            int len = (int)strlen(buf.lines[buf.curRow]);
            if (buf.curCol < len) buf.curCol++;
            else if (buf.curRow < buf.numLines - 1) {
                buf.curRow++;
                buf.curCol = 0;
            }
        }
        if (PRESSED(KEY_HOME)) buf.curCol = 0;
        if (PRESSED(KEY_END))  buf.curCol = (int)strlen(buf.lines[buf.curRow]);

        /* manter o cursor visível */
        if (buf.curRow < buf.topRow) buf.topRow = buf.curRow;
        if (buf.curRow >= buf.topRow + EDIT_ROWS)
            buf.topRow = buf.curRow - EDIT_ROWS + 1;

        bool cursorOn = ((int)(GetTime() * 2) % 2) == 0;

        BeginDrawing();
        ClearBackground(th->bg);
        drawScreen(cursorOn);
        EndDrawing();
    }
}

int main(int argc, char **argv) {
    InitWindow(SCR_COLS * CW, SCR_ROWS * CH, "ED-11");
    SetTargetFPS(60);
    ToggleFullscreen();

    buf_init(&buf);
    if (argc > 1) loadFile(argv[1]);
    else          loadFile(filename);

    editorLoop();

    CloseWindow();
    return 0;
}