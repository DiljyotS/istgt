
#include "istgt.h"
#include "istgt_misc.h"
#include "istgt_scsi.h"
#include "replication.h"
#include "ring_mempool.h"

#define	RCOMMON_CMD_MEMPOOL_ENTRIES	100000
rte_smempool_t rcommon_cmd_mempool;
size_t rcommon_cmd_mempool_count = RCOMMON_CMD_MEMPOOL_ENTRIES;

#define build_rcomm_cmd \
	{\
		rcomm_cmd = get_from_mempool(&rcommon_cmd_mempool);\
		rc = pthread_mutex_init(&rcomm_cmd->rcommand_mtx, NULL);\
		rcomm_cmd->total = 0;\
		rcomm_cmd->copies_sent = 0;\
		rcomm_cmd->total_len = 0;\
		rcomm_cmd->offset = offset;\
		rcomm_cmd->data_len = nbytes;\
		rcomm_cmd->state = CMD_CREATED;\
		rcomm_cmd->luworker_id = cmd->luworkerindx;\
		rcomm_cmd->acks_recvd = 0;\
		rcomm_cmd->status = 0;\
		rcomm_cmd->completed = false; \
		switch(cmd->cdb0) {\
			case SBC_WRITE_6:  \
			case SBC_WRITE_10: \
			case SBC_WRITE_12: \
			case SBC_WRITE_16: \
				cmd_write = true; \
				break; \
			default: \
				break;\
		}\
		if(cmd_write) {\
			rcomm_cmd->opcode = ZVOL_OPCODE_WRITE;\
			rcomm_cmd->iovcnt = cmd->iobufindx+1;\
		} else {\
			TAILQ_INIT(&rcomm_cmd->data_read_ptr);\
			rcomm_cmd->opcode = ZVOL_OPCODE_READ;\
			rcomm_cmd->iovcnt = 0;\
		}\
		if(cmd_write) {\
			for (i=1; i < iovcnt + 1; i++) {\
				rcomm_cmd->iov[i].iov_base = cmd->iobuf[i-1].iov_base;\
				rcomm_cmd->iov[i].iov_len = cmd->iobuf[i-1].iov_len;\
			}\
			rcomm_cmd->total_len += cmd->iobufsize;\
		}\
	}

#ifdef REPLICATION
void
clear_rcomm_cmd(rcommon_cmd_t *rcomm_cmd)
{
	int i;
	pthread_mutex_destroy(&(rcomm_cmd->rcommand_mtx));
	for (i=1; i<rcomm_cmd->iovcnt + 1; i++)
		xfree(rcomm_cmd->iov[i].iov_base);
	put_to_mempool(&rcommon_cmd_mempool, rcomm_cmd);
}


int64_t
replicate(ISTGT_LU_DISK *spec, ISTGT_LU_CMD_Ptr cmd, uint64_t offset, uint64_t nbytes)
{
	int rc, status, i;
	bool cmd_write = false;
	rcommon_cmd_t *rcomm_cmd;
	int iovcnt = cmd->iobufindx+1;
	build_rcomm_cmd;
	cmd->data = NULL;
	MTX_LOCK(&spec->rcommonq_mtx);
	if(spec->ready == false) {
		REPLICA_LOG("SPEC is not ready\n");
		MTX_UNLOCK(&spec->rcommonq_mtx);
		clear_rcomm_cmd(rcomm_cmd);
		return -1;
	}
	TAILQ_INSERT_TAIL(&spec->rcommon_sendq, rcomm_cmd, send_cmd_next);
	pthread_cond_signal(&spec->rcommonq_cond);
	MTX_LOCK(&spec->luworker_rmutex[cmd->luworkerindx]);
	MTX_UNLOCK(&spec->rcommonq_mtx);
	if (rcomm_cmd->status == 0) //assuming status won't be 0 after IO completion
		pthread_cond_wait(&spec->luworker_rcond[cmd->luworkerindx], &spec->luworker_rmutex[cmd->luworkerindx]);
	MTX_UNLOCK(&spec->luworker_rmutex[cmd->luworkerindx]);
	MTX_LOCK(&spec->rcommonq_mtx);
	MTX_LOCK(&rcomm_cmd->rcommand_mtx);
	status = rcomm_cmd->status;
	if(status == -1 )  {
		REPLICA_LOG("STATUS = -1\n");
		if(rcomm_cmd->completed) {
			MTX_UNLOCK(&rcomm_cmd->rcommand_mtx);
			clear_rcomm_cmd(rcomm_cmd);
		} else {
			rcomm_cmd->completed = true;
			MTX_UNLOCK(&rcomm_cmd->rcommand_mtx);
		}
		cmd->data = NULL;
		rc = -1;
		MTX_UNLOCK(&spec->rcommonq_mtx);
		return -1;
	}
	rc = cmd->data_len = rcomm_cmd->data_len;
	cmd->data = rcomm_cmd->data;
	if(rcomm_cmd->completed) {
		MTX_UNLOCK(&rcomm_cmd->rcommand_mtx);
		MTX_UNLOCK(&spec->rcommonq_mtx);
		clear_rcomm_cmd(rcomm_cmd);
	} else {
		rcomm_cmd->completed = true;
		MTX_UNLOCK(&rcomm_cmd->rcommand_mtx);
		MTX_UNLOCK(&spec->rcommonq_mtx);
	}
	return rc;
}
#endif
