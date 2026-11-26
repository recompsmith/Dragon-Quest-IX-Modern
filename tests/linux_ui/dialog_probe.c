#include "menu.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char* ui_tr(const char* text) { return text; }

int main(void) {
    if (getenv("DIALOG_TEST_CLOSE_STDOUT")) close(STDOUT_FILENO);
    char* text = ask_text(NULL, "Cheats", "Montant d'or : $(touch SHOULD_NOT_EXIST)");
    if (getenv("DIALOG_TEST_CANCEL")) assert(text == NULL);
    else assert(text && !strcmp(text, getenv("DIALOG_TEST_VALUE")));
    free(text);
    return 0;
}
