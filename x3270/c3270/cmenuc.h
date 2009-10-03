extern Boolean menu_is_up;
extern void cmenu_init(void);
extern Boolean menu_char(int row, int col, ucs4_t *u, Boolean *highlighted);
void menu_key();
typedef struct cmenu cmenu_t;
extern cmenu_t *file_menu, *options_menu, *keypad_menu;
extern void popup_menu(int x);
extern void menu_cursor(int *row, int *col);
