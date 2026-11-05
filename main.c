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

static const Color PHOS    = { 51, 255, 102, 255 };
static const Color PHOS_BG = { 0, 0, 0, 255 };

static char lines[MAX_LINES][LINE_LEN];
static int  numLines = 1;
static int  curRow = 0, curCol = 0, topRow = 0;
static char filename[MAX_PATH_LEN] = "SEMNOME.TXT";
static char status[64] = "PRONTO";

static void setStatus(const char *msg) {
    snprintf(status, sizeof status, "%s", msg);
}

static void clampCol(void) {
    int len = (int)strlen(lines[curRow]);
    if (curCol > len) curCol = len;
}

static Rectangle btnOpenRect(void) {
    return (Rectangle){ 52 * CW, BTN_ROW * CH, BTN_COLS * CW, CH };
}

static Rectangle btnNewRect(void) {
    return (Rectangle){ 59 * CW, BTN_ROW * CH, BTN_COLS * CW, CH };
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
    FILE *f = fopen(path, "r");
    snprintf(filename, sizeof filename, "%s", path);
    numLines = 0;

    if (f) {
        while (numLines < MAX_LINES && fgets(lines[numLines], LINE_LEN, f)) {
            lines[numLines][strcspn(lines[numLines], "\r\n")] = '\0';
            numLines++;
        }
        fclose(f);
        setStatus("LIDO");
    } else {
        setStatus("NOVO FICHEIRO");
    }

    if (numLines == 0) {
        lines[0][0] = '\0';
        numLines = 1;
    }
}

static void saveFile(void) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        setStatus("ERRO AO GRAVAR");
        return;
    }
    for (int i = 0; i < numLines; i++)
        fprintf(f, "%s\n", lines[i]);
    fclose(f);
    setStatus("GRAVADO");
}

static void openAction(void) {
    char path[MAX_PATH_LEN];
    if (fileDialog(path, sizeof path, false)) {
        loadFile(path);
        curRow = curCol = topRow = 0;
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
    lines[0][0] = '\0';
    numLines = 1;
    curRow = curCol = topRow = 0;
    setStatus("NOVO FICHEIRO");
}

/* ---------- edição ---------- */

static void insertChar(int c) {
    int len = (int)strlen(lines[curRow]);
    if (len >= LINE_LEN - 1 || c < 32 || c > 126) return;
    memmove(&lines[curRow][curCol + 1], &lines[curRow][curCol], len - curCol + 1);
    lines[curRow][curCol] = (char)c;
    curCol++;
}

static void newLine(void) {
    if (numLines >= MAX_LINES) return;
    for (int i = numLines; i > curRow + 1; i--)
        strcpy(lines[i], lines[i - 1]);
    numLines++;
    strcpy(lines[curRow + 1], &lines[curRow][curCol]);
    lines[curRow][curCol] = '\0';
    curRow++;
    curCol = 0;
}

static void backspace(void) {
    if (curCol > 0) {
        int len = (int)strlen(lines[curRow]);
        memmove(&lines[curRow][curCol - 1], &lines[curRow][curCol], len - curCol + 1);
        curCol--;
    } else if (curRow > 0) {
        int prevLen = (int)strlen(lines[curRow - 1]);
        int len = (int)strlen(lines[curRow]);
        if (prevLen + len < LINE_LEN) {
            strcat(lines[curRow - 1], lines[curRow]);
            for (int i = curRow; i < numLines - 1; i++)
                strcpy(lines[i], lines[i + 1]);
            numLines--;
            curRow--;
            curCol = prevLen;
        }
    }
}

static void deleteChar(void) {
    int len = (int)strlen(lines[curRow]);
    if (curCol < len) {
        memmove(&lines[curRow][curCol], &lines[curRow][curCol + 1], len - curCol);
    } else if (curRow < numLines - 1 && len + (int)strlen(lines[curRow + 1]) < LINE_LEN) {
        strcat(lines[curRow], lines[curRow + 1]);
        for (int i = curRow + 1; i < numLines - 1; i++)
            strcpy(lines[i], lines[i + 1]);
        numLines--;
    }
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
    DrawRectangleRec(r, PHOS_BG);
    DrawRectangleLinesEx(r, 1, PHOS_BG);
    putStr((int)(r.x / CW), BTN_ROW, label, PHOS);
}

static void drawScreen(bool cursorOn) {
    /* cabeçalho invertido */
    DrawRectangle(0, 0, SCR_COLS * CW, CH, PHOS);
    putStr(1, 0, "ED-11 EDITOR  CTRL+S GRAVAR  ESC SAIR", PHOS_BG);

    /* botões */
    Vector2 m = GetMousePosition();
    bool overOpen = CheckCollisionPointRec(m, btnOpenRect());
    bool overNew  = CheckCollisionPointRec(m, btnNewRect());
    drawButton(btnOpenRect(), overOpen ? " ABRIR " : " ABRIR ");
    drawButton(btnNewRect(),  overNew  ? " NOVO "  : " NOVO ");

    /* área de edição */
    for (int r = 0; r < EDIT_ROWS; r++) {
        int li = topRow + r;
        if (li >= numLines) break;
        putStr(0, r + 1, lines[li], PHOS);
    }

    /* cursor */
    if (cursorOn) {
        int sy = curRow - topRow + 1;
        if (sy >= 1 && sy <= EDIT_ROWS) {
            char under = lines[curRow][curCol];
            if (under == '\0') under = ' ';
            DrawRectangle(curCol * CW, sy * CH, CW, CH, PHOS);
            char b[2] = { under, '\0' };
#if UPPERCASE_ONLY
            b[0] = (char)toupper((unsigned char)b[0]);
#endif
            DrawText(b, curCol * CW, sy * CH, 20, PHOS_BG);
        }
    }

    /* barra de estado */
    char st[SCR_COLS + 1];
    snprintf(st, sizeof st, " %s  LIN %d/%d  COL %d  %s",
             filename, curRow + 1, numLines, curCol + 1, status);
    DrawRectangle(0, (SCR_ROWS - 1) * CH, SCR_COLS * CW, CH, PHOS);
    putStr(0, SCR_ROWS - 1, st, PHOS_BG);

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
            if (CheckCollisionPointRec(m, btnOpenRect())) openAction();
            if (CheckCollisionPointRec(m, btnNewRect()))  newAction();
        }

        /* texto escrito */
        if (!ctrl) {
            int c;
            while ((c = GetCharPressed()) != 0)
                insertChar(c);
        }

        /* atalhos */
        if (ctrl && IsKeyPressed(KEY_S)) saveFile();
        if (ctrl && IsKeyPressed(KEY_O)) openAction();
        if (ctrl && IsKeyPressed(KEY_N)) newAction();
        if (IsKeyPressed(KEY_ESCAPE))    running = false;

        /* edição */
        if (PRESSED(KEY_ENTER))     newLine();
        if (PRESSED(KEY_BACKSPACE)) backspace();
        if (PRESSED(KEY_DELETE))    deleteChar();
        if (PRESSED(KEY_TAB)) for (int i = 0; i < 4; i++) insertChar(' ');

        /* movimento */
        if (PRESSED(KEY_UP) && curRow > 0) {
            curRow--;
            clampCol();
        }
        if (PRESSED(KEY_DOWN) && curRow < numLines - 1) {
            curRow++;
            clampCol();
        }
        if (PRESSED(KEY_LEFT)) {
            if (curCol > 0) curCol--;
            else if (curRow > 0) { curRow--; curCol = (int)strlen(lines[curRow]); }
        }
        if (PRESSED(KEY_RIGHT)) {
            int len = (int)strlen(lines[curRow]);
            if (curCol < len) curCol++;
            else if (curRow < numLines - 1) { curRow++; curCol = 0; }
        }
        if (PRESSED(KEY_HOME)) curCol = 0;
        if (PRESSED(KEY_END))  curCol = (int)strlen(lines[curRow]);

        /* manter o cursor visível */
        if (curRow < topRow) topRow = curRow;
        if (curRow >= topRow + EDIT_ROWS) topRow = curRow - EDIT_ROWS + 1;

        bool cursorOn = ((int)(GetTime() * 2) % 2) == 0;

        BeginDrawing();
        ClearBackground(PHOS_BG);
        drawScreen(cursorOn);
        EndDrawing();
    }
}

int main(int argc, char **argv) {
    InitWindow(SCR_COLS * CW, SCR_ROWS * CH, "ED-11");
    SetTargetFPS(60);

    if (argc > 1) loadFile(argv[1]);
    else          loadFile(filename);

    editorLoop();

    CloseWindow();
    return 0;
}