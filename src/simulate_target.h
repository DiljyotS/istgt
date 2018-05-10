#include "replication.h"

int64_t replica_write(spec_t *spec, void *buf, uint64_t offset, uint64_t nbytes);
int64_t replica_read(spec_t *spec, void **buf, uint64_t offset, uint64_t nbytes);
spec_t *init_target(void);
