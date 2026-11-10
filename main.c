#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <raylib.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

/* ---------- medidas ---------- */

#define CW             14    /* largura de uma célula da grelha, em píxeis */
#define CH             24    /* altura de uma célula */
#define WIN_W          1120  /* tamanho da janela antes de passar a ecrã inteiro */
#define WIN_H          600
#define GUTTER         5     /* colunas reservadas aos números das linhas */
#define TITLE_COLS     14    /* coluna onde começam os botões */
#define MAX_LINES      1000
#define LINE_LEN       256
#define MAX_PATH_LEN   512
#define UPPERCASE_ONLY 0

#define PRESSED(k) (IsKeyPressed(k) || IsKeyPressedRepeat(k))

/* ---------- texto e cursor (buffer) ---------- */

typedef struct {
    char lines[MAX_LINES][LINE_LEN];
    int  numLines;
    int  curRow, curCol, topRow;
} Buffer;

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
    Color ink;      /* texto do documento (a numeração e a linha atual usam versões mais leves) */
    Color bar;      /* barras de cima e de baixo, cursor e moldura dos botões */
    Color barText;  /* texto dentro das barras e letra do cursor */
} Theme;

static const Theme THEME_DARK = {
    { 0, 0, 0, 255 },        /* fundo */
    { 0, 0, 0, 255 },        /* botões */
    { 51, 255, 102, 255 },   /* texto dos botões verde */
    { 51, 255, 102, 255 },   /* texto verde fósforo */
    { 51, 255, 102, 255 },   /* barras e cursor verdes */
    { 0, 0, 0, 255 },        /* texto das barras preto */
};

static const Theme THEME_LIGHT = {
    { 0, 0, 0, 255 },        /* fundo preto */
    { 0, 0, 0, 255 },        /* botões pretos */
    { 255, 255, 255, 255 },  /* texto dos botões branco */
    { 255, 255, 255, 255 },  /* texto branco */
    { 35, 35, 35, 255 },     /* barras e cursor cinzentos */
    { 255, 255, 255, 255 },  /* texto das barras branco */
};

static const Theme *th = &THEME_DARK;
static bool lightTheme = false;

/* ---------- estado ---------- */

typedef enum {
    ACT_NONE, ACT_OPEN, ACT_NEW, ACT_SAVE, ACT_FIND,
    ACT_GOTO, ACT_THEME, ACT_HELP, ACT_QUIT
} Action;

typedef enum { PROMPT_NONE, PROMPT_FIND, PROMPT_GOTO } PromptKind;

typedef struct { const char *label; Action act; } ButtonDef;

static const ButtonDef BUTTONS[] = {
    { " ABRIR ",    ACT_OPEN  },
    { " NOVO ",     ACT_NEW   },
    { " GRAVAR ",   ACT_SAVE  },
    { " PROCURAR ", ACT_FIND  },
    { " IR LIN ",   ACT_GOTO  },
    { " TEMA ",     ACT_THEME },
    { " AJUDA ",    ACT_HELP  },
    { " SAIR ",     ACT_QUIT  },
};
#define N_BUTTONS ((int)(sizeof BUTTONS / sizeof BUTTONS[0]))

static const char *HELP_LINES[] = {
    "AJUDA DO ED-11",
    "",
    "CTRL+O   ABRIR FICHEIRO",
    "CTRL+N   NOVO FICHEIRO",
    "CTRL+S   GRAVAR",
    "CTRL+F   PROCURAR TEXTO",
    "F3       PROCURAR SEGUINTE",
    "CTRL+G   IR PARA LINHA",
    "CTRL+T   TROCAR TEMA",
    "F11      ECRA INTEIRO LIGA/DESLIGA",
    "F1       ESTA AJUDA",
    "ESC      SAIR (CONFIRMA SE HA ALTERACOES)",
    "",
    "CLIQUE NO TEXTO PARA MOVER O CURSOR",
    "",
    "PRIMA QUALQUER TECLA PARA VOLTAR",
};

static Buffer buf;
static char filename[MAX_PATH_LEN] = "SEMNOME.TXT";
static char status[96] = "PRONTO";
static bool dirty = false;            /* há alterações por gravar */
static bool running = true;
static bool showHelp = false;
static int  cols = 80, rows = 25;     /* tamanho da grelha, recalculado a cada frame */
static int  leftCol = 0;              /* primeira coluna visível (scroll horizontal) */
static Action pendingDiscard = ACT_NONE;
static PromptKind prompt = PROMPT_NONE;
static char promptText[64] = "";
static int  promptLen = 0;
static char lastFind[64] = "";

/* ---------- auxiliares ---------- */

static void setStatus(const char *msg) {
    snprintf(status, sizeof status, "%s", msg);
}

static void clampCol(void) {
    int len = (int)strlen(buf.lines[buf.curRow]);
    if (buf.curCol > len) buf.curCol = len;
}

static int editRowsNow(void) {
    int n = rows - 3;               /* uma linha de cima, uma de prompt e uma de estado */
    return n < 1 ? 1 : n;
}

static int textColsNow(void) {
    int n = cols - GUTTER;
    return n < 1 ? 1 : n;
}

static Rectangle buttonRect(int i) {
    int col = TITLE_COLS;
    for (int k = 0; k < i; k++)
        col += (int)strlen(BUTTONS[k].label) + 1;
    return (Rectangle){ (float)(col * CW), 0,
                        (float)(strlen(BUTTONS[i].label) * CW), CH };
}

/* escreve até maxCols caracteres de s, a começar em skip, na coluna e linha indicadas */
static void putText(int col, int row, const char *s, int skip, int maxCols, Color c) {
    int len = (int)strlen(s);
    for (int i = 0; skip + i < len && i < maxCols; i++) {
        char b[2] = { s[skip + i], '\0' };
#if UPPERCASE_ONLY
        b[0] = (char)toupper((unsigned char)b[0]);
#endif
        DrawText(b, (col + i) * CW, row * CH, 20, c);
    }
}

static void typeChar(int c) {
    if (c < 32 || c > 126) return;
    buf_insertChar(&buf, c);
    dirty = true;
}

/* ---------- tema ---------- */

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
    dirty = false;
    leftCol = 0;
    pendingDiscard = ACT_NONE;
}

static void saveFile(void) {
    if (buf_save(&buf, filename)) {
        dirty = false;
        pendingDiscard = ACT_NONE;
        setStatus("GRAVADO");
    } else {
        setStatus("ERRO AO GRAVAR");
    }
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
    leftCol = 0;
    dirty = false;
    pendingDiscard = ACT_NONE;
    setStatus("NOVO FICHEIRO");
}


static bool confirmDiscard(Action a) {
    if (!dirty) return true;
    if (pendingDiscard == a) {
        pendingDiscard = ACT_NONE;
        return true;
    }
    pendingDiscard = a;
    setStatus("ALTERACOES NAO GRAVADAS: REPITA PARA CONTINUAR");
    return false;
}

/* ---------- prompt (procurar e ir para linha) ---------- */

static void openPrompt(PromptKind k) {
    prompt = k;
    promptLen = 0;
    promptText[0] = '\0';
    setStatus(k == PROMPT_FIND ? "ESCREVA O TEXTO E CARREGUE ENTER"
                               : "ESCREVA O NUMERO DA LINHA");
}

static void closePrompt(void) {
    prompt = PROMPT_NONE;
    promptLen = 0;
    promptText[0] = '\0';
}

/* procura sem distinguir maiúsculas de minúsculas */
static const char *findCI(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0) return NULL;
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] &&
               toupper((unsigned char)p[i]) == toupper((unsigned char)needle[i]))
            i++;
        if (i == nl) return p;
    }
    return NULL;
}

/* procura lastFind a partir da posição do cursor, dando a volta ao texto se preciso */
static void findNextMatch(void) {
    for (int k = 0; k <= buf.numLines; k++) {
        int r = (buf.curRow + k) % buf.numLines;
        int from = (k == 0) ? buf.curCol + 1 : 0;
        if (from > (int)strlen(buf.lines[r])) continue;
        const char *hit = findCI(buf.lines[r] + from, lastFind);
        if (hit) {
            buf.curRow = r;
            buf.curCol = (int)(hit - buf.lines[r]);
            setStatus("ENCONTRADO");
            return;
        }
    }
    setStatus("NAO ENCONTRADO");
}

static void runPrompt(void) {
    if (prompt == PROMPT_GOTO) {
        if (promptLen > 0) {
            int n = atoi(promptText);
            if (n < 1) n = 1;
            if (n > buf.numLines) n = buf.numLines;
            buf.curRow = n - 1;
            buf.curCol = 0;
            char msg[64];
            snprintf(msg, sizeof msg, "LINHA %d", n);
            setStatus(msg);
        }
        closePrompt();
    } else if (prompt == PROMPT_FIND) {
        if (promptLen > 0) snprintf(lastFind, sizeof lastFind, "%s", promptText);
        closePrompt();
        if (lastFind[0]) findNextMatch();
        else setStatus("NADA PARA PROCURAR");
    }
}

/* ---------- ações (botões e atalhos chamam sempre esta função) ---------- */

static void doAction(Action a) {
    if (prompt != PROMPT_NONE && a != ACT_FIND && a != ACT_GOTO) closePrompt();

    switch (a) {
        case ACT_OPEN:  if (confirmDiscard(a)) openAction(); break;
        case ACT_NEW:   if (confirmDiscard(a)) newAction();  break;
        case ACT_SAVE:  saveFile();                          break;
        case ACT_FIND:  openPrompt(PROMPT_FIND);             break;
        case ACT_GOTO:  openPrompt(PROMPT_GOTO);             break;
        case ACT_THEME: toggleTheme();                       break;
        case ACT_HELP:  showHelp = true;                     break;
        case ACT_QUIT:  if (confirmDiscard(a)) running = false; break;
        default: break;
    }
}

static void handleClick(Vector2 m) {
    for (int i = 0; i < N_BUTTONS; i++) {
        if (CheckCollisionPointRec(m, buttonRect(i))) {
            doAction(BUTTONS[i].act);
            return;
        }
    }

    int r = (int)(m.y / CH) - 1;
    if (r >= 0 && r < editRowsNow() && m.x >= GUTTER * CW) {
        int li = buf.topRow + r;
        if (li < buf.numLines) {
            buf.curRow = li;
            buf.curCol = leftCol + (int)(m.x / CW) - GUTTER;
            clampCol();
        }
    }
}

/* ---------- teclado ---------- */

static void promptKeys(void) {
    int c;
    while ((c = GetCharPressed()) != 0) {
        bool ok = (c >= 32 && c <= 126) &&
                  (prompt == PROMPT_FIND || (c >= '0' && c <= '9'));
        if (ok && promptLen < (int)sizeof promptText - 1) {
            promptText[promptLen++] = (char)c;
            promptText[promptLen] = '\0';
        }
    }

    if (PRESSED(KEY_BACKSPACE) && promptLen > 0)
        promptText[--promptLen] = '\0';

    if (IsKeyPressed(KEY_ESCAPE)) {
        closePrompt();
        setStatus("CANCELADO");
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        runPrompt();
}

static void normalKeys(bool ctrl) {
    int c;
    while ((c = GetCharPressed()) != 0)
        if (!ctrl) typeChar(c);

    if (ctrl) {
        if (IsKeyPressed(KEY_S)) doAction(ACT_SAVE);
        if (IsKeyPressed(KEY_O)) doAction(ACT_OPEN);
        if (IsKeyPressed(KEY_N)) doAction(ACT_NEW);
        if (IsKeyPressed(KEY_T)) doAction(ACT_THEME);
        if (IsKeyPressed(KEY_F)) doAction(ACT_FIND);
        if (IsKeyPressed(KEY_G)) doAction(ACT_GOTO);
        return;
    }

    if (IsKeyPressed(KEY_F1)) doAction(ACT_HELP);
    if (IsKeyPressed(KEY_F3)) {
        if (lastFind[0]) findNextMatch();
        else setStatus("USE CTRL+F PRIMEIRO");
    }
    if (IsKeyPressed(KEY_ESCAPE)) doAction(ACT_QUIT);

    if (PRESSED(KEY_ENTER))     { buf_newLine(&buf);    dirty = true; }
    if (PRESSED(KEY_BACKSPACE)) { buf_backspace(&buf);  dirty = true; }
    if (PRESSED(KEY_DELETE))    { buf_deleteChar(&buf); dirty = true; }
    if (PRESSED(KEY_TAB)) {
        for (int i = 0; i < 4; i++) typeChar(' ');
    }

    /* movimento */
    if (PRESSED(KEY_UP) && buf.curRow > 0) {
        buf.curRow--;
        clampCol();
    }
    if (PRESSED(KEY_DOWN) && buf.curRow < buf.numLines - 1) {
        buf.curRow++;
        clampCol();
    }  /*---apenas para mover o cursor para a esquerda---*/
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
}

/* ajusta a vista (linhas e colunas) para o cursor ficar sempre visível */
static void keepCursorVisible(void) {
    int er = editRowsNow(), tc = textColsNow();

    if (buf.curRow < buf.topRow) buf.topRow = buf.curRow;
    if (buf.curRow >= buf.topRow + er) buf.topRow = buf.curRow - er + 1;

    if (buf.curCol < leftCol) leftCol = buf.curCol;
    if (buf.curCol >= leftCol + tc) leftCol = buf.curCol - tc + 1;
}

/* ---------- desenho ---------- */

static void drawScreen(bool cursorOn) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int er = editRowsNow(), tc = textColsNow();
    Vector2 m = GetMousePosition();

    /* barra de cima com os botões */
    DrawRectangle(0, 0, sw, CH, th->bar);
    putText(1, 0, "ED-11 EDITOR", 0, 12, th->barText);
    for (int i = 0; i < N_BUTTONS; i++) {
        Rectangle r = buttonRect(i);
        bool hover = CheckCollisionPointRec(m, r);
        DrawRectangleRec(r, hover ? th->ink : th->btn);
        DrawRectangleLinesEx(r, 1, th->bar);
        putText((int)(r.x / CW), 0, BUTTONS[i].label, 0,
                (int)(r.width / CW), hover ? th->bg : th->btnText);
    }

    if (showHelp) {
        int n = (int)(sizeof HELP_LINES / sizeof HELP_LINES[0]);
        for (int i = 0; i < n && i + 2 < rows - 1; i++)
            putText(4, i + 2, HELP_LINES[i], 0, cols - 8, th->ink);
    } else {
        /* área de edição */
        for (int r = 0; r < er; r++) {
            int li = buf.topRow + r;
            int y = r + 1;

            if (li >= buf.numLines) {
                putText(GUTTER - 1, y, "~", 0, 1, Fade(th->ink, 0.3f));
                continue;
            }

            if (li == buf.curRow)
                DrawRectangle(0, y * CH, sw, CH, Fade(th->ink, 0.12f));

            char num[16];
            snprintf(num, sizeof num, "%4d", li + 1);
            putText(0, y, num, 0, GUTTER, Fade(th->ink, 0.45f));
            putText(GUTTER, y, buf.lines[li], leftCol, tc, th->ink);
        }

        /* cursor */
        if (cursorOn) {
            int sy = buf.curRow - buf.topRow + 1;
            int sx = buf.curCol - leftCol + GUTTER;
            if (sy >= 1 && sy <= er &&
                buf.curCol >= leftCol && buf.curCol < leftCol + tc) {
                char under = buf.lines[buf.curRow][buf.curCol];
                if (under == '\0') under = ' ';
                char b[2] = { under, '\0' };
                DrawRectangle(sx * CW, sy * CH, CW, CH, th->bar);
                putText(sx, sy, b, 0, 1, th->barText);
            }
        }
    }

    /* linha do prompt, só visível quando está ativa */
    if (prompt != PROMPT_NONE) {
        char pl[128];
        snprintf(pl, sizeof pl, "%s: %s_",
                 prompt == PROMPT_FIND ? "PROCURAR" : "IR PARA LINHA", promptText);
        DrawRectangle(0, (rows - 2) * CH, sw, CH, Fade(th->ink, 0.12f));
        putText(1, rows - 2, pl, 0, cols - 2, th->ink);
    }

    /* barra de estado */
    char st[768];
    snprintf(st, sizeof st, " %s%s  LIN %d/%d  COL %d  %s",
             filename, dirty ? " *" : "",
             buf.curRow + 1, buf.numLines, buf.curCol + 1, status);
    DrawRectangle(0, (rows - 1) * CH, sw, CH, th->bar);
    putText(0, rows - 1, st, 0, cols, th->barText);

    /* linhas de varrimento */
    for (int y = 0; y < sh; y += 3)
        DrawRectangle(0, y, sw, 1, (Color){ 0, 0, 0, 80 });
}

/* ---------- ciclo principal ---------- */

static void editorLoop(void) {
    while (!WindowShouldClose() && running) {
        cols = GetScreenWidth() / CW;
        rows = GetScreenHeight() / CH;
        if (cols < 20) cols = 20;
        if (rows < 6)  rows = 6;

        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (showHelp) {
            /* qualquer tecla ou clique fecha a ajuda */
            bool any = false;
            while (GetKeyPressed() != 0) any = true;
            while (GetCharPressed() != 0) any = true;
            if (any || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) showHelp = false;
        } else {
            while (GetKeyPressed() != 0) { }   /* esvazia a fila de teclas */

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                handleClick(GetMousePosition());

            if (prompt != PROMPT_NONE) promptKeys();
            else                       normalKeys(ctrl);
        }

        keepCursorVisible();
        bool cursorOn = ((int)(GetTime() * 2) % 2) == 0;

        BeginDrawing();
        ClearBackground(th->bg);
        drawScreen(cursorOn);
        EndDrawing();
    }
}

int main(int argc, char **argv) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIN_W, WIN_H, "ED-11");
    SetTargetFPS(60);
    ToggleFullscreen();

    buf_init(&buf);
    if (argc > 1) loadFile(argv[1]);
    else          loadFile(filename);

    editorLoop();

    CloseWindow();
    return 0;
}