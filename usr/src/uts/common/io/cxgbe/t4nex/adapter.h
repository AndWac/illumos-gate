/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source. A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * This file is part of the Chelsio T4 support code.
 *
 * Copyright (C) 2011-2013 Chelsio Communications.  All rights reserved.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the LICENSE file included in this
 * release for licensing terms and conditions.
 */

/*
 * Copyright 2024 Oxide Computer Company
 */

#ifndef __CXGBE_ADAPTER_H
#define	__CXGBE_ADAPTER_H

#include <sys/ddi.h>
#include <sys/mac_provider.h>
#include <sys/ethernet.h>
#include <sys/queue.h>
#include <sys/containerof.h>
#include <sys/ddi_ufm.h>

#include "firmware/t4fw_interface.h"
#include "shared.h"

struct adapter;
typedef struct adapter adapter_t;

#define	FW_IQ_QSIZE	256
#define	FW_IQ_ESIZE	64	/* At least 64 mandated by the firmware spec */

#define	RX_IQ_QSIZE	1024
#define	RX_IQ_ESIZE	64	/* At least 64 so CPL_RX_PKT will fit */

#define	EQ_ESIZE	64	/* All egress queues use this entry size */

#define	RX_FL_ESIZE	64	/* 8 64bit addresses */

#define	FL_BUF_SIZES	4

#define	CTRL_EQ_QSIZE	128

#define	TX_EQ_QSIZE	1024
#define	TX_SGL_SEGS	36
#define	TX_WR_FLITS	(SGE_MAX_WR_LEN / 8)

#define	UDBS_SEG_SHIFT	7	/* log2(UDBS_SEG_SIZE) */
#define	UDBS_DB_OFFSET	8	/* offset of the 4B doorbell in a segment */
#define	UDBS_WR_OFFSET	64	/* offset of the work request in a segment */

typedef enum t4_port_flags {
	TPF_INIT_DONE	= (1 << 0),
} t4_port_flags_t;

typedef enum t4_port_feat {
	CXGBE_HW_LSO	= (1 << 0),
	CXGBE_HW_CSUM	= (1 << 1),
} t4_port_feat_t;

struct port_info {
	dev_info_t *dip;
	mac_handle_t mh;
	mac_callbacks_t *mc;
	void *props;
	int mtu;
	uint8_t hw_addr[ETHERADDRL];

	kmutex_t lock;
	struct adapter *adapter;

	t4_port_flags_t flags;

	uint16_t viid;
	int16_t  xact_addr_filt; /* index of exact MAC address filter */
	uint16_t rss_size;	/* size of VI's RSS table slice */
	uint16_t ntxq;		/* # of tx queues */
	uint16_t first_txq;	/* index of first tx queue */
	uint16_t nrxq;		/* # of rx queues */
	uint16_t first_rxq;	/* index of first rx queue */
	uint8_t  lport;		/* associated offload logical port */
	int8_t   mdio_addr;
	uint8_t  port_type;
	uint8_t  mod_type;
	uint8_t  port_id;
	uint8_t  tx_chan;
	uint8_t  rx_chan;
	uint8_t  rx_cchan;
	uint8_t instance; /* Associated adapter instance */
	uint8_t child_inst; /* Associated child instance */
	uint8_t	tmr_idx;
	int8_t	pktc_idx;
	struct link_config link_cfg;
	struct port_stats stats;
	t4_port_feat_t features;
	uint8_t macaddr_cnt;
	u8 rss_mode;
	u16 viid_mirror;
	kstat_t *ksp_config;
	kstat_t *ksp_info;
	kstat_t *ksp_fec;

	u8 vivld;
	u8 vin;
	u8 smt_idx;

	u8 vivld_mirror;
	u8 vin_mirror;
	u8 smt_idx_mirror;
};

struct fl_sdesc {
	struct rxbuf *rxb;
};

struct tx_desc {
	__be64 flit[8];
};

/* DMA maps used for tx */
struct tx_maps {
	ddi_dma_handle_t *map;
	uint32_t map_total;	/* # of DMA maps */
	uint32_t map_pidx;	/* next map to be used */
	uint32_t map_cidx;	/* reclaimed up to this index */
	uint32_t map_avail;	/* # of available maps */
};

struct tx_sdesc {
	mblk_t *m;
	uint32_t txb_used;	/* # of bytes of tx copy buffer used */
	uint16_t hdls_used;	/* # of dma handles used */
	uint16_t desc_used;	/* # of hardware descriptors used */
};

typedef enum t4_iq_flags {
	IQ_ALLOCATED	= (1 << 0),	/* firmware resources allocated */
	IQ_INTR		= (1 << 1),	/* iq takes direct interrupt */
	IQ_HAS_FL	= (1 << 2),	/* iq has fl */
} t4_iq_flags_t;

typedef enum t4_iq_state {
	IQS_DISABLED	= 0,
	IQS_BUSY	= 1,
	IQS_IDLE	= 2,
} t4_iq_state_t;

struct rxbuf_cache_params {
	dev_info_t		*dip;
	ddi_dma_attr_t		dma_attr_rx;
	ddi_device_acc_attr_t	acc_attr_rx;
	size_t			buf_size;
};

/*
 * Ingress Queue: T4 is producer, driver is consumer.
 */
struct sge_iq {
	t4_iq_state_t state;
	t4_iq_flags_t flags;

	ddi_dma_handle_t dhdl;
	ddi_acc_handle_t ahdl;

	__be64 *desc;		/* KVA of descriptor ring */
	uint64_t ba;		/* bus address of descriptor ring */
	const __be64 *cdesc;	/* current descriptor */
	struct adapter *adapter; /* associated  adapter */
	uint8_t  gen;		/* generation bit */
	uint8_t  intr_params;	/* interrupt holdoff parameters */
	int8_t   intr_pktc_idx;	/* packet count threshold index */
	uint8_t  intr_next;	/* holdoff for next interrupt */
	uint8_t  esize;		/* size (bytes) of each entry in the queue */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer index */
	uint16_t pending;	/* # of descs processed since last doorbell */
	uint16_t cntxt_id;	/* SGE context id  for the iq */
	uint16_t abs_id;	/* absolute SGE id for the iq */
	kmutex_t lock;		/* Rx access lock */
	uint8_t polling;

	STAILQ_ENTRY(sge_iq) link;
};

typedef enum t4_eq_flags {
	EQ_ALLOCATED	= (1 << 0),	/* firmware resources allocated */
	EQ_MTX		= (1 << 1),	/* mutex has been initialized */
} t4_eq_flags_t;

/* Listed in order of preference. */
typedef enum t4_doorbells {
	DOORBELL_UDB	= (1 << 0),
	DOORBELL_WCWR	= (1 << 1),
	DOORBELL_UDBWC	= (1 << 2),
	DOORBELL_KDB	= (1 << 3),
} t4_doorbells_t;

/*
 * Egress Queue: driver is producer, T4 is consumer.
 *
 * Note: A free list is an egress queue (driver produces the buffers and T4
 * consumes them) but it's special enough to have its own struct (see sge_fl).
 */
struct sge_eq {
	ddi_dma_handle_t desc_dhdl;
	ddi_acc_handle_t desc_ahdl;
	t4_eq_flags_t flags;
	kmutex_t lock;

	struct tx_desc *desc;	/* KVA of descriptor ring */
	uint64_t ba;		/* bus address of descriptor ring */
	struct sge_qstat *spg;	/* status page, for convenience */
	t4_doorbells_t doorbells;
	volatile uint32_t *udb; /* KVA of doorbell (lies within BAR2) */
	uint_t udb_qid;		/* relative qid within the doorbell page */
	uint16_t cap;		/* max # of desc, for convenience */
	uint16_t avail;		/* available descriptors, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer idx (desc idx) */
	uint16_t pidx;		/* producer idx (desc idx) */
	uint16_t pending;	/* # of descriptors used since last doorbell */
	uint16_t iqid;		/* iq that gets egr_update for the eq */
	uint8_t tx_chan;	/* tx channel used by the eq */
	uint32_t cntxt_id;	/* SGE context id for the eq */
};

typedef enum t4_fl_flags {
	FL_MTX		= (1 << 0),	/* mutex has been initialized */
	FL_STARVING	= (1 << 1),	/* on the list of starving fl's */
	FL_DOOMED	= (1 << 2),	/* about to be destroyed */
} t4_fl_flags_t;

#define	FL_RUNNING_LOW(fl)	(fl->cap - fl->needed <= fl->lowat)
#define	FL_NOT_RUNNING_LOW(fl)	(fl->cap - fl->needed >= 2 * fl->lowat)

struct sge_fl {
	t4_fl_flags_t flags;
	kmutex_t lock;
	ddi_dma_handle_t dhdl;
	ddi_acc_handle_t ahdl;

	__be64 *desc;		/* KVA of descriptor ring, ptr to addresses */
	uint64_t ba;		/* bus address of descriptor ring */
	struct fl_sdesc *sdesc;	/* KVA of software descriptor ring */
	uint32_t cap;		/* max # of buffers, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cntxt_id;	/* SGE context id for the freelist */
	uint32_t cidx;		/* consumer idx (buffer idx, NOT hw desc idx) */
	uint32_t pidx;		/* producer idx (buffer idx, NOT hw desc idx) */
	uint32_t needed;	/* # of buffers needed to fill up fl. */
	uint32_t lowat;		/* # of buffers <= this means fl needs help */
	uint32_t pending;	/* # of bufs allocated since last doorbell */
	uint32_t offset;	/* current packet within the larger buffer */
	uint16_t copy_threshold; /* anything this size or less is copied up */

	uint64_t copied_up;	/* # of frames copied into mblk and handed up */
	uint64_t passed_up;	/* # of frames wrapped in mblk and handed up */
	uint64_t allocb_fail;	/* # of mblk allocation failures */

	TAILQ_ENTRY(sge_fl) link; /* All starving freelists */
};

/* txq: SGE egress queue + miscellaneous items */
struct sge_txq {
	struct sge_eq eq;	/* MUST be first */

	struct port_info *port;	/* the port this txq belongs to */
	struct tx_sdesc *sdesc;	/* KVA of software descriptor ring */
	mac_ring_handle_t ring_handle;

	/* DMA handles used for tx */
	ddi_dma_handle_t *tx_dhdl;
	uint32_t tx_dhdl_total;	/* Total # of handles */
	uint32_t tx_dhdl_pidx;	/* next handle to be used */
	uint32_t tx_dhdl_cidx;	/* reclaimed up to this index */
	uint32_t tx_dhdl_avail;	/* # of available handles */

	/* Copy buffers for tx */
	ddi_dma_handle_t txb_dhdl;
	ddi_acc_handle_t txb_ahdl;
	caddr_t txb_va;		/* KVA of copy buffers area */
	uint64_t txb_ba;	/* bus address of copy buffers area */
	uint32_t txb_size;	/* total size */
	uint32_t txb_next;	/* offset of next useable area in the buffer */
	uint32_t txb_avail;	/* # of bytes available */
	uint16_t copy_threshold; /* anything this size or less is copied up */

	uint64_t txpkts;	/* # of ethernet packets */
	uint64_t txbytes;	/* # of ethernet bytes */
	kstat_t *ksp;

	/* stats for common events first */

	uint64_t txcsum;	/* # of times hardware assisted with checksum */
	uint64_t tso_wrs;	/* # of IPv4 TSO work requests */
	uint64_t imm_wrs;	/* # of work requests with immediate data */
	uint64_t sgl_wrs;	/* # of work requests with direct SGL */
	uint64_t txpkt_wrs;	/* # of txpkt work requests (not coalesced) */
	uint64_t txpkts_wrs;	/* # of coalesced tx work requests */
	uint64_t txpkts_pkts;	/* # of frames in coalesced tx work requests */
	uint64_t txb_used;	/* # of tx copy buffers used (64 byte each) */
	uint64_t hdl_used;	/* # of DMA handles used */

	/* stats for not-that-common events */

	uint32_t txb_full;	/* txb ran out of space */
	uint32_t dma_hdl_failed; /* couldn't obtain DMA handle */
	uint32_t dma_map_failed; /* couldn't obtain DMA mapping */
	uint32_t qfull;		/* out of hardware descriptors */
	uint32_t qflush;	/* # of SGE_EGR_UPDATE notifications for txq */
	uint32_t pullup_early;	/* # of pullups before starting frame's SGL */
	uint32_t pullup_late;	/* # of pullups while building frame's SGL */
	uint32_t pullup_failed;	/* # of failed pullups */
	uint32_t csum_failed;	/* # of csum reqs we failed to fulfill */
};

/* rxq: SGE ingress queue + SGE free list + miscellaneous items */
struct sge_rxq {
	struct sge_iq iq;	/* MUST be first */
	struct sge_fl fl;

	struct port_info *port;	/* the port this rxq belongs to */
	kstat_t *ksp;

	mac_ring_handle_t ring_handle;
	uint64_t ring_gen_num;

	/* stats for common events first */

	uint64_t rxcsum;	/* # of times hardware assisted with checksum */
	uint64_t rxpkts;	/* # of ethernet packets */
	uint64_t rxbytes;	/* # of ethernet bytes */

	/* stats for not-that-common events */

	uint32_t nomem;		/* mblk allocation during rx failed */
};

struct sge {
	int fl_starve_threshold;
	int s_qpp;

	int nrxq;	/* total rx queues (all ports and the rest) */
	int ntxq;	/* total tx queues (all ports and the rest) */
	int niq;	/* total ingress queues */
	int neq;	/* total egress queues */
	int stat_len;	/* length of status page at ring end */
	int pktshift;	/* padding between CPL & packet data */
	int fl_align;	/* response queue message alignment */

	struct sge_iq fwq;	/* Firmware event queue */
	struct sge_txq *txq;	/* NIC tx queues */
	struct sge_rxq *rxq;	/* NIC rx queues */

	int iq_start; /* iq context id map start index */
	int eq_start; /* eq context id map start index */
	int iqmap_sz; /* size of iq context id map */
	int eqmap_sz; /* size of eq context id map */
	struct sge_iq **iqmap;	/* iq->cntxt_id to iq mapping */
	struct sge_eq **eqmap;	/* eq->cntxt_id to eq mapping */

	/* Device access and DMA attributes for all the descriptor rings */
	ddi_device_acc_attr_t acc_attr_desc;
	ddi_dma_attr_t	dma_attr_desc;

	/* Device access and DMA attributes for tx buffers */
	ddi_device_acc_attr_t acc_attr_tx;
	ddi_dma_attr_t	dma_attr_tx;

	/* Device access and DMA attributes for rx buffers are in rxb_params */
	kmem_cache_t *rxbuf_cache;
	struct rxbuf_cache_params rxb_params;
};

struct driver_properties {
	/* There is a driver.conf variable for each of these */
	int max_ntxq_10g;
	int max_nrxq_10g;
	int max_ntxq_1g;
	int max_nrxq_1g;
	int intr_types;
	int tmr_idx_10g;
	int pktc_idx_10g;
	int tmr_idx_1g;
	int pktc_idx_1g;
	int qsize_txq;
	int qsize_rxq;

	int timer_val[SGE_NTIMERS];
	int counter_val[SGE_NCOUNTERS];

	int wc;

	int multi_rings;
	int t4_fw_install;
};

struct t4_mbox_list {
	STAILQ_ENTRY(t4_mbox_list) link;
};

typedef enum t4_adapter_flags {
	TAF_INIT_DONE	= (1 << 0),
	TAF_FW_OK	= (1 << 1),
	TAF_INTR_FWD	= (1 << 2),
	TAF_INTR_ALLOC	= (1 << 3),
	TAF_MASTER_PF	= (1 << 4),

	TAF_BUSY	= (1 << 9),
} t4_adapter_flags_t;

struct adapter {
	SLIST_ENTRY(adapter) link;
	dev_info_t *dip;
	dev_t dev;

	unsigned int pf;
	unsigned int mbox;

	unsigned int vpd_busy;
	unsigned int vpd_flag;

	u32 t4_bar0;

	uint_t open;	/* character device is open */

	/* PCI config space access handle */
	ddi_acc_handle_t pci_regh;

	/* MMIO register access handle */
	ddi_acc_handle_t regh;
	caddr_t regp;
	/* BAR1 register access handle */
	ddi_acc_handle_t reg1h;
	caddr_t reg1p;

	/* Interrupt information */
	int intr_type;
	int intr_count;
	int intr_cap;
	uint_t intr_pri;
	ddi_intr_handle_t *intr_handle;

	struct driver_properties props;
	kstat_t *ksp;
	kstat_t *ksp_stat;

	struct sge sge;

	struct port_info *port[MAX_NPORTS];
	ddi_taskq_t *tq[NCHAN];
	uint8_t chan_map[NCHAN];
	uint32_t filter_mode;

	t4_adapter_flags_t flags;
	t4_doorbells_t doorbells;
	int registered_device_map;
	int open_device_map;

	unsigned int cfcsum;
	struct adapter_params params;

	uint16_t linkcaps;
	uint16_t niccaps;
	uint16_t toecaps;
	uint16_t rdmacaps;
	uint16_t iscsicaps;
	uint16_t fcoecaps;

	kmutex_t lock;
	kcondvar_t cv;

	/* Starving free lists */
	kmutex_t sfl_lock;	/* same cache-line as sc_lock? but that's ok */
	TAILQ_HEAD(, sge_fl) sfl;
	timeout_id_t sfl_timer;

	/* Sensors */
	id_t temp_sensor;
	id_t volt_sensor;

	ddi_ufm_handle_t *ufm_hdl;

	/* support for single-threading access to adapter mailbox registers */
	kmutex_t mbox_lock;
	STAILQ_HEAD(, t4_mbox_list) mbox_list;
};

struct memwin {
	uint32_t base;
	uint32_t aperture;
};

#define	ADAPTER_LOCK(sc)		mutex_enter(&(sc)->lock)
#define	ADAPTER_UNLOCK(sc)		mutex_exit(&(sc)->lock)
#define	ADAPTER_LOCK_ASSERT_OWNED(sc)	ASSERT(mutex_owned(&(sc)->lock))
#define	ADAPTER_LOCK_ASSERT_NOTOWNED(sc) ASSERT(!mutex_owned(&(sc)->lock))

#define	PORT_LOCK(pi)			mutex_enter(&(pi)->lock)
#define	PORT_UNLOCK(pi)			mutex_exit(&(pi)->lock)
#define	PORT_LOCK_ASSERT_OWNED(pi)	ASSERT(mutex_owned(&(pi)->lock))
#define	PORT_LOCK_ASSERT_NOTOWNED(pi)	ASSERT(!mutex_owned(&(pi)->lock))

#define	IQ_LOCK(iq)			mutex_enter(&(iq)->lock)
#define	IQ_UNLOCK(iq)			mutex_exit(&(iq)->lock)
#define	IQ_LOCK_ASSERT_OWNED(iq)	ASSERT(mutex_owned(&(iq)->lock))
#define	IQ_LOCK_ASSERT_NOTOWNED(iq)	ASSERT(!mutex_owned(&(iq)->lock))

#define	FL_LOCK(fl)			mutex_enter(&(fl)->lock)
#define	FL_UNLOCK(fl)			mutex_exit(&(fl)->lock)
#define	FL_LOCK_ASSERT_OWNED(fl)	ASSERT(mutex_owned(&(fl)->lock))
#define	FL_LOCK_ASSERT_NOTOWNED(fl)	ASSERT(!mutex_owned(&(fl)->lock))

#define	RXQ_LOCK(rxq)			IQ_LOCK(&(rxq)->iq)
#define	RXQ_UNLOCK(rxq)			IQ_UNLOCK(&(rxq)->iq)
#define	RXQ_LOCK_ASSERT_OWNED(rxq)	IQ_LOCK_ASSERT_OWNED(&(rxq)->iq)
#define	RXQ_LOCK_ASSERT_NOTOWNED(rxq)	IQ_LOCK_ASSERT_NOTOWNED(&(rxq)->iq)

#define	RXQ_FL_LOCK(rxq)		FL_LOCK(&(rxq)->fl)
#define	RXQ_FL_UNLOCK(rxq)		FL_UNLOCK(&(rxq)->fl)
#define	RXQ_FL_LOCK_ASSERT_OWNED(rxq)	FL_LOCK_ASSERT_OWNED(&(rxq)->fl)
#define	RXQ_FL_LOCK_ASSERT_NOTOWNED(rxq) FL_LOCK_ASSERT_NOTOWNED(&(rxq)->fl)

#define	EQ_LOCK(eq)			mutex_enter(&(eq)->lock)
#define	EQ_UNLOCK(eq)			mutex_exit(&(eq)->lock)
#define	EQ_LOCK_ASSERT_OWNED(eq)	ASSERT(mutex_owned(&(eq)->lock))
#define	EQ_LOCK_ASSERT_NOTOWNED(eq)	ASSERT(!mutex_owned(&(eq)->lock))

#define	TXQ_LOCK(txq)			EQ_LOCK(&(txq)->eq)
#define	TXQ_UNLOCK(txq)			EQ_UNLOCK(&(txq)->eq)
#define	TXQ_LOCK_ASSERT_OWNED(txq)	EQ_LOCK_ASSERT_OWNED(&(txq)->eq)
#define	TXQ_LOCK_ASSERT_NOTOWNED(txq)	EQ_LOCK_ASSERT_NOTOWNED(&(txq)->eq)

#define	for_each_txq(pi, iter, txq) \
	txq = &pi->adapter->sge.txq[pi->first_txq]; \
	for (iter = 0; iter < pi->ntxq; ++iter, ++txq)
#define	for_each_rxq(pi, iter, rxq) \
	rxq = &pi->adapter->sge.rxq[pi->first_rxq]; \
	for (iter = 0; iter < pi->nrxq; ++iter, ++rxq)

#define	NFIQ(sc) ((sc)->intr_count > 1 ? (sc)->intr_count - 1 : 1)

/* One for errors, one for firmware events */
#define	T4_EXTRA_INTR 2

static inline void t4_mbox_list_add(struct adapter *adap,
				    struct t4_mbox_list *entry)
{
	mutex_enter(&adap->mbox_lock);
	STAILQ_INSERT_TAIL(&adap->mbox_list, entry, link);
	mutex_exit(&adap->mbox_lock);
}

static inline void t4_mbox_list_del(struct adapter *adap,
				    struct t4_mbox_list *entry)
{
	mutex_enter(&adap->mbox_lock);
	STAILQ_REMOVE(&adap->mbox_list, entry, t4_mbox_list, link);
	mutex_exit(&adap->mbox_lock);
}

static inline struct t4_mbox_list *
t4_mbox_list_first_entry(struct adapter *adap)
{
	return (STAILQ_FIRST(&adap->mbox_list));
}

static inline uint32_t
t4_read_reg(struct adapter *sc, uint32_t reg)
{
	/* LINTED: E_BAD_PTR_CAST_ALIGN */
	return (ddi_get32(sc->regh, (uint32_t *)(sc->regp + reg)));
}

static inline void
t4_write_reg(struct adapter *sc, uint32_t reg, uint32_t val)
{
	/* LINTED: E_BAD_PTR_CAST_ALIGN */
	ddi_put32(sc->regh, (uint32_t *)(sc->regp + reg), val);
}

static inline void
t4_os_pci_read_cfg1(struct adapter *sc, int reg, uint8_t *val)
{
	*val = pci_config_get8(sc->pci_regh, reg);
}

static inline void
t4_os_pci_write_cfg1(struct adapter *sc, int reg, uint8_t val)
{
	pci_config_put8(sc->pci_regh, reg, val);
}

static inline void
t4_os_pci_read_cfg2(struct adapter *sc, int reg, uint16_t *val)
{
	*val = pci_config_get16(sc->pci_regh, reg);
}

static inline void
t4_os_pci_write_cfg2(struct adapter *sc, int reg, uint16_t val)
{
	pci_config_put16(sc->pci_regh, reg, val);
}

static inline void
t4_os_pci_read_cfg4(struct adapter *sc, int reg, uint32_t *val)
{
	*val = pci_config_get32(sc->pci_regh, reg);
}

static inline void
t4_os_pci_write_cfg4(struct adapter *sc, int reg, uint32_t val)
{
	pci_config_put32(sc->pci_regh, reg, val);
}

static inline uint32_t
t4_read_reg32(struct adapter *sc, uint32_t reg)
{
	return (ddi_get32(sc->regh, (uint32_t *)(sc->regp + reg)));
}

static inline uint64_t
t4_read_reg64(struct adapter *sc, uint32_t reg)
{
	return (ddi_get64(sc->regh, (uint64_t *)(sc->regp + reg)));
}

static inline void
t4_write_reg64(struct adapter *sc, uint32_t reg, uint64_t val)
{
	ddi_put64(sc->regh, (uint64_t *)(sc->regp + reg), val);
}

static inline struct port_info *
adap2pinfo(struct adapter *sc, int idx)
{
	return (sc->port[idx]);
}

static inline void
t4_os_set_hw_addr(struct adapter *sc, int idx, uint8_t hw_addr[])
{
	bcopy(hw_addr, sc->port[idx]->hw_addr, ETHERADDRL);
}

static inline bool
is_10G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_10G) != 0);
}

static inline struct sge_rxq *
iq_to_rxq(struct sge_iq *iq)
{
	return (__containerof(iq, struct sge_rxq, iq));
}

static inline bool
is_25G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_25G) != 0);
}

static inline bool
is_40G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_40G) != 0);
}

static inline bool
is_50G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_50G) != 0);
}

static inline bool
is_100G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.pcaps & FW_PORT_CAP32_SPEED_100G) != 0);
}

static inline bool
is_10XG_port(const struct port_info *pi)
{
	return (is_10G_port(pi) || is_40G_port(pi) ||
	    is_25G_port(pi) || is_50G_port(pi) ||
	    is_100G_port(pi));
}

/*
 * t4_os_pci_read_seeprom - read four bytes of SEEPROM/VPD contents
 * @adapter: the adapter
 * @addr: SEEPROM/VPD Address to read
 * @valp: where to store the value read
 *
 * Read a 32-bit value from the given address in the SEEPROM/VPD.  The address
 * must be four-byte aligned.  Returns 0 on success, a negative erro number
 * on failure.
 */
static inline int t4_os_pci_read_seeprom(adapter_t *adapter, int addr,
    u32 *valp)
{
	int t4_seeprom_read(struct adapter *adapter, u32 addr, u32 *data);
	int ret;

	ret = t4_seeprom_read(adapter, addr, valp);

	return (ret >= 0 ? 0 : ret);
}

/*
 * t4_os_pci_write_seeprom - write four bytes of SEEPROM/VPD contents
 * @adapter: the adapter
 * @addr: SEEPROM/VPD Address to write
 * @val: the value write
 *
 * Write a 32-bit value to the given address in the SEEPROM/VPD.  The address
 * must be four-byte aligned.  Returns 0 on success, a negative erro number
 * on failure.
 */
static inline int t4_os_pci_write_seeprom(adapter_t *adapter, int addr, u32 val)
{
	int t4_seeprom_write(struct adapter *adapter, u32 addr, u32 data);
	int ret;

	ret = t4_seeprom_write(adapter, addr, val);

	return (ret >= 0 ? 0 : ret);
}

static inline int t4_os_pci_set_vpd_size(struct adapter *adapter, size_t len)
{
	return (0);
}

static inline unsigned int t4_use_ldst(struct adapter *adap)
{
	return (adap->flags & FW_OK);
}

static inline void t4_db_full(struct adapter *adap) {}
static inline void t4_db_dropped(struct adapter *adap) {}

/* t4_nexus.c */
int t4_os_find_pci_capability(struct adapter *sc, int cap);
void t4_os_portmod_changed(struct adapter *sc, int idx);
int adapter_full_init(struct adapter *sc);
int adapter_full_uninit(struct adapter *sc);
int port_full_init(struct port_info *pi);
int port_full_uninit(struct port_info *pi);
void enable_port_queues(struct port_info *pi);
void disable_port_queues(struct port_info *pi);
void t4_iterate(void (*func)(int, void *), void *arg);

/* t4_debug.c */
void t4_debug_init(void);
void t4_debug_fini(void);

/* t4_sge.c */
void t4_sge_init(struct adapter *sc);
int t4_setup_adapter_queues(struct adapter *sc);
int t4_teardown_adapter_queues(struct adapter *sc);
int t4_setup_port_queues(struct port_info *pi);
int t4_teardown_port_queues(struct port_info *pi);
uint_t t4_intr_all(caddr_t arg1, caddr_t arg2);
uint_t t4_intr(caddr_t arg1, caddr_t arg2);
uint_t t4_intr_err(caddr_t arg1, caddr_t arg2);
int t4_mgmt_tx(struct adapter *sc, mblk_t *m);
void memwin_info(struct adapter *, int, uint32_t *, uint32_t *);
uint32_t position_memwin(struct adapter *, int, uint32_t);

mblk_t *t4_eth_tx(void *, mblk_t *);
mblk_t *t4_mc_tx(void *arg, mblk_t *m);
mblk_t *t4_ring_rx(struct sge_rxq *rxq, int poll_bytes);
int t4_alloc_tx_maps(struct adapter *sc, struct tx_maps *txmaps,  int count,
    int flags);

/* t4_mac.c */
void t4_mc_init(struct port_info *pi);
void t4_mc_cb_init(struct port_info *);
void t4_os_link_changed(struct adapter *sc, int idx, int link_stat);
void t4_mac_rx(struct port_info *pi, struct sge_rxq *rxq, mblk_t *m);
void t4_mac_tx_update(struct port_info *pi, struct sge_txq *txq);
int t4_addmac(void *arg, const uint8_t *ucaddr);

/* t4_ioctl.c */
int t4_ioctl(struct adapter *sc, int cmd, void *data, int mode);

int begin_synchronized_op(struct port_info *pi, int hold, int waitok);
void end_synchronized_op(struct port_info *pi, int held);

#define	setbit(a, i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a, i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a, i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))

#endif /* __CXGBE_ADAPTER_H */
