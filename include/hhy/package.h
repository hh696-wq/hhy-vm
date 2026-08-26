#ifndef HHY_PACKAGE_H
#define HHY_PACKAGE_H

#include <stdbool.h>

int hhy_package_install(const char *source, bool assume_yes);
int hhy_package_list(void);
int hhy_package_remove(const char *name);
bool hhy_package_home(char *out, unsigned long size);
bool hhy_package_verify(const char *name, const char **error);

#endif
