#ifndef KSYMS_H
#define KSYMS_H

#define KSYMDEC(x) extern typeof(&x) _##x;
#define KSYMDEF(x) typeof(&x) _##x = NULL;
#define KSYM(x) _##x
#define KSYMINIT(x) _##x = (typeof(&x))kallsyms_lookup_name(#x)

#endif
