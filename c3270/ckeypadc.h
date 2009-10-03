/* Global declarations for keypad.c (which should be ckeypad.c) */
extern Boolean keypad_is_up;
extern Boolean keypad_char(int row, int col, ucs4_t *u, Boolean *highlighted);
extern void keypad_cursor(int *row, int *col);
extern void pop_up_keypad(Boolean up);
extern void keypad_key(int k, ucs4_t u);
