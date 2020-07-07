#ifndef KSYMS_H
#define KSYMS_H

#define KSYMDEC(x) extern typeof(&x) _##x;
#define KSYMDEF(x) void *_##x = NULL;
#define KSYM(x) ((typeof(&x)_##x)
#define KSYMINIT(x) (_##x = (void *)kallsyms_lookup_name(#x))
#define KSYMINIT_FAULT(x) if (!KSYMINIT(x)) return EBADF

#endif
