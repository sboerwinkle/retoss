
extern char writeFile(const char *name, const list<char> *data);
extern char readFile(const char *name, list<char> *out);

// Like `writeFile` / `readFile`, but these don't restrict to the `data/` directory.
// They can also be called outside of `file_init` ... `file_destroy`.
extern char writeSystemFile(const char *name, const list<char> *data);
extern char readSystemFile(const char *name, list<char> *out);

extern void file_init();
extern void file_destroy();
