/*-
 * Copyright (c) 2014-2017 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#define	WT_OPTRACK_MAXRECS 16384
#define	WT_OPTRACK_BUFSIZE WT_OPTRACK_MAXRECS * sizeof(WT_OPTRACK_RECORD)
#define	WT_OPTRACK_VERSION 1

/*
 * WT_OPTRACK_HEADER --
 *     A header in the operation tracking log file. The internal session
 *     identifier is a boolean: 1 if the session is internal, 0 otherwise.
 */
struct __wt_optrack_header {
	uint32_t optrack_version;
	uint32_t optrack_session_internal;
};

/*
 * WT_OPTRACK_RECORD --
 *     A structure for logging function entry and exit events.
 *
 * We pad the record so that the size of the entire record is three double
 * words, or 24 bytes. If we don't do this, the compiler will pad it for us,
 * because we keep records in the record buffer array and each new record must
 * be aligned on the 8-byte boundary, since its first element is an 8-byte
 * timestamp. Instead of letting the compiler insert the padding silently, we
 * pad explicitly, so that whoever writes the binary decoder can refer to this
 * struct to find out the record size.
 *
 * The operation id included in this structure is a unique address of a function
 * in the binary. As we log operations, we keep track of the correspondence
 * between function addresses and their names. When the log file is decoded,
 * operations identifiers are replaced with function names. Therefore, the
 * present design assumes that the user will be inserting the tracking macros
 * on function boundaries: when we enter into the function and when we exit
 * from it.
 */
struct __wt_optrack_record {
	uint64_t timestamp;
	uint64_t op_id;
	uint16_t op_type;
	char padding[6];
};

#define	WT_TRACK_OP(s, optype)						\
	tr->timestamp = __wt_rdtsc(s);					\
	tr->op_type = optype;						\
	tr->op_id = (uint64_t)&__func__;				\
									\
	if (optype == 0 && !id_recorded)				\
		__wt_optrack_record_funcid(s, tr->op_id,		\
		    (void*)&__func__, sizeof(__func__), &id_recorded);	\
	if (s->optrackbuf_ptr == WT_OPTRACK_MAXRECS) {			\
		s->optrack_offset += __wt_optrack_flush_buffer(s);	\
		s->optrackbuf_ptr = 0;					\
	}

/*
 * We do not synchronize access to optrack buffer pointer under the assumption
 * that there is no more than one thread using a given session. This assumption
 * does not always hold. When it does not, we might have a race. In this case,
 * we may lose a few log records. We prefer to risk losing a few log records
 * occasionally in order not to synchronize this code, which is intended to be
 * very lightweight.
 * Exclude the default session (ID 0) because it can be used by multiple
 * threads and it is also used in error paths during failed open calls.
 */
#define	WT_TRACK_OP_INIT(s)						\
	WT_OPTRACK_RECORD *tr;						\
	static volatile bool id_recorded = false;			\
	if (F_ISSET(S2C(s), WT_CONN_OPTRACK) && (s)->id != 0) {		\
		tr = &(s->optrack_buf[s->optrackbuf_ptr % WT_OPTRACK_MAXRECS]);\
		s->optrackbuf_ptr++;					\
		WT_TRACK_OP(s, 0);					\
	}

#define	WT_TRACK_OP_END(s)						\
	if (F_ISSET(S2C(s), WT_CONN_OPTRACK) && (s)->id != 0) {		\
		tr = &(s->optrack_buf[s->optrackbuf_ptr % WT_OPTRACK_MAXRECS]);\
		s->optrackbuf_ptr++;					\
		WT_TRACK_OP(s, 1);					\
	}
