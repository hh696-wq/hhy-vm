#ifndef HHY_RUNTIME_OWNERSHIP_H
#define HHY_RUNTIME_OWNERSHIP_H

/* Internal ownership vocabulary. These annotations intentionally have no ABI
 * effect; they make reviews and static governance checks explicit.
 *
 * HHY_BORROWED:         valid only while its owner remains alive.
 * HHY_MANAGED_SCANNED:  GC-managed storage that may contain managed pointers.
 * HHY_MANAGED_ATOMIC:   GC-managed bytes that must not contain pointers.
 * HHY_NATIVE_OWNED:     native/library resource requiring explicit release.
 */
#define HHY_BORROWED
#define HHY_MANAGED_SCANNED
#define HHY_MANAGED_ATOMIC
#define HHY_NATIVE_OWNED

#endif
