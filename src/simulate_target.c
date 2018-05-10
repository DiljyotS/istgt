#include "istgt.h"
#include "istgt_misc.h"
#include "istgt_scsi.h"
#include "replication.h"
#include "ring_mempool.h"
#include "simulate_target.h"

int64_t
replica_write(spec_t *spec, void *buf, uint64_t offset, uint64_t nbytes)
{
	int64_t rc;
	uint8_t indx = 0;
	ISTGT_LU_CMD_Ptr cmd = (ISTGT_LU_CMD_Ptr)malloc(sizeof(ISTGT_LU_CMD_Ptr));
	cmd->luworkerindx = indx;
	cmd->cdb0 = SBC_WRITE_6;
	cmd->iobufindx = 2;
	cmd->iobuf[1].iov_base = buf;
	cmd->iobuf[1].iov_len = nbytes;
	rc = replicate(spec, cmd, offset, nbytes);
	free(cmd);
	return rc;
}

int64_t
replica_read(spec_t *spec, void **buf, uint64_t offset, uint64_t nbytes)
{
	int64_t rc;
	uint8_t indx = 0;
	ISTGT_LU_CMD_Ptr cmd = (ISTGT_LU_CMD_Ptr)malloc(sizeof(ISTGT_LU_CMD_Ptr));
	cmd->luworkerindx = indx;
	cmd->cdb0 = SBC_READ_6;
	cmd->iobufindx = 1;
	rc = replicate(spec, cmd, offset, nbytes);
	*buf = cmd->data;
	free(cmd);
	return rc;
}


spec_t *
init_target(void)
{
	spec_t *spec = (spec_t *)malloc(sizeof(spec_t));
	initialize_replication();
	initialize_volume(spec);
	return spec;
}

int
main(void)
{
	spec_t *spec = init_target();
	return 0;
}
