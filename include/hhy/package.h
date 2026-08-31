#ifndef HHY_PACKAGE_H
#define HHY_PACKAGE_H

#include <stdbool.h>

typedef struct {
    bool assume_yes;
    bool dry_run;
    const char *registry;
    const char *trust_root;
} HhyPackageInstallOptions;

int hhy_package_install(const char *source, const HhyPackageInstallOptions *options);
int hhy_registry_install(const char *identity, const HhyPackageInstallOptions *options);
int hhy_package_list(void);
int hhy_package_remove(const char *name);
bool hhy_package_home(char *out, unsigned long size);
bool hhy_package_verify(const char *name, const char **error);

#endif
