/* $OpenBSD: softraid.c,v 1.266 2012/01/17 12:56:38 jsing Exp $ */
/*
 * Copyright (c) 2007, 2008, 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Chris Kuethe <ckuethe@openbsd.org>
 * Copyright (c) 2009 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/sensors.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/workq.h>
#include <sys/kthread.h>
#include <sys/dkio.h>

#ifdef AOE
#include <sys/mbuf.h>
#include <net/if_aoe.h>
#endif /* AOE */

#include <crypto/cryptodev.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

/* #define SR_FANCY_STATS */

#ifdef SR_DEBUG
#define SR_FANCY_STATS
uint32_t	sr_debug = 0
		    /* | SR_D_CMD */
		    /* | SR_D_MISC */
		    /* | SR_D_INTR */
		    /* | SR_D_IOCTL */
		    /* | SR_D_CCB */
		    /* | SR_D_WU */
		    /* | SR_D_META */
		    /* | SR_D_DIS */
		    /* | SR_D_STATE */
		;
#endif

struct sr_softc *softraid0;

int		sr_match(struct device *, void *, void *);
void		sr_attach(struct device *, struct device *, void *);
int		sr_detach(struct device *, int);
void		sr_map_root(void);

struct cfattach softraid_ca = {
	sizeof(struct sr_softc), sr_match, sr_attach, sr_detach,
};

struct cfdriver softraid_cd = {
	NULL, "softraid", DV_DULL
};

/* scsi & discipline */
void			sr_scsi_cmd(struct scsi_xfer *);
void			sr_minphys(struct buf *, struct scsi_link *);
int			sr_scsi_probe(struct scsi_link *);
void			sr_copy_internal_data(struct scsi_xfer *,
			    void *, size_t);
int			sr_scsi_ioctl(struct scsi_link *, u_long,
			    caddr_t, int);
int			sr_ioctl(struct device *, u_long, caddr_t);
int			sr_ioctl_inq(struct sr_softc *, struct bioc_inq *);
int			sr_ioctl_vol(struct sr_softc *, struct bioc_vol *);
int			sr_ioctl_disk(struct sr_softc *, struct bioc_disk *);
int			sr_ioctl_setstate(struct sr_softc *,
			    struct bioc_setstate *);
int			sr_ioctl_createraid(struct sr_softc *,
			    struct bioc_createraid *, int);
int			sr_ioctl_deleteraid(struct sr_softc *,
			    struct bioc_deleteraid *);
int			sr_ioctl_discipline(struct sr_softc *,
			    struct bioc_discipline *);
int			sr_ioctl_installboot(struct sr_softc *,
			    struct bioc_installboot *);
void			sr_chunks_unwind(struct sr_softc *,
			    struct sr_chunk_head *);
void			sr_discipline_free(struct sr_discipline *);
void			sr_discipline_shutdown(struct sr_discipline *, int);
int			sr_discipline_init(struct sr_discipline *, int);
void			sr_set_chunk_state(struct sr_discipline *, int, int);
void			sr_set_vol_state(struct sr_discipline *);

/* utility functions */
void			sr_shutdown(struct sr_softc *);
void			sr_shutdownhook(void *);
void			sr_uuid_get(struct sr_uuid *);
void			sr_uuid_print(struct sr_uuid *, int);
void			sr_checksum_print(u_int8_t *);
int			sr_boot_assembly(struct sr_softc *);
int			sr_already_assembled(struct sr_discipline *);
int			sr_hotspare(struct sr_softc *, dev_t);
void			sr_hotspare_rebuild(struct sr_discipline *);
int			sr_rebuild_init(struct sr_discipline *, dev_t, int);
void			sr_rebuild(void *);
void			sr_rebuild_thread(void *);
void			sr_roam_chunks(struct sr_discipline *);
int			sr_chunk_in_use(struct sr_softc *, dev_t);
void			sr_startwu_callback(void *, void *);
int			sr_rw(struct sr_softc *, dev_t, char *, size_t,
			    daddr64_t, long);

/* don't include these on RAMDISK */
#ifndef SMALL_KERNEL
void			sr_sensors_refresh(void *);
int			sr_sensors_create(struct sr_discipline *);
void			sr_sensors_delete(struct sr_discipline *);
#endif

/* metadata */
int			sr_meta_probe(struct sr_discipline *, dev_t *, int);
int			sr_meta_attach(struct sr_discipline *, int, int);
int			sr_meta_rw(struct sr_discipline *, dev_t, void *,
			    size_t, daddr64_t, long);
int			sr_meta_clear(struct sr_discipline *);
void			sr_meta_init(struct sr_discipline *, int, int);
void			sr_meta_init_complete(struct sr_discipline *);
void			sr_meta_opt_handler(struct sr_discipline *,
			    struct sr_meta_opt_hdr *);

/* hotplug magic */
void			sr_disk_attach(struct disk *, int);

struct sr_hotplug_list {
	void			(*sh_hotplug)(struct sr_discipline *,
				    struct disk *, int);
	struct sr_discipline	*sh_sd;

	SLIST_ENTRY(sr_hotplug_list) shl_link;
};
SLIST_HEAD(sr_hotplug_list_head, sr_hotplug_list);

struct			sr_hotplug_list_head	sr_hotplug_callbacks;
extern void		(*softraid_disk_attach)(struct disk *, int);

/* scsi glue */
struct scsi_adapter sr_switch = {
	sr_scsi_cmd, sr_minphys, sr_scsi_probe, NULL, sr_scsi_ioctl
};

/* native metadata format */
int			sr_meta_native_bootprobe(struct sr_softc *, dev_t,
			    struct sr_boot_chunk_head *);
#define SR_META_NOTCLAIMED	(0)
#define SR_META_CLAIMED		(1)
int			sr_meta_native_probe(struct sr_softc *,
			   struct sr_chunk *);
int			sr_meta_native_attach(struct sr_discipline *, int);
int			sr_meta_native_write(struct sr_discipline *, dev_t,
			    struct sr_metadata *,void *);

#ifdef SR_DEBUG
void			sr_meta_print(struct sr_metadata *);
#else
#define			sr_meta_print(m)
#endif

/* the metadata driver should remain stateless */
struct sr_meta_driver {
	daddr64_t		smd_offset;	/* metadata location */
	u_int32_t		smd_size;	/* size of metadata */

	int			(*smd_probe)(struct sr_softc *,
				   struct sr_chunk *);
	int			(*smd_attach)(struct sr_discipline *, int);
	int			(*smd_detach)(struct sr_discipline *);
	int			(*smd_read)(struct sr_discipline *, dev_t,
				    struct sr_metadata *, void *);
	int			(*smd_write)(struct sr_discipline *, dev_t,
				    struct sr_metadata *, void *);
	int			(*smd_validate)(struct sr_discipline *,
				    struct sr_metadata *, void *);
} smd[] = {
	{ SR_META_OFFSET, SR_META_SIZE * 512,
	  sr_meta_native_probe, sr_meta_native_attach, NULL,
	  sr_meta_native_read, sr_meta_native_write, NULL },
	{ 0, 0, NULL, NULL, NULL, NULL }
};

int
sr_meta_attach(struct sr_discipline *sd, int chunk_no, int force)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*ch_entry, *chunk1, *chunk2;
	int			rv = 1, i = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_attach(%d)\n", DEVNAME(sc));

	/* in memory copy of metadata */
	sd->sd_meta = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (!sd->sd_meta) {
		printf("%s: could not allocate memory for metadata\n",
		    DEVNAME(sc));
		goto bad;
	}

	if (sd->sd_meta_type != SR_META_F_NATIVE) {
		/* in memory copy of foreign metadata */
		sd->sd_meta_foreign = malloc(smd[sd->sd_meta_type].smd_size,
		    M_DEVBUF, M_ZERO | M_NOWAIT);
		if (!sd->sd_meta_foreign) {
			/* unwind frees sd_meta */
			printf("%s: could not allocate memory for foreign "
			    "metadata\n", DEVNAME(sc));
			goto bad;
		}
	}

	/* we have a valid list now create an array index */
	cl = &sd->sd_vol.sv_chunk_list;
	sd->sd_vol.sv_chunks = malloc(sizeof(struct sr_chunk *) * chunk_no,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* fill out chunk array */
	i = 0;
	SLIST_FOREACH(ch_entry, cl, src_link)
		sd->sd_vol.sv_chunks[i++] = ch_entry;

	/* attach metadata */
	if (smd[sd->sd_meta_type].smd_attach(sd, force))
		goto bad;

	/* Force chunks into correct order now that metadata is attached. */
	SLIST_FOREACH(ch_entry, cl, src_link)
		SLIST_REMOVE(cl, ch_entry, sr_chunk, src_link);
	for (i = 0; i < chunk_no; i++) {
		ch_entry = sd->sd_vol.sv_chunks[i];
		chunk2 = NULL;
		SLIST_FOREACH(chunk1, cl, src_link) {
			if (chunk1->src_meta.scmi.scm_chunk_id >
			    ch_entry->src_meta.scmi.scm_chunk_id)
				break;
			chunk2 = chunk1;
		}
		if (chunk2 == NULL)
			SLIST_INSERT_HEAD(cl, ch_entry, src_link);
		else
			SLIST_INSERT_AFTER(chunk2, ch_entry, src_link);
	}
	i = 0;
	SLIST_FOREACH(ch_entry, cl, src_link)
		sd->sd_vol.sv_chunks[i++] = ch_entry;

	rv = 0;
bad:
	return (rv);
}

int
sr_meta_probe(struct sr_discipline *sd, dev_t *dt, int no_chunk)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct vnode		*vn;
	struct sr_chunk		*ch_entry, *ch_prev = NULL;
	struct sr_chunk_head	*cl;
	char			devname[32];
	int			i, d, type, found, prevf, error;
	dev_t			dev;

	DNPRINTF(SR_D_META, "%s: sr_meta_probe(%d)\n", DEVNAME(sc), no_chunk);

	if (no_chunk == 0)
		goto unwind;

	cl = &sd->sd_vol.sv_chunk_list;

	for (d = 0, prevf = SR_META_F_INVALID; d < no_chunk; d++) {
		ch_entry = malloc(sizeof(struct sr_chunk), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		/* keep disks in user supplied order */
		if (ch_prev)
			SLIST_INSERT_AFTER(ch_prev, ch_entry, src_link);
		else
			SLIST_INSERT_HEAD(cl, ch_entry, src_link);
		ch_prev = ch_entry;
		dev = dt[d];
		ch_entry->src_dev_mm = dev;

		if (dev == NODEV) {
			ch_entry->src_meta.scm_status = BIOC_SDOFFLINE;
			continue;
		} else {
			sr_meta_getdevname(sc, dev, devname, sizeof(devname));
			if (bdevvp(dev, &vn)) {
				printf("%s: sr_meta_probe: can't allocate "
				    "vnode\n", DEVNAME(sc));
				goto unwind;
			}

			/*
			 * XXX leaving dev open for now; move this to attach
			 * and figure out the open/close dance for unwind.
			 */
			error = VOP_OPEN(vn, FREAD | FWRITE, NOCRED, curproc);
			if (error) {
				DNPRINTF(SR_D_META,"%s: sr_meta_probe can't "
				    "open %s\n", DEVNAME(sc), devname);
				vput(vn);
				goto unwind;
			}

			strlcpy(ch_entry->src_devname, devname,
			    sizeof(ch_entry->src_devname));
			ch_entry->src_vn = vn;
		}

		/* determine if this is a device we understand */
		for (i = 0, found = SR_META_F_INVALID; smd[i].smd_probe; i++) {
			type = smd[i].smd_probe(sc, ch_entry);
			if (type == SR_META_F_INVALID)
				continue;
			else {
				found = type;
				break;
			}
		}

		if (found == SR_META_F_INVALID)
			goto unwind;
		if (prevf == SR_META_F_INVALID)
			prevf = found;
		if (prevf != found) {
			DNPRINTF(SR_D_META, "%s: prevf != found\n",
			    DEVNAME(sc));
			goto unwind;
		}
	}

	return (prevf);
unwind:
	return (SR_META_F_INVALID);
}

void
sr_meta_getdevname(struct sr_softc *sc, dev_t dev, char *buf, int size)
{
	int			maj, unit, part;
	char			*name;

	DNPRINTF(SR_D_META, "%s: sr_meta_getdevname(%p, %d)\n",
	    DEVNAME(sc), buf, size);

	if (!buf)
		return;

	maj = major(dev);
	part = DISKPART(dev);
	unit = DISKUNIT(dev);

	name = findblkname(maj);
	if (name == NULL)
		return;

	snprintf(buf, size, "%s%d%c", name, unit, part + 'a');
}

int
sr_rw(struct sr_softc *sc, dev_t dev, char *buf, size_t size, daddr64_t offset,
    long flags)
{
	struct vnode		*vp;
	struct buf		b;
	size_t			bufsize, dma_bufsize;
	int			rv = 1;
	char			*dma_buf;

	DNPRINTF(SR_D_MISC, "%s: sr_rw(0x%x, %p, %d, %llu 0x%x)\n",
	    DEVNAME(sc), dev, buf, size, offset, flags);

	dma_bufsize = (size > MAXPHYS) ? MAXPHYS : size;
	dma_buf = dma_alloc(dma_bufsize, PR_WAITOK);

	if (bdevvp(dev, &vp)) {
		printf("%s: sr_rw: failed to allocate vnode\n", DEVNAME(sc));
		goto done;
	}

	while (size > 0) {
		DNPRINTF(SR_D_MISC, "%s: dma_buf %p, size %d, offset %llu)\n",
		    DEVNAME(sc), dma_buf, size, offset);

		bufsize = (size > MAXPHYS) ? MAXPHYS : size;
		if (flags == B_WRITE)
			bcopy(buf, dma_buf, bufsize);

		bzero(&b, sizeof(b));
		b.b_flags = flags | B_PHYS;
		b.b_proc = curproc;
		b.b_dev = dev;
		b.b_iodone = NULL;
		b.b_error = 0;
		b.b_blkno = offset;
		b.b_data = dma_buf;
		b.b_bcount = bufsize;
		b.b_bufsize = bufsize;
		b.b_resid = bufsize;
		b.b_vp = vp;

		if ((b.b_flags & B_READ) == 0)
			vp->v_numoutput++;

		LIST_INIT(&b.b_dep);
		VOP_STRATEGY(&b);
		biowait(&b);

		if (b.b_flags & B_ERROR) {
			printf("%s: I/O error %d on dev 0x%x at block %llu\n",
			    DEVNAME(sc), b.b_error, dev, b.b_blkno);
			goto done;
		}

		if (flags == B_READ)
			bcopy(dma_buf, buf, bufsize);

		size -= bufsize;
		buf += bufsize;
		offset += howmany(bufsize, DEV_BSIZE);
	}

	rv = 0;

done:
	if (vp)
		vput(vp);

	dma_free(dma_buf, dma_bufsize);

	return (rv);
}

int
sr_meta_rw(struct sr_discipline *sd, dev_t dev, void *md, size_t size,
    daddr64_t offset, long flags)
{
	int			rv = 1;

	DNPRINTF(SR_D_META, "%s: sr_meta_rw(0x%x, %p, %d, %llu 0x%x)\n",
	    DEVNAME(sd->sd_sc), dev, md, size, offset, flags);

	if (md == NULL) {
		printf("%s: sr_meta_rw: invalid metadata pointer\n",
		    DEVNAME(sd->sd_sc));
		goto done;
	}

	rv = sr_rw(sd->sd_sc, dev, md, size, offset, flags);

done:
	return (rv);
}

int
sr_meta_clear(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_chunk		*ch_entry;
	void			*m;
	int			rv = 1;

	DNPRINTF(SR_D_META, "%s: sr_meta_clear\n", DEVNAME(sc));

	if (sd->sd_meta_type != SR_META_F_NATIVE) {
		printf("%s: sr_meta_clear can not clear foreign metadata\n",
		    DEVNAME(sc));
		goto done;
	}

	m = malloc(SR_META_SIZE * 512, M_DEVBUF, M_WAITOK | M_ZERO);
	SLIST_FOREACH(ch_entry, cl, src_link) {
		if (sr_meta_native_write(sd, ch_entry->src_dev_mm, m, NULL)) {
			/* XXX mark disk offline */
			DNPRINTF(SR_D_META, "%s: sr_meta_clear failed to "
			    "clear %s\n", ch_entry->src_devname);
			rv++;
			continue;
		}
		bzero(&ch_entry->src_meta, sizeof(ch_entry->src_meta));
	}

	bzero(sd->sd_meta, SR_META_SIZE * 512);

	free(m, M_DEVBUF);
	rv = 0;
done:
	return (rv);
}

void
sr_meta_init(struct sr_discipline *sd, int level, int no_chunk)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_metadata	*sm = sd->sd_meta;
	struct sr_chunk_head	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_meta_chunk	*scm;
	struct sr_chunk		*chunk;
	int			cid = 0;
	u_int64_t		max_chunk_sz = 0, min_chunk_sz = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_init\n", DEVNAME(sc));

	if (!sm)
		return;

	/* Initialise volume metadata. */
	sm->ssdi.ssd_magic = SR_MAGIC;
	sm->ssdi.ssd_version = SR_META_VERSION;
	sm->ssdi.ssd_vol_flags = sd->sd_meta_flags;
	sm->ssdi.ssd_volid = 0;
	sm->ssdi.ssd_chunk_no = no_chunk;
	sm->ssdi.ssd_level = level;

	sm->ssd_data_offset = SR_DATA_OFFSET;
	sm->ssd_ondisk = 0;

	sr_uuid_get(&sm->ssdi.ssd_uuid);

	/* Initialise chunk metadata and get min/max chunk sizes. */
	SLIST_FOREACH(chunk, cl, src_link) {
		scm = &chunk->src_meta;
		scm->scmi.scm_size = chunk->src_size;
		scm->scmi.scm_chunk_id = cid++;
		scm->scm_status = BIOC_SDONLINE;
		scm->scmi.scm_volid = 0;
		strlcpy(scm->scmi.scm_devname, chunk->src_devname,
		    sizeof(scm->scmi.scm_devname));
		bcopy(&sm->ssdi.ssd_uuid, &scm->scmi.scm_uuid,
		    sizeof(scm->scmi.scm_uuid));
		sr_checksum(sc, scm, &scm->scm_checksum,
		    sizeof(scm->scm_checksum));

		if (min_chunk_sz == 0)
			min_chunk_sz = scm->scmi.scm_size;
		min_chunk_sz = MIN(min_chunk_sz, scm->scmi.scm_size);
		max_chunk_sz = MAX(max_chunk_sz, scm->scmi.scm_size);
	}

	/* Equalize chunk sizes. */
	SLIST_FOREACH(chunk, cl, src_link)
		chunk->src_meta.scmi.scm_coerced_size = min_chunk_sz;

	sd->sd_vol.sv_chunk_minsz = min_chunk_sz;
	sd->sd_vol.sv_chunk_maxsz = max_chunk_sz;
}

void
sr_meta_init_complete(struct sr_discipline *sd)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	struct sr_metadata	*sm = sd->sd_meta;

	DNPRINTF(SR_D_META, "%s: sr_meta_complete\n", DEVNAME(sc));

	/* Complete initialisation of volume metadata. */
	strlcpy(sm->ssdi.ssd_vendor, "OPENBSD", sizeof(sm->ssdi.ssd_vendor));
	snprintf(sm->ssdi.ssd_product, sizeof(sm->ssdi.ssd_product),
	    "SR %s", sd->sd_name);
	snprintf(sm->ssdi.ssd_revision, sizeof(sm->ssdi.ssd_revision),
	    "%03d", sm->ssdi.ssd_version);
}

void
sr_meta_opt_handler(struct sr_discipline *sd, struct sr_meta_opt_hdr *om)
{
	if (om->som_type != SR_OPT_BOOT)
		panic("unknown optional metadata type");
}

void
sr_meta_save_callback(void *arg1, void *arg2)
{
	struct sr_discipline	*sd = arg1;
	int			s;

	s = splbio();

	if (sr_meta_save(arg1, SR_META_DIRTY))
		printf("%s: save metadata failed\n",
		    DEVNAME(sd->sd_sc));

	sd->sd_must_flush = 0;
	splx(s);
}

int
sr_meta_save(struct sr_discipline *sd, u_int32_t flags)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_metadata	*sm = sd->sd_meta, *m;
	struct sr_meta_driver	*s;
	struct sr_chunk		*src;
	struct sr_meta_chunk	*cm;
	struct sr_workunit	wu;
	struct sr_meta_opt_hdr	*omh;
	struct sr_meta_opt_item *omi;
	int			i;

	DNPRINTF(SR_D_META, "%s: sr_meta_save %s\n",
	    DEVNAME(sc), sd->sd_meta->ssd_devname);

	if (!sm) {
		printf("%s: no in memory copy of metadata\n", DEVNAME(sc));
		goto bad;
	}

	/* meta scratchpad */
	s = &smd[sd->sd_meta_type];
	m = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (!m) {
		printf("%s: could not allocate metadata scratch area\n",
		    DEVNAME(sc));
		goto bad;
	}

	/* from here on out metadata is updated */
restart:
	sm->ssd_ondisk++;
	sm->ssd_meta_flags = flags;
	bcopy(sm, m, sizeof(*m));

	/* Chunk metadata. */
	cm = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < sm->ssdi.ssd_chunk_no; i++) {
		src = sd->sd_vol.sv_chunks[i];
		bcopy(&src->src_meta, cm, sizeof(*cm));
		cm++;
	}

	/* Optional metadata. */
	omh = (struct sr_meta_opt_hdr *)(cm);
	SLIST_FOREACH(omi, &sd->sd_meta_opt, omi_link) {
		DNPRINTF(SR_D_META, "%s: saving optional metadata type %u with "
		    "length %u\n", DEVNAME(sc), omi->omi_som->som_type,
		    omi->omi_som->som_length);
		bzero(&omi->omi_som->som_checksum, MD5_DIGEST_LENGTH);
		sr_checksum(sc, omi->omi_som, &omi->omi_som->som_checksum,
		    omi->omi_som->som_length);
		bcopy(omi->omi_som, omh, omi->omi_som->som_length);
		omh = (struct sr_meta_opt_hdr *)((u_int8_t *)omh +
		    omi->omi_som->som_length);
	}

	for (i = 0; i < sm->ssdi.ssd_chunk_no; i++) {
		src = sd->sd_vol.sv_chunks[i];

		/* skip disks that are offline */
		if (src->src_meta.scm_status == BIOC_SDOFFLINE)
			continue;

		/* calculate metadata checksum for correct chunk */
		m->ssdi.ssd_chunk_id = i;
		sr_checksum(sc, m, &m->ssd_checksum,
		    sizeof(struct sr_meta_invariant));

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: sr_meta_save %s: volid: %d "
		    "chunkid: %d checksum: ",
		    DEVNAME(sc), src->src_meta.scmi.scm_devname,
		    m->ssdi.ssd_volid, m->ssdi.ssd_chunk_id);

		if (sr_debug & SR_D_META)
			sr_checksum_print((u_int8_t *)&m->ssd_checksum);
		DNPRINTF(SR_D_META, "\n");
		sr_meta_print(m);
#endif

		/* translate and write to disk */
		if (s->smd_write(sd, src->src_dev_mm, m, NULL /* XXX */)) {
			printf("%s: could not write metadata to %s\n",
			    DEVNAME(sc), src->src_devname);
			/* restart the meta write */
			src->src_meta.scm_status = BIOC_SDOFFLINE;
			/* XXX recalculate volume status */
			goto restart;
		}
	}

	/* not all disciplines have sync */
	if (sd->sd_scsi_sync) {
		bzero(&wu, sizeof(wu));
		wu.swu_fake = 1;
		wu.swu_dis = sd;
		sd->sd_scsi_sync(&wu);
	}
	free(m, M_DEVBUF);
	return (0);
bad:
	return (1);
}

int
sr_meta_read(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head 	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_metadata	*sm;
	struct sr_chunk		*ch_entry;
	struct sr_meta_chunk	*cp;
	struct sr_meta_driver	*s;
	void			*fm = NULL;
	int			no_disk = 0, got_meta = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_read\n", DEVNAME(sc));

	sm = malloc(SR_META_SIZE * 512, M_DEVBUF, M_WAITOK | M_ZERO);
	s = &smd[sd->sd_meta_type];
	if (sd->sd_meta_type != SR_META_F_NATIVE)
		fm = malloc(s->smd_size, M_DEVBUF, M_WAITOK | M_ZERO);

	cp = (struct sr_meta_chunk *)(sm + 1);
	SLIST_FOREACH(ch_entry, cl, src_link) {
		/* skip disks that are offline */
		if (ch_entry->src_meta.scm_status == BIOC_SDOFFLINE) {
			DNPRINTF(SR_D_META,
			    "%s: %s chunk marked offline, spoofing status\n",
			    DEVNAME(sc), ch_entry->src_devname);
			cp++; /* adjust chunk pointer to match failure */
			continue;
		} else if (s->smd_read(sd, ch_entry->src_dev_mm, sm, fm)) {
			/* read and translate */
			/* XXX mark chunk offline, elsewhere!! */
			ch_entry->src_meta.scm_status = BIOC_SDOFFLINE;
			cp++; /* adjust chunk pointer to match failure */
			DNPRINTF(SR_D_META, "%s: sr_meta_read failed\n",
			    DEVNAME(sc));
			continue;
		}

		if (sm->ssdi.ssd_magic != SR_MAGIC) {
			DNPRINTF(SR_D_META, "%s: sr_meta_read !SR_MAGIC\n",
			    DEVNAME(sc));
			continue;
		}

		/* validate metadata */
		if (sr_meta_validate(sd, ch_entry->src_dev_mm, sm, fm)) {
			DNPRINTF(SR_D_META, "%s: invalid metadata\n",
			    DEVNAME(sc));
			no_disk = -1;
			goto done;
		}

		/* assume first chunk contains metadata */
		if (got_meta == 0) {
			sr_meta_opt_load(sc, sm, &sd->sd_meta_opt);
			bcopy(sm, sd->sd_meta, sizeof(*sd->sd_meta));
			got_meta = 1;
		}

		bcopy(cp, &ch_entry->src_meta, sizeof(ch_entry->src_meta));

		no_disk++;
		cp++;
	}

	free(sm, M_DEVBUF);
	if (fm)
		free(fm, M_DEVBUF);

done:
	DNPRINTF(SR_D_META, "%s: sr_meta_read found %d parts\n", DEVNAME(sc),
	    no_disk);
	return (no_disk);
}

void
sr_meta_opt_load(struct sr_softc *sc, struct sr_metadata *sm,
    struct sr_meta_opt_head *som)
{
	struct sr_meta_opt_hdr	*omh;
	struct sr_meta_opt_item *omi;
	u_int8_t		checksum[MD5_DIGEST_LENGTH];
	int			i;

	/* Process optional metadata. */
	omh = (struct sr_meta_opt_hdr *)((u_int8_t *)(sm + 1) +
	    sizeof(struct sr_meta_chunk) * sm->ssdi.ssd_chunk_no);
	for (i = 0; i < sm->ssdi.ssd_opt_no; i++) {

		omi = malloc(sizeof(struct sr_meta_opt_item), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		SLIST_INSERT_HEAD(som, omi, omi_link);

		if (omh->som_length == 0) {

			/* Load old fixed length optional metadata. */
			DNPRINTF(SR_D_META, "%s: old optional metadata of type "
			    "%u\n", DEVNAME(sc), omh->som_type);

			/* Validate checksum. */
			sr_checksum(sc, (void *)omh, &checksum,
			    SR_OLD_META_OPT_SIZE - MD5_DIGEST_LENGTH);
			if (bcmp(&checksum, (void *)omh + SR_OLD_META_OPT_MD5,
			    sizeof(checksum)))
				panic("%s: invalid optional metadata "
				    "checksum", DEVNAME(sc));

			/* Determine correct length. */
			switch (omh->som_type) {
			case SR_OPT_CRYPTO:
				omh->som_length = sizeof(struct sr_meta_crypto);
				break;
			case SR_OPT_BOOT:
				omh->som_length = sizeof(struct sr_meta_boot);
				break;
			case SR_OPT_KEYDISK:
				omh->som_length =
				    sizeof(struct sr_meta_keydisk);
				break;
			default:
				panic("unknown old optional metadata "
				    "type %u\n", omh->som_type);
			}

			omi->omi_som = malloc(omh->som_length, M_DEVBUF,
			    M_WAITOK | M_ZERO);
			bcopy((u_int8_t *)omh + SR_OLD_META_OPT_OFFSET,
			    (u_int8_t *)omi->omi_som + sizeof(*omi->omi_som),
			    omh->som_length - sizeof(*omi->omi_som));
			omi->omi_som->som_type = omh->som_type;
			omi->omi_som->som_length = omh->som_length;

			omh = (struct sr_meta_opt_hdr *)((void *)omh +
			    SR_OLD_META_OPT_SIZE);
		} else {

			/* Load variable length optional metadata. */
			DNPRINTF(SR_D_META, "%s: optional metadata of type %u, "
			    "length %u\n", DEVNAME(sc), omh->som_type,
			    omh->som_length);
			omi->omi_som = malloc(omh->som_length, M_DEVBUF,
			    M_WAITOK | M_ZERO);
			bcopy(omh, omi->omi_som, omh->som_length);

			/* Validate checksum. */
			bcopy(&omi->omi_som->som_checksum, &checksum,
			    MD5_DIGEST_LENGTH);
			bzero(&omi->omi_som->som_checksum, MD5_DIGEST_LENGTH);
			sr_checksum(sc, omi->omi_som,
			    &omi->omi_som->som_checksum, omh->som_length);
			if (bcmp(&checksum, &omi->omi_som->som_checksum,
			    sizeof(checksum)))
				panic("%s: invalid optional metadata checksum",
				    DEVNAME(sc));

			omh = (struct sr_meta_opt_hdr *)((void *)omh +
			    omh->som_length);
		}
	}
}

int
sr_meta_validate(struct sr_discipline *sd, dev_t dev, struct sr_metadata *sm,
    void *fm)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_meta_driver	*s;
#ifdef SR_DEBUG
	struct sr_meta_chunk	*mc;
#endif
	u_int8_t		checksum[MD5_DIGEST_LENGTH];
	char			devname[32];
	int			rv = 1;

	DNPRINTF(SR_D_META, "%s: sr_meta_validate(%p)\n", DEVNAME(sc), sm);

	sr_meta_getdevname(sc, dev, devname, sizeof(devname));

	s = &smd[sd->sd_meta_type];
	if (sd->sd_meta_type != SR_META_F_NATIVE)
		if (s->smd_validate(sd, sm, fm)) {
			printf("%s: invalid foreign metadata\n", DEVNAME(sc));
			goto done;
		}

	/*
	 * at this point all foreign metadata has been translated to the native
	 * format and will be treated just like the native format
	 */

	if (sm->ssdi.ssd_magic != SR_MAGIC) {
		printf("%s: not valid softraid metadata\n", DEVNAME(sc));
		goto done;
	}

	/* Verify metadata checksum. */
	sr_checksum(sc, sm, &checksum, sizeof(struct sr_meta_invariant));
	if (bcmp(&checksum, &sm->ssd_checksum, sizeof(checksum))) {
		printf("%s: invalid metadata checksum\n", DEVNAME(sc));
		goto done;
	}

	/* Handle changes between versions. */
	if (sm->ssdi.ssd_version == 3) {

		/*
		 * Version 3 - update metadata version and fix up data offset
		 * value since this did not exist in version 3.
		 */
		if (sm->ssd_data_offset == 0)
			sm->ssd_data_offset = SR_META_V3_DATA_OFFSET;

	} else if (sm->ssdi.ssd_version == 4) {

		/*
		 * Version 4 - original metadata format did not store
		 * data offset so fix this up if necessary.
		 */
		if (sm->ssd_data_offset == 0)
			sm->ssd_data_offset = SR_DATA_OFFSET;

	} else if (sm->ssdi.ssd_version == SR_META_VERSION) {

		/*
		 * Version 5 - variable length optional metadata. Migration
		 * from earlier fixed length optional metadata is handled
		 * in sr_meta_read().
		 */

	} else {

		printf("%s: %s can not read metadata version %u, expected %u\n",
		    DEVNAME(sc), devname, sm->ssdi.ssd_version,
		    SR_META_VERSION);
		goto done;

	}

	/* Update version number and revision string. */
	sm->ssdi.ssd_version = SR_META_VERSION;
	snprintf(sm->ssdi.ssd_revision, sizeof(sm->ssdi.ssd_revision),
	    "%03d", SR_META_VERSION);

#ifdef SR_DEBUG
	/* warn if disk changed order */
	mc = (struct sr_meta_chunk *)(sm + 1);
	if (strncmp(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname, devname,
	    sizeof(mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname)))
		DNPRINTF(SR_D_META, "%s: roaming device %s -> %s\n",
		    DEVNAME(sc), mc[sm->ssdi.ssd_chunk_id].scmi.scm_devname,
		    devname);
#endif

	/* we have meta data on disk */
	DNPRINTF(SR_D_META, "%s: sr_meta_validate valid metadata %s\n",
	    DEVNAME(sc), devname);

	rv = 0;
done:
	return (rv);
}

int
sr_meta_native_bootprobe(struct sr_softc *sc, dev_t devno,
    struct sr_boot_chunk_head *bch)
{
	struct vnode		*vn;
	struct disklabel	label;
	struct sr_metadata	*md = NULL;
	struct sr_discipline	*fake_sd = NULL;
	struct sr_boot_chunk	*bc;
	char			devname[32];
	dev_t			chrdev, rawdev;
	int			error, i;
	int			rv = SR_META_NOTCLAIMED;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe\n", DEVNAME(sc));

	/*
	 * Use character raw device to avoid SCSI complaints about missing
	 * media on removable media devices.
	 */
	chrdev = blktochr(devno);
	rawdev = MAKEDISKDEV(major(chrdev), DISKUNIT(devno), RAW_PART);
	if (cdevvp(rawdev, &vn)) {
		printf("%s: sr_meta_native_bootprobe: can't allocate vnode\n",
		    DEVNAME(sc));
		goto done;
	}

	/* open device */
	error = VOP_OPEN(vn, FREAD, NOCRED, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe open "
		    "failed\n", DEVNAME(sc));
		vput(vn);
		goto done;
	}

	/* get disklabel */
	error = VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD, NOCRED,
	    curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe ioctl "
		    "failed\n", DEVNAME(sc));
		VOP_CLOSE(vn, FREAD, NOCRED, curproc);
		vput(vn);
		goto done;
	}

	/* we are done, close device */
	error = VOP_CLOSE(vn, FREAD, NOCRED, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe close "
		    "failed\n", DEVNAME(sc));
		vput(vn);
		goto done;
	}
	vput(vn);

	md = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto done;
	}

	/* create fake sd to use utility functions */
	fake_sd = malloc(sizeof(struct sr_discipline), M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (fake_sd == NULL) {
		printf("%s: not enough memory for fake discipline\n",
		    DEVNAME(sc));
		goto done;
	}
	fake_sd->sd_sc = sc;
	fake_sd->sd_meta_type = SR_META_F_NATIVE;

	for (i = 0; i < MAXPARTITIONS; i++) {
		if (label.d_partitions[i].p_fstype != FS_RAID)
			continue;

		/* open partition */
		rawdev = MAKEDISKDEV(major(devno), DISKUNIT(devno), i);
		if (bdevvp(rawdev, &vn)) {
			printf("%s: sr_meta_native_bootprobe: can't allocate "
			    "vnode for partition\n", DEVNAME(sc));
			goto done;
		}
		error = VOP_OPEN(vn, FREAD, NOCRED, curproc);
		if (error) {
			DNPRINTF(SR_D_META, "%s: sr_meta_native_bootprobe "
			    "open failed, partition %d\n",
			    DEVNAME(sc), i);
			vput(vn);
			continue;
		}

		if (sr_meta_native_read(fake_sd, rawdev, md, NULL)) {
			printf("%s: native bootprobe could not read native "
			    "metadata\n", DEVNAME(sc));
			VOP_CLOSE(vn, FREAD, NOCRED, curproc);
			vput(vn);
			continue;
		}

		/* are we a softraid partition? */
		if (md->ssdi.ssd_magic != SR_MAGIC) {
			VOP_CLOSE(vn, FREAD, NOCRED, curproc);
			vput(vn);
			continue;
		}

		sr_meta_getdevname(sc, rawdev, devname, sizeof(devname));
		if (sr_meta_validate(fake_sd, rawdev, md, NULL) == 0) {
			if (md->ssdi.ssd_vol_flags & BIOC_SCNOAUTOASSEMBLE) {
				DNPRINTF(SR_D_META, "%s: don't save %s\n",
				    DEVNAME(sc), devname);
			} else {
				/* XXX fix M_WAITOK, this is boot time */
				bc = malloc(sizeof(struct sr_boot_chunk),
				    M_DEVBUF, M_WAITOK | M_ZERO);
				bc->sbc_metadata =
				    malloc(sizeof(struct sr_metadata),
				    M_DEVBUF, M_WAITOK | M_ZERO);
				bcopy(md, bc->sbc_metadata,
				    sizeof(struct sr_metadata));
				bc->sbc_mm = rawdev;
				SLIST_INSERT_HEAD(bch, bc, sbc_link);
				rv = SR_META_CLAIMED;
			}
		}

		/* we are done, close partition */
		VOP_CLOSE(vn, FREAD, NOCRED, curproc);
		vput(vn);
	}

done:
	if (fake_sd)
		free(fake_sd, M_DEVBUF);
	if (md)
		free(md, M_DEVBUF);

	return (rv);
}

int
sr_boot_assembly(struct sr_softc *sc)
{
	struct sr_boot_volume_head bvh;
	struct sr_boot_chunk_head bch, kdh;
	struct sr_boot_volume	*bv, *bv1, *bv2;
	struct sr_boot_chunk	*bc, *bcnext, *bc1, *bc2;
	struct sr_disk_head	sdklist;
	struct sr_disk		*sdk;
	struct disk		*dk;
	struct bioc_createraid	bcr;
	struct sr_meta_chunk	*hm;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*hotspare, *chunk, *last;
	u_int64_t		*ondisk = NULL;
	dev_t			*devs = NULL;
	char			devname[32];
	int			rv = 0, i;

	DNPRINTF(SR_D_META, "%s: sr_boot_assembly\n", DEVNAME(sc));

	SLIST_INIT(&sdklist);
	SLIST_INIT(&bvh);
	SLIST_INIT(&bch);
	SLIST_INIT(&kdh);

	dk = TAILQ_FIRST(&disklist);
	while (dk != TAILQ_END(&disklist)) {

		/* See if this disk has been checked. */
		SLIST_FOREACH(sdk, &sdklist, sdk_link)
			if (sdk->sdk_devno == dk->dk_devno)
				break;

		if (sdk != NULL || dk->dk_devno == NODEV) {
			dk = TAILQ_NEXT(dk, dk_link);
			continue;
		}

		/* Add this disk to the list that we've checked. */
		sdk = malloc(sizeof(struct sr_disk), M_DEVBUF,
		    M_NOWAIT | M_CANFAIL | M_ZERO);
		if (sdk == NULL)
			goto unwind;
		sdk->sdk_devno = dk->dk_devno;
		SLIST_INSERT_HEAD(&sdklist, sdk, sdk_link);

		/* Only check sd(4) and wd(4) devices. */
		if (strncmp(dk->dk_name, "sd", 2) &&
		    strncmp(dk->dk_name, "wd", 2)) {
			dk = TAILQ_NEXT(dk, dk_link);
			continue;
		}

		/* native softraid uses partitions */
		sr_meta_native_bootprobe(sc, dk->dk_devno, &bch);

		/* probe non-native disks if native failed. */

		/* Restart scan since we may have slept. */
		dk = TAILQ_FIRST(&disklist);
	}

	/*
	 * Create a list of volumes and associate chunks with each volume.
	 */
	for (bc = SLIST_FIRST(&bch); bc != SLIST_END(&bch); bc = bcnext) {

		bcnext = SLIST_NEXT(bc, sbc_link);
		SLIST_REMOVE(&bch, bc, sr_boot_chunk, sbc_link);
		bc->sbc_chunk_id = bc->sbc_metadata->ssdi.ssd_chunk_id;

		/* Handle key disks separately. */
		if (bc->sbc_metadata->ssdi.ssd_level == SR_KEYDISK_LEVEL) {
			SLIST_INSERT_HEAD(&kdh, bc, sbc_link);
			continue;
		}

		SLIST_FOREACH(bv, &bvh, sbv_link) {
			if (bcmp(&bc->sbc_metadata->ssdi.ssd_uuid,
			    &bv->sbv_uuid,
			    sizeof(bc->sbc_metadata->ssdi.ssd_uuid)) == 0)
				break;
		}

		if (bv == NULL) {
			bv = malloc(sizeof(struct sr_boot_volume),
			    M_DEVBUF, M_NOWAIT | M_CANFAIL | M_ZERO);
			if (bv == NULL) {
				printf("%s: failed to allocate boot volume!\n",
				    DEVNAME(sc));
				goto unwind;
			}

			bv->sbv_level = bc->sbc_metadata->ssdi.ssd_level;
			bv->sbv_volid = bc->sbc_metadata->ssdi.ssd_volid;
			bv->sbv_chunk_no = bc->sbc_metadata->ssdi.ssd_chunk_no;
			bcopy(&bc->sbc_metadata->ssdi.ssd_uuid, &bv->sbv_uuid,
			    sizeof(bc->sbc_metadata->ssdi.ssd_uuid));
			SLIST_INIT(&bv->sbv_chunks);

			/* Maintain volume order. */
			bv2 = NULL;
			SLIST_FOREACH(bv1, &bvh, sbv_link) {
				if (bv1->sbv_volid > bv->sbv_volid)
					break;
				bv2 = bv1;
			}
			if (bv2 == NULL) {
				DNPRINTF(SR_D_META, "%s: insert volume %u "
				    "at head\n", DEVNAME(sc), bv->sbv_volid);
				SLIST_INSERT_HEAD(&bvh, bv, sbv_link);
			} else {
				DNPRINTF(SR_D_META, "%s: insert volume %u "
				    "after %u\n", DEVNAME(sc), bv->sbv_volid,
				    bv2->sbv_volid);
				SLIST_INSERT_AFTER(bv2, bv, sbv_link);
			}
		}

		/* Maintain chunk order. */
		bc2 = NULL;
		SLIST_FOREACH(bc1, &bv->sbv_chunks, sbc_link) {
			if (bc1->sbc_chunk_id > bc->sbc_chunk_id)
				break;
			bc2 = bc1;
		}
		if (bc2 == NULL) {
			DNPRINTF(SR_D_META, "%s: volume %u insert chunk %u "
			    "at head\n", DEVNAME(sc), bv->sbv_volid,
			    bc->sbc_chunk_id);
			SLIST_INSERT_HEAD(&bv->sbv_chunks, bc, sbc_link);
		} else {
			DNPRINTF(SR_D_META, "%s: volume %u insert chunk %u "
			    "after %u\n", DEVNAME(sc), bv->sbv_volid,
			    bc->sbc_chunk_id, bc2->sbc_chunk_id);
			SLIST_INSERT_AFTER(bc2, bc, sbc_link);
		}

		bv->sbv_chunks_found++;
	}

	/* Allocate memory for device and ondisk version arrays. */
	devs = malloc(BIOC_CRMAXLEN * sizeof(dev_t), M_DEVBUF,
	    M_NOWAIT | M_CANFAIL);
	if (devs == NULL) {
		printf("%s: failed to allocate device array\n", DEVNAME(sc));
		goto unwind;
	}
	ondisk = malloc(BIOC_CRMAXLEN * sizeof(u_int64_t), M_DEVBUF,
	    M_NOWAIT | M_CANFAIL);
	if (ondisk == NULL) {
		printf("%s: failed to allocate ondisk array\n", DEVNAME(sc));
		goto unwind;
	}

	/*
	 * Assemble hotspare "volumes".
	 */
	SLIST_FOREACH(bv, &bvh, sbv_link) {

		/* Check if this is a hotspare "volume". */
		if (bv->sbv_level != SR_HOTSPARE_LEVEL ||
		    bv->sbv_chunk_no != 1)
			continue;

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: assembling hotspare volume ",
		    DEVNAME(sc));
		if (sr_debug & SR_D_META)
			sr_uuid_print(&bv->sbv_uuid, 0);
		DNPRINTF(SR_D_META, " volid %u with %u chunks\n",
		    bv->sbv_volid, bv->sbv_chunk_no);
#endif

		/* Create hotspare chunk metadata. */
		hotspare = malloc(sizeof(struct sr_chunk), M_DEVBUF,
		    M_NOWAIT | M_CANFAIL | M_ZERO);
		if (hotspare == NULL) {
			printf("%s: failed to allocate hotspare\n",
			    DEVNAME(sc));
			goto unwind;
		}

		bc = SLIST_FIRST(&bv->sbv_chunks);
		sr_meta_getdevname(sc, bc->sbc_mm, devname, sizeof(devname));
		hotspare->src_dev_mm = bc->sbc_mm;
		strlcpy(hotspare->src_devname, devname,
		    sizeof(hotspare->src_devname));
		hotspare->src_size = bc->sbc_metadata->ssdi.ssd_size;

		hm = &hotspare->src_meta;
		hm->scmi.scm_volid = SR_HOTSPARE_VOLID;
		hm->scmi.scm_chunk_id = 0;
		hm->scmi.scm_size = bc->sbc_metadata->ssdi.ssd_size;
		hm->scmi.scm_coerced_size = bc->sbc_metadata->ssdi.ssd_size;
		strlcpy(hm->scmi.scm_devname, devname,
		    sizeof(hm->scmi.scm_devname));
		bcopy(&bc->sbc_metadata->ssdi.ssd_uuid, &hm->scmi.scm_uuid,
		    sizeof(struct sr_uuid));

		sr_checksum(sc, hm, &hm->scm_checksum,
		    sizeof(struct sr_meta_chunk_invariant));

		hm->scm_status = BIOC_SDHOTSPARE;

		/* Add chunk to hotspare list. */
		rw_enter_write(&sc->sc_hs_lock);
		cl = &sc->sc_hotspare_list;
		if (SLIST_EMPTY(cl))
			SLIST_INSERT_HEAD(cl, hotspare, src_link);
		else {
			SLIST_FOREACH(chunk, cl, src_link)
				last = chunk;
			SLIST_INSERT_AFTER(last, hotspare, src_link);
		}
		sc->sc_hotspare_no++;
		rw_exit_write(&sc->sc_hs_lock);

	}

	/*
	 * Assemble RAID volumes.
	 */
	SLIST_FOREACH(bv, &bvh, sbv_link) {

		bzero(&bc, sizeof(bc));

		/* Check if this is a hotspare "volume". */
		if (bv->sbv_level == SR_HOTSPARE_LEVEL &&
		    bv->sbv_chunk_no == 1)
			continue;

#ifdef SR_DEBUG
		DNPRINTF(SR_D_META, "%s: assembling volume ", DEVNAME(sc));
		if (sr_debug & SR_D_META)
			sr_uuid_print(&bv->sbv_uuid, 0);
		DNPRINTF(SR_D_META, " volid %u with %u chunks\n",
		    bv->sbv_volid, bv->sbv_chunk_no);
#endif

		/*
		 * If this is a crypto volume, try to find a matching
		 * key disk...
		 */
		bcr.bc_key_disk = NODEV;
		if (bv->sbv_level == 'C') {
			SLIST_FOREACH(bc, &kdh, sbc_link) {
				if (bcmp(&bc->sbc_metadata->ssdi.ssd_uuid,
				    &bv->sbv_uuid,
				    sizeof(bc->sbc_metadata->ssdi.ssd_uuid))
				    == 0)
					bcr.bc_key_disk = bc->sbc_mm;
			}
		}

		for (i = 0; i < BIOC_CRMAXLEN; i++) {
			devs[i] = NODEV; /* mark device as illegal */
			ondisk[i] = 0;
		}

		SLIST_FOREACH(bc, &bv->sbv_chunks, sbc_link) {
			if (devs[bc->sbc_chunk_id] != NODEV) {
				bv->sbv_chunks_found--;
				sr_meta_getdevname(sc, bc->sbc_mm, devname,
				    sizeof(devname));
				printf("%s: found duplicate chunk %u for "
				    "volume %u on device %s\n", DEVNAME(sc),
				    bc->sbc_chunk_id, bv->sbv_volid, devname);
			}

			if (devs[bc->sbc_chunk_id] == NODEV ||
			    bc->sbc_metadata->ssd_ondisk >
			    ondisk[bc->sbc_chunk_id]) {
				devs[bc->sbc_chunk_id] = bc->sbc_mm;
				ondisk[bc->sbc_chunk_id] =
				    bc->sbc_metadata->ssd_ondisk;
				DNPRINTF(SR_D_META, "%s: using ondisk "
				    "metadata version %llu for chunk %u\n",
				    DEVNAME(sc), ondisk[bc->sbc_chunk_id],
				    bc->sbc_chunk_id);
			}
		}

		if (bv->sbv_chunk_no != bv->sbv_chunks_found) {
			printf("%s: not all chunks were provided; "
			    "attempting to bring volume %d online\n",
			    DEVNAME(sc), bv->sbv_volid);
		}

		bcr.bc_level = bv->sbv_level;
		bcr.bc_dev_list_len = bv->sbv_chunk_no * sizeof(dev_t);
		bcr.bc_dev_list = devs;
		bcr.bc_flags = BIOC_SCDEVT;

		rw_enter_write(&sc->sc_lock);
		sr_ioctl_createraid(sc, &bcr, 0);
		rw_exit_write(&sc->sc_lock);

		rv++;
	}

	/* done with metadata */
unwind:
	/* Free boot volumes and associated chunks. */
	for (bv1 = SLIST_FIRST(&bvh); bv1 != SLIST_END(&bvh); bv1 = bv2) {
		bv2 = SLIST_NEXT(bv1, sbv_link);
		for (bc1 = SLIST_FIRST(&bv1->sbv_chunks);
		    bc1 != SLIST_END(&bv1->sbv_chunks); bc1 = bc2) {
			bc2 = SLIST_NEXT(bc1, sbc_link);
			if (bc1->sbc_metadata)
				free(bc1->sbc_metadata, M_DEVBUF);
			free(bc1, M_DEVBUF);
		}
		free(bv1, M_DEVBUF);
	}
	/* Free keydisks chunks. */
	for (bc1 = SLIST_FIRST(&kdh); bc1 != SLIST_END(&kdh); bc1 = bc2) {
		bc2 = SLIST_NEXT(bc1, sbc_link);
		if (bc1->sbc_metadata)
			free(bc1->sbc_metadata, M_DEVBUF);
		free(bc1, M_DEVBUF);
	}
	/* Free unallocated chunks. */
	for (bc1 = SLIST_FIRST(&bch); bc1 != SLIST_END(&bch); bc1 = bc2) {
		bc2 = SLIST_NEXT(bc1, sbc_link);
		if (bc1->sbc_metadata)
			free(bc1->sbc_metadata, M_DEVBUF);
		free(bc1, M_DEVBUF);
	}

	while (!SLIST_EMPTY(&sdklist)) {
		sdk = SLIST_FIRST(&sdklist);
		SLIST_REMOVE_HEAD(&sdklist, sdk_link);
		free(sdk, M_DEVBUF);
	}

	if (devs)
		free(devs, M_DEVBUF);
	if (ondisk)
		free(ondisk, M_DEVBUF);

	return (rv);
}

void
sr_map_root(void)
{
	struct sr_softc		*sc = softraid0;
	struct sr_meta_opt_item	*omi;
	struct sr_meta_boot	*sbm;
	u_char			duid[8];
	int			i, j;

	if (sc == NULL)
		return;

	DNPRINTF(SR_D_MISC, "%s: sr_map_root\n", DEVNAME(sc));
	bzero(duid, sizeof(duid));
	if (bcmp(rootduid, duid, sizeof(duid)) == 0) {
		DNPRINTF(SR_D_MISC, "%s: root duid is zero\n", DEVNAME(sc));
		return;
	}

	for (i = 0; i < SR_MAX_LD; i++) {
		if (sc->sc_dis[i] == NULL)
			continue;
		SLIST_FOREACH(omi, &sc->sc_dis[i]->sd_meta_opt, omi_link) {
			if (omi->omi_som->som_type != SR_OPT_BOOT)
				continue;
			sbm = (struct sr_meta_boot *)omi->omi_som;
			for (j = 0; j < SR_MAX_BOOT_DISKS; j++) {
				if (bcmp(rootduid, sbm->sbm_boot_duid[j],
				    sizeof(rootduid)) == 0) {
					bcopy(sbm->sbm_root_duid, rootduid,
					    sizeof(rootduid));
					DNPRINTF(SR_D_MISC, "%s: root duid "
					    "mapped to %02hx%02hx%02hx%02hx"
					    "%02hx%02hx%02hx%02hx\n",
					    DEVNAME(sc), rootduid[0],
					    rootduid[1], rootduid[2],
					    rootduid[3], rootduid[4],
					    rootduid[5], rootduid[6],
					    rootduid[7]);
					return;
				}
			}
		}
	}
}

int
sr_meta_native_probe(struct sr_softc *sc, struct sr_chunk *ch_entry)
{
	struct disklabel	label;
	char			*devname;
	int			error, part;
	daddr64_t		size;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_probe(%s)\n",
	   DEVNAME(sc), ch_entry->src_devname);

	devname = ch_entry->src_devname;
	part = DISKPART(ch_entry->src_dev_mm);

	/* get disklabel */
	error = VOP_IOCTL(ch_entry->src_vn, DIOCGDINFO, (caddr_t)&label, FREAD,
	    NOCRED, curproc);
	if (error) {
		DNPRINTF(SR_D_META, "%s: %s can't obtain disklabel\n",
		    DEVNAME(sc), devname);
		goto unwind;
	}
	bcopy(label.d_uid, ch_entry->src_duid, sizeof(ch_entry->src_duid));

	/* make sure the partition is of the right type */
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		DNPRINTF(SR_D_META,
		    "%s: %s partition not of type RAID (%d)\n", DEVNAME(sc),
		    devname,
		    label.d_partitions[part].p_fstype);
		goto unwind;
	}

	size = DL_GETPSIZE(&label.d_partitions[part]) - SR_DATA_OFFSET;
	if (size <= 0) {
		DNPRINTF(SR_D_META, "%s: %s partition too small\n", DEVNAME(sc),
		    devname);
		goto unwind;
	}
	ch_entry->src_size = size;

	DNPRINTF(SR_D_META, "%s: probe found %s size %d\n", DEVNAME(sc),
	    devname, size);

	return (SR_META_F_NATIVE);
unwind:
	DNPRINTF(SR_D_META, "%s: invalid device: %s\n", DEVNAME(sc),
	    devname ? devname : "nodev");
	return (SR_META_F_INVALID);
}

int
sr_meta_native_attach(struct sr_discipline *sd, int force)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk_head 	*cl = &sd->sd_vol.sv_chunk_list;
	struct sr_metadata	*md = NULL;
	struct sr_chunk		*ch_entry, *ch_next;
	struct sr_uuid		uuid;
	u_int64_t		version = 0;
	int			sr, not_sr, rv = 1, d, expected = -1, old_meta = 0;

	DNPRINTF(SR_D_META, "%s: sr_meta_native_attach\n", DEVNAME(sc));

	md = malloc(SR_META_SIZE * 512, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (md == NULL) {
		printf("%s: not enough memory for metadata buffer\n",
		    DEVNAME(sc));
		goto bad;
	}

	bzero(&uuid, sizeof uuid);

	sr = not_sr = d = 0;
	SLIST_FOREACH(ch_entry, cl, src_link) {
		if (ch_entry->src_dev_mm == NODEV)
			continue;

		if (sr_meta_native_read(sd, ch_entry->src_dev_mm, md, NULL)) {
			printf("%s: could not read native metadata\n",
			    DEVNAME(sc));
			goto bad;
		}

		if (md->ssdi.ssd_magic == SR_MAGIC) {
			sr++;
			ch_entry->src_meta.scmi.scm_chunk_id =
			    md->ssdi.ssd_chunk_id;
			if (d == 0) {
				bcopy(&md->ssdi.ssd_uuid, &uuid, sizeof uuid);
				expected = md->ssdi.ssd_chunk_no;
				version = md->ssd_ondisk;
				d++;
				continue;
			} else if (bcmp(&md->ssdi.ssd_uuid, &uuid,
			    sizeof uuid)) {
				printf("%s: not part of the same volume\n",
				    DEVNAME(sc));
				goto bad;
			}
			if (md->ssd_ondisk != version) {
				old_meta++;
				version = MAX(md->ssd_ondisk, version);
			}
		} else
			not_sr++;
	}

	if (sr && not_sr) {
		printf("%s: not all chunks are of the native metadata format\n",
		    DEVNAME(sc));
		goto bad;
	}

	/* mixed metadata versions; mark bad disks offline */
	if (old_meta) {
		d = 0;
		for (ch_entry = SLIST_FIRST(cl); ch_entry != SLIST_END(cl);
		    ch_entry = ch_next, d++) {
			ch_next = SLIST_NEXT(ch_entry, src_link);

			/* XXX do we want to read this again? */
			if (ch_entry->src_dev_mm == NODEV)
				panic("src_dev_mm == NODEV");
			if (sr_meta_native_read(sd, ch_entry->src_dev_mm, md,
			    NULL))
				printf("%s: could not read native metadata\n",
				    DEVNAME(sc));
			if (md->ssd_ondisk != version)
				sd->sd_vol.sv_chunks[d]->src_meta.scm_status =
				    BIOC_SDOFFLINE;
		}
	}

	if (expected != sr && !force && expected != -1) {
		DNPRINTF(SR_D_META, "%s: not all chunks were provided, trying "
		    "anyway\n", DEVNAME(sc));
	}

	rv = 0;
bad:
	if (md)
		free(md, M_DEVBUF);
	return (rv);
}

int
sr_meta_native_read(struct sr_discipline *sd, dev_t dev,
    struct sr_metadata *md, void *fm)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	DNPRINTF(SR_D_META, "%s: sr_meta_native_read(0x%x, %p)\n",
	    DEVNAME(sc), dev, md);

	return (sr_meta_rw(sd, dev, md, SR_META_SIZE * 512, SR_META_OFFSET,
	    B_READ));
}

int
sr_meta_native_write(struct sr_discipline *sd, dev_t dev,
    struct sr_metadata *md, void *fm)
{
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif
	DNPRINTF(SR_D_META, "%s: sr_meta_native_write(0x%x, %p)\n",
	    DEVNAME(sc), dev, md);

	return (sr_meta_rw(sd, dev, md, SR_META_SIZE * 512, SR_META_OFFSET,
	    B_WRITE));
}

void
sr_hotplug_register(struct sr_discipline *sd, void *func)
{
	struct sr_hotplug_list	*mhe;

	DNPRINTF(SR_D_MISC, "%s: sr_hotplug_register: %p\n",
	    DEVNAME(sd->sd_sc), func);

	/* make sure we aren't on the list yet */
	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_hotplug == func)
			return;

	mhe = malloc(sizeof(struct sr_hotplug_list), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	mhe->sh_hotplug = func;
	mhe->sh_sd = sd;
	SLIST_INSERT_HEAD(&sr_hotplug_callbacks, mhe, shl_link);
}

void
sr_hotplug_unregister(struct sr_discipline *sd, void *func)
{
	struct sr_hotplug_list	*mhe;

	DNPRINTF(SR_D_MISC, "%s: sr_hotplug_unregister: %s %p\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname, func);

	/* make sure we are on the list yet */
	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_hotplug == func) {
			SLIST_REMOVE(&sr_hotplug_callbacks, mhe,
			    sr_hotplug_list, shl_link);
			free(mhe, M_DEVBUF);
			if (SLIST_EMPTY(&sr_hotplug_callbacks))
				SLIST_INIT(&sr_hotplug_callbacks);
			return;
		}
}

void
sr_disk_attach(struct disk *diskp, int action)
{
	struct sr_hotplug_list	*mhe;

	SLIST_FOREACH(mhe, &sr_hotplug_callbacks, shl_link)
		if (mhe->sh_sd->sd_ready)
			mhe->sh_hotplug(mhe->sh_sd, diskp, action);
}

int
sr_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
sr_attach(struct device *parent, struct device *self, void *aux)
{
	struct sr_softc		*sc = (void *)self;
	struct scsibus_attach_args saa;

	DNPRINTF(SR_D_MISC, "\n%s: sr_attach", DEVNAME(sc));

	if (softraid0 == NULL)
		softraid0 = sc;

	rw_init(&sc->sc_lock, "sr_lock");
	rw_init(&sc->sc_hs_lock, "sr_hs_lock");

	SLIST_INIT(&sr_hotplug_callbacks);
	SLIST_INIT(&sc->sc_hotspare_list);

#if NBIO > 0
	if (bio_register(&sc->sc_dev, sr_ioctl) != 0)
		printf("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = sr_ioctl;
#endif /* NBIO > 0 */

#ifndef SMALL_KERNEL
	strlcpy(sc->sc_sensordev.xname, DEVNAME(sc),
	    sizeof(sc->sc_sensordev.xname));
	sensordev_install(&sc->sc_sensordev);
#endif /* SMALL_KERNEL */

	printf("\n");

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &sr_switch;
	sc->sc_link.adapter_target = SR_MAX_LD;
	sc->sc_link.adapter_buswidth = SR_MAX_LD;
	sc->sc_link.luns = 1;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_scsibus = (struct scsibus_softc *)config_found(&sc->sc_dev,
	    &saa, scsiprint);

	softraid_disk_attach = sr_disk_attach;

	sc->sc_shutdownhook = shutdownhook_establish(sr_shutdownhook, sc);

	sr_boot_assembly(sc);
}

int
sr_detach(struct device *self, int flags)
{
	struct sr_softc		*sc = (void *)self;
	int			rv;

	DNPRINTF(SR_D_MISC, "%s: sr_detach\n", DEVNAME(sc));

	if (sc->sc_shutdownhook)
		shutdownhook_disestablish(sc->sc_shutdownhook);

	sr_shutdown(sc);

#ifndef SMALL_KERNEL
	if (sc->sc_sensor_task != NULL)
		sensor_task_unregister(sc->sc_sensor_task);
	sensordev_deinstall(&sc->sc_sensordev);
#endif /* SMALL_KERNEL */

	if (sc->sc_scsibus != NULL) {
		rv = config_detach((struct device *)sc->sc_scsibus, flags);
		if (rv != 0)
			return (rv);
		sc->sc_scsibus = NULL;
	}

	return (rv);
}

void
sr_minphys(struct buf *bp, struct scsi_link *sl)
{
	DNPRINTF(SR_D_MISC, "sr_minphys: %d\n", bp->b_bcount);

	/* XXX currently using SR_MAXFER = MAXPHYS */
	if (bp->b_bcount > SR_MAXFER)
		bp->b_bcount = SR_MAXFER;
	minphys(bp);
}

void
sr_copy_internal_data(struct scsi_xfer *xs, void *v, size_t size)
{
	size_t			copy_cnt;

	DNPRINTF(SR_D_MISC, "sr_copy_internal_data xs: %p size: %d\n",
	    xs, size);

	if (xs->datalen) {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(v, xs->data, copy_cnt);
	}
}

int
sr_ccb_alloc(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;
	int			i;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_alloc\n", DEVNAME(sd->sd_sc));

	if (sd->sd_ccb)
		return (1);

	sd->sd_ccb = malloc(sizeof(struct sr_ccb) *
	    sd->sd_max_wu * sd->sd_max_ccb_per_wu, M_DEVBUF, M_WAITOK | M_ZERO);
	TAILQ_INIT(&sd->sd_ccb_freeq);
	for (i = 0; i < sd->sd_max_wu * sd->sd_max_ccb_per_wu; i++) {
		ccb = &sd->sd_ccb[i];
		ccb->ccb_dis = sd;
		sr_ccb_put(ccb);
	}

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_alloc ccb: %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_max_wu * sd->sd_max_ccb_per_wu);

	return (0);
}

void
sr_ccb_free(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;

	if (!sd)
		return;

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_free %p\n", DEVNAME(sd->sd_sc), sd);

	while ((ccb = TAILQ_FIRST(&sd->sd_ccb_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_ccb_freeq, ccb, ccb_link);

	if (sd->sd_ccb)
		free(sd->sd_ccb, M_DEVBUF);
}

struct sr_ccb *
sr_ccb_get(struct sr_discipline *sd)
{
	struct sr_ccb		*ccb;
	int			s;

	s = splbio();

	ccb = TAILQ_FIRST(&sd->sd_ccb_freeq);
	if (ccb) {
		TAILQ_REMOVE(&sd->sd_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = SR_CCB_INPROGRESS;
	}

	splx(s);

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_get: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	return (ccb);
}

void
sr_ccb_put(struct sr_ccb *ccb)
{
	struct sr_discipline	*sd = ccb->ccb_dis;
	int			s;

	DNPRINTF(SR_D_CCB, "%s: sr_ccb_put: %p\n", DEVNAME(sd->sd_sc),
	    ccb);

	s = splbio();

	ccb->ccb_wu = NULL;
	ccb->ccb_state = SR_CCB_FREE;
	ccb->ccb_target = -1;
	ccb->ccb_opaque = NULL;

	TAILQ_INSERT_TAIL(&sd->sd_ccb_freeq, ccb, ccb_link);

	splx(s);
}

int
sr_wu_alloc(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;
	int			i, no_wu;

	if (!sd)
		return (1);

	DNPRINTF(SR_D_WU, "%s: sr_wu_alloc %p %d\n", DEVNAME(sd->sd_sc),
	    sd, sd->sd_max_wu);

	if (sd->sd_wu)
		return (1);

	no_wu = sd->sd_max_wu;
	sd->sd_wu_pending = no_wu;

	sd->sd_wu = malloc(sizeof(struct sr_workunit) * no_wu,
	    M_DEVBUF, M_WAITOK | M_ZERO);
	TAILQ_INIT(&sd->sd_wu_freeq);
	TAILQ_INIT(&sd->sd_wu_pendq);
	TAILQ_INIT(&sd->sd_wu_defq);
	for (i = 0; i < no_wu; i++) {
		wu = &sd->sd_wu[i];
		wu->swu_dis = sd;
		sr_wu_put(sd, wu);
	}

	return (0);
}

void
sr_wu_free(struct sr_discipline *sd)
{
	struct sr_workunit	*wu;

	if (!sd)
		return;

	DNPRINTF(SR_D_WU, "%s: sr_wu_free %p\n", DEVNAME(sd->sd_sc), sd);

	while ((wu = TAILQ_FIRST(&sd->sd_wu_freeq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
	while ((wu = TAILQ_FIRST(&sd->sd_wu_pendq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);
	while ((wu = TAILQ_FIRST(&sd->sd_wu_defq)) != NULL)
		TAILQ_REMOVE(&sd->sd_wu_defq, wu, swu_link);

	if (sd->sd_wu)
		free(sd->sd_wu, M_DEVBUF);
}

void
sr_wu_put(void *xsd, void *xwu)
{
	struct sr_discipline	*sd = (struct sr_discipline *)xsd;
	struct sr_workunit	*wu = (struct sr_workunit *)xwu;
	struct sr_ccb		*ccb;

	int			s;

	DNPRINTF(SR_D_WU, "%s: sr_wu_put: %p\n", DEVNAME(sd->sd_sc), wu);

	s = splbio();
	if (wu->swu_cb_active == 1)
		panic("%s: sr_wu_put got active wu", DEVNAME(sd->sd_sc));
	while ((ccb = TAILQ_FIRST(&wu->swu_ccb)) != NULL) {
		TAILQ_REMOVE(&wu->swu_ccb, ccb, ccb_link);
		sr_ccb_put(ccb);
	}
	splx(s);

	bzero(wu, sizeof(*wu));
	TAILQ_INIT(&wu->swu_ccb);
	wu->swu_dis = sd;

	mtx_enter(&sd->sd_wu_mtx);
	TAILQ_INSERT_TAIL(&sd->sd_wu_freeq, wu, swu_link);
	sd->sd_wu_pending--;
	mtx_leave(&sd->sd_wu_mtx);
}

void *
sr_wu_get(void *xsd)
{
	struct sr_discipline	*sd = (struct sr_discipline *)xsd;
	struct sr_workunit	*wu;

	mtx_enter(&sd->sd_wu_mtx);
	wu = TAILQ_FIRST(&sd->sd_wu_freeq);
	if (wu) {
		TAILQ_REMOVE(&sd->sd_wu_freeq, wu, swu_link);
		sd->sd_wu_pending++;
	}
	mtx_leave(&sd->sd_wu_mtx);

	DNPRINTF(SR_D_WU, "%s: sr_wu_get: %p\n", DEVNAME(sd->sd_sc), wu);

	return (wu);
}

void
sr_scsi_done(struct sr_discipline *sd, struct scsi_xfer *xs)
{
	DNPRINTF(SR_D_DIS, "%s: sr_scsi_done: xs %p\n", DEVNAME(sd->sd_sc), xs);

	scsi_done(xs);
}

void
sr_scsi_cmd(struct scsi_xfer *xs)
{
	int			s;
	struct scsi_link	*link = xs->sc_link;
	struct sr_softc		*sc = link->adapter_softc;
	struct sr_workunit	*wu = NULL;
	struct sr_discipline	*sd;
	struct sr_ccb		*ccb;

	DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: target %d xs: %p "
	    "flags: %#x\n", DEVNAME(sc), link->target, xs, xs->flags);

	sd = sc->sc_dis[link->target];
	if (sd == NULL) {
		printf("%s: sr_scsi_cmd NULL discipline\n", DEVNAME(sc));
		goto stuffup;
	}

	if (sd->sd_deleted) {
		printf("%s: %s device is being deleted, failing io\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname);
		goto stuffup;
	}

	wu = xs->io;
	/* scsi layer *can* re-send wu without calling sr_wu_put(). */
	s = splbio();
	if (wu->swu_cb_active == 1)
		panic("%s: sr_scsi_cmd got active wu", DEVNAME(sd->sd_sc));
	while ((ccb = TAILQ_FIRST(&wu->swu_ccb)) != NULL) {
		TAILQ_REMOVE(&wu->swu_ccb, ccb, ccb_link);
		sr_ccb_put(ccb);
	}
	splx(s);

	bzero(wu, sizeof(*wu));
	TAILQ_INIT(&wu->swu_ccb);
	wu->swu_state = SR_WU_INPROGRESS;
	wu->swu_dis = sd;
	wu->swu_xs = xs;

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case READ_16:
	case WRITE_COMMAND:
	case WRITE_BIG:
	case WRITE_16:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: READ/WRITE %02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		if (sd->sd_scsi_rw(wu))
			goto stuffup;
		break;

	case SYNCHRONIZE_CACHE:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: SYNCHRONIZE_CACHE\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_sync(wu))
			goto stuffup;
		goto complete;

	case TEST_UNIT_READY:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: TEST_UNIT_READY\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_tur(wu))
			goto stuffup;
		goto complete;

	case START_STOP:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: START_STOP\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_start_stop(wu))
			goto stuffup;
		goto complete;

	case INQUIRY:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd: INQUIRY\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_inquiry(wu))
			goto stuffup;
		goto complete;

	case READ_CAPACITY:
	case READ_CAPACITY_16:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd READ CAPACITY 0x%02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		if (sd->sd_scsi_read_cap(wu))
			goto stuffup;
		goto complete;

	case REQUEST_SENSE:
		DNPRINTF(SR_D_CMD, "%s: sr_scsi_cmd REQUEST SENSE\n",
		    DEVNAME(sc));
		if (sd->sd_scsi_req_sense(wu))
			goto stuffup;
		goto complete;

	default:
		DNPRINTF(SR_D_CMD, "%s: unsupported scsi command %x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		/* XXX might need to add generic function to handle others */
		goto stuffup;
	}

	return;
stuffup:
	if (sd && sd->sd_scsi_sense.error_code) {
		xs->error = XS_SENSE;
		bcopy(&sd->sd_scsi_sense, &xs->sense, sizeof(xs->sense));
		bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));
	} else {
		xs->error = XS_DRIVER_STUFFUP;
	}
complete:
	sr_scsi_done(sd, xs);
}

int
sr_scsi_probe(struct scsi_link *link)
{
	struct sr_softc		*sc = link->adapter_softc;
	struct sr_discipline	*sd;

	KASSERT(link->target < SR_MAX_LD && link->lun == 0);

	sd = sc->sc_dis[link->target];
	if (sd == NULL)
		return (ENODEV);

	link->pool = &sd->sd_iopool;
	if (sd->sd_openings)
		link->openings = sd->sd_openings(sd);
	else
		link->openings = sd->sd_max_wu;

	return (0);
}

int
sr_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag)
{
	DNPRINTF(SR_D_IOCTL, "%s: sr_scsi_ioctl cmd: %#x\n",
	    DEVNAME((struct sr_softc *)link->adapter_softc), cmd);

	switch (cmd) {
	case DIOCGCACHE:
	case DIOCSCACHE:
		return (EOPNOTSUPP);
	default:
		return (sr_ioctl(link->adapter_softc, cmd, addr));
	}
}

int
sr_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct sr_softc		*sc = (struct sr_softc *)dev;
	int			rv = 0;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl ", DEVNAME(sc));

	rw_enter_write(&sc->sc_lock);

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(SR_D_IOCTL, "inq\n");
		rv = sr_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		DNPRINTF(SR_D_IOCTL, "vol\n");
		rv = sr_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		DNPRINTF(SR_D_IOCTL, "disk\n");
		rv = sr_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		DNPRINTF(SR_D_IOCTL, "alarm\n");
		/*rv = sr_ioctl_alarm(sc, (struct bioc_alarm *)addr); */
		break;

	case BIOCBLINK:
		DNPRINTF(SR_D_IOCTL, "blink\n");
		/*rv = sr_ioctl_blink(sc, (struct bioc_blink *)addr); */
		break;

	case BIOCSETSTATE:
		DNPRINTF(SR_D_IOCTL, "setstate\n");
		rv = sr_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	case BIOCCREATERAID:
		DNPRINTF(SR_D_IOCTL, "createraid\n");
		rv = sr_ioctl_createraid(sc, (struct bioc_createraid *)addr, 1);
		break;

	case BIOCDELETERAID:
		DNPRINTF(SR_D_IOCTL, "deleteraid\n");
		rv = sr_ioctl_deleteraid(sc, (struct bioc_deleteraid *)addr);
		break;

	case BIOCDISCIPLINE:
		DNPRINTF(SR_D_IOCTL, "discipline\n");
		rv = sr_ioctl_discipline(sc, (struct bioc_discipline *)addr);
		break;

	case BIOCINSTALLBOOT:
		DNPRINTF(SR_D_IOCTL, "installboot\n");
		rv = sr_ioctl_installboot(sc, (struct bioc_installboot *)addr);
		break;

	default:
		DNPRINTF(SR_D_IOCTL, "invalid ioctl\n");
		rv = ENOTTY;
	}

	rw_exit_write(&sc->sc_lock);

	return (rv);
}

int
sr_ioctl_inq(struct sr_softc *sc, struct bioc_inq *bi)
{
	int			i, vol, disk;

	for (i = 0, vol = 0, disk = 0; i < SR_MAX_LD; i++)
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i]) {
			vol++;
			disk += sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no;
		}

	strlcpy(bi->bi_dev, sc->sc_dev.dv_xname, sizeof(bi->bi_dev));
	bi->bi_novol = vol + sc->sc_hotspare_no;
	bi->bi_nodisk = disk + sc->sc_hotspare_no;

	return (0);
}

int
sr_ioctl_vol(struct sr_softc *sc, struct bioc_vol *bv)
{
	int			i, vol, rv = EINVAL;
	struct sr_discipline	*sd;
	struct sr_chunk		*hotspare;
	daddr64_t		rb, sz;

	for (i = 0, vol = -1; i < SR_MAX_LD; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bv->bv_volid)
			continue;

		if (sc->sc_dis[i] == NULL)
			goto done;

		sd = sc->sc_dis[i];
		bv->bv_status = sd->sd_vol_status;
		bv->bv_size = sd->sd_meta->ssdi.ssd_size << DEV_BSHIFT;
		bv->bv_level = sd->sd_meta->ssdi.ssd_level;
		bv->bv_nodisk = sd->sd_meta->ssdi.ssd_chunk_no;

#ifdef CRYPTO
		if (sd->sd_meta->ssdi.ssd_level == 'C' &&
		    sd->mds.mdd_crypto.key_disk != NULL)
			bv->bv_nodisk++;
#endif

		if (bv->bv_status == BIOC_SVREBUILD) {
			sz = sd->sd_meta->ssdi.ssd_size;
			rb = sd->sd_meta->ssd_rebuild;
			if (rb > 0)
				bv->bv_percent = 100 -
				    ((sz * 100 - rb * 100) / sz) - 1;
			else
				bv->bv_percent = 0;
		}
		strlcpy(bv->bv_dev, sd->sd_meta->ssd_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, sd->sd_meta->ssdi.ssd_vendor,
		    sizeof(bv->bv_vendor));
		rv = 0;
		goto done;
	}

	/* Check hotspares list. */
	SLIST_FOREACH(hotspare, &sc->sc_hotspare_list, src_link) {
		vol++;
		if (vol != bv->bv_volid)
			continue;

		bv->bv_status = BIOC_SVONLINE;
		bv->bv_size = hotspare->src_meta.scmi.scm_size << DEV_BSHIFT;
		bv->bv_level = -1;	/* Hotspare. */
		bv->bv_nodisk = 1;
		strlcpy(bv->bv_dev, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bv->bv_dev));
		strlcpy(bv->bv_vendor, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bv->bv_vendor));
		rv = 0;
		goto done;
	}

done:
	return (rv);
}

int
sr_ioctl_disk(struct sr_softc *sc, struct bioc_disk *bd)
{
	int			i, vol, rv = EINVAL, id;
	struct sr_chunk		*src, *hotspare;

	for (i = 0, vol = -1; i < SR_MAX_LD; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bd->bd_volid)
			continue;

		if (sc->sc_dis[i] == NULL)
			goto done;

		id = bd->bd_diskid;

		if (id < sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no)
			src = sc->sc_dis[i]->sd_vol.sv_chunks[id];
#ifdef CRYPTO
		else if (id == sc->sc_dis[i]->sd_meta->ssdi.ssd_chunk_no &&
		    sc->sc_dis[i]->sd_meta->ssdi.ssd_level == 'C' &&
		    sc->sc_dis[i]->mds.mdd_crypto.key_disk != NULL)
			src = sc->sc_dis[i]->mds.mdd_crypto.key_disk;
#endif
		else
			break;

		bd->bd_status = src->src_meta.scm_status;
		bd->bd_size = src->src_meta.scmi.scm_size << DEV_BSHIFT;
		bd->bd_channel = vol;
		bd->bd_target = id;
		strlcpy(bd->bd_vendor, src->src_meta.scmi.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		goto done;
	}

	/* Check hotspares list. */
	SLIST_FOREACH(hotspare, &sc->sc_hotspare_list, src_link) {
		vol++;
		if (vol != bd->bd_volid)
			continue;

		if (bd->bd_diskid != 0)
			break;

		bd->bd_status = hotspare->src_meta.scm_status;
		bd->bd_size = hotspare->src_meta.scmi.scm_size << DEV_BSHIFT;
		bd->bd_channel = vol;
		bd->bd_target = bd->bd_diskid;
		strlcpy(bd->bd_vendor, hotspare->src_meta.scmi.scm_devname,
		    sizeof(bd->bd_vendor));
		rv = 0;
		goto done;
	}

done:
	return (rv);
}

int
sr_ioctl_setstate(struct sr_softc *sc, struct bioc_setstate *bs)
{
	int			rv = EINVAL;
	int			i, vol, found, c;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct sr_chunk_head	*cl;

	if (bs->bs_other_id_type == BIOC_SSOTHER_UNUSED)
		goto done;

	if (bs->bs_status == BIOC_SSHOTSPARE) {
		rv = sr_hotspare(sc, (dev_t)bs->bs_other_id);
		goto done;
	}

	for (i = 0, vol = -1; i < SR_MAX_LD; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (sc->sc_dis[i])
			vol++;
		if (vol != bs->bs_volid)
			continue;
		sd = sc->sc_dis[i];
		break;
	}
	if (sd == NULL)
		goto done;

	switch (bs->bs_status) {
	case BIOC_SSOFFLINE:
		/* Take chunk offline */
		found = c = 0;
		cl = &sd->sd_vol.sv_chunk_list;
		SLIST_FOREACH(ch_entry, cl, src_link) {
			if (ch_entry->src_dev_mm == bs->bs_other_id) {
				found = 1;
				break;
			}
			c++;
		}
		if (found == 0) {
			printf("%s: chunk not part of array\n", DEVNAME(sc));
			goto done;
		}

		/* XXX: check current state first */
		sd->sd_set_chunk_state(sd, c, BIOC_SDOFFLINE);

		if (sr_meta_save(sd, SR_META_DIRTY)) {
			printf("%s: could not save metadata to %s\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);
			goto done;
		}
		rv = 0;
		break;

	case BIOC_SDSCRUB:
		break;

	case BIOC_SSREBUILD:
		rv = sr_rebuild_init(sd, (dev_t)bs->bs_other_id, 0);
		break;

	default:
		printf("%s: unsupported state request %d\n",
		    DEVNAME(sc), bs->bs_status);
	}

done:
	return (rv);
}

int
sr_chunk_in_use(struct sr_softc *sc, dev_t dev)
{
	struct sr_discipline	*sd;
	struct sr_chunk		*chunk;
	int			i, c;

	DNPRINTF(SR_D_MISC, "%s: sr_chunk_in_use(%d)\n", DEVNAME(sc), dev);

	if (dev == NODEV)
		return BIOC_SDINVALID;

	/* See if chunk is already in use. */
	for (i = 0; i < SR_MAX_LD; i++) {
		if (sc->sc_dis[i] == NULL)
			continue;
		sd = sc->sc_dis[i];
		for (c = 0; c < sd->sd_meta->ssdi.ssd_chunk_no; c++) {
			chunk = sd->sd_vol.sv_chunks[c];
			if (chunk->src_dev_mm == dev)
				return chunk->src_meta.scm_status;
		}
	}

	/* Check hotspares list. */
	SLIST_FOREACH(chunk, &sc->sc_hotspare_list, src_link)
		if (chunk->src_dev_mm == dev)
			return chunk->src_meta.scm_status;

	return BIOC_SDINVALID;
}

int
sr_hotspare(struct sr_softc *sc, dev_t dev)
{
	struct sr_discipline	*sd = NULL;
	struct sr_metadata	*sm = NULL;
	struct sr_meta_chunk    *hm;
	struct sr_chunk_head	*cl;
	struct sr_chunk		*chunk, *last, *hotspare = NULL;
	struct sr_uuid		uuid;
	struct disklabel	label;
	struct vnode		*vn;
	daddr64_t		size;
	char			devname[32];
	int			rv = EINVAL;
	int			c, part, open = 0;

	/*
	 * Add device to global hotspares list.
	 */

	sr_meta_getdevname(sc, dev, devname, sizeof(devname));

	/* Make sure chunk is not already in use. */
	c = sr_chunk_in_use(sc, dev);
	if (c != BIOC_SDINVALID && c != BIOC_SDOFFLINE) {
		if (c == BIOC_SDHOTSPARE)
			printf("%s: %s is already a hotspare\n",
			    DEVNAME(sc), devname);
		else
			printf("%s: %s is already in use\n",
			    DEVNAME(sc), devname);
		goto done;
	}

	/* XXX - See if there is an existing degraded volume... */

	/* Open device. */
	if (bdevvp(dev, &vn)) {
		printf("%s: sr_hotspare: can't allocate vnode\n", DEVNAME(sc));
		goto done;
	}
	if (VOP_OPEN(vn, FREAD | FWRITE, NOCRED, curproc)) {
		DNPRINTF(SR_D_META,"%s: sr_hotspare cannot open %s\n",
		    DEVNAME(sc), devname);
		vput(vn);
		goto fail;
	}
	open = 1; /* close dev on error */

	/* Get partition details. */
	part = DISKPART(dev);
	if (VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD,
	    NOCRED, curproc)) {
		DNPRINTF(SR_D_META, "%s: sr_hotspare ioctl failed\n",
		    DEVNAME(sc));
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, curproc);
		vput(vn);
		goto fail;
	}
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		printf("%s: %s partition not of type RAID (%d)\n",
		    DEVNAME(sc), devname,
		    label.d_partitions[part].p_fstype);
		goto fail;
	}

	/* Calculate partition size. */
	size = DL_GETPSIZE(&label.d_partitions[part]) - SR_DATA_OFFSET;

	/*
	 * Create and populate chunk metadata.
	 */

	sr_uuid_get(&uuid);
	hotspare = malloc(sizeof(struct sr_chunk), M_DEVBUF, M_WAITOK | M_ZERO);

	hotspare->src_dev_mm = dev;
	hotspare->src_vn = vn;
	strlcpy(hotspare->src_devname, devname, sizeof(hm->scmi.scm_devname));
	hotspare->src_size = size;

	hm = &hotspare->src_meta;
	hm->scmi.scm_volid = SR_HOTSPARE_VOLID;
	hm->scmi.scm_chunk_id = 0;
	hm->scmi.scm_size = size;
	hm->scmi.scm_coerced_size = size;
	strlcpy(hm->scmi.scm_devname, devname, sizeof(hm->scmi.scm_devname));
	bcopy(&uuid, &hm->scmi.scm_uuid, sizeof(struct sr_uuid));

	sr_checksum(sc, hm, &hm->scm_checksum,
	    sizeof(struct sr_meta_chunk_invariant));

	hm->scm_status = BIOC_SDHOTSPARE;

	/*
	 * Create and populate our own discipline and metadata.
	 */

	sm = malloc(sizeof(struct sr_metadata), M_DEVBUF, M_WAITOK | M_ZERO);
	sm->ssdi.ssd_magic = SR_MAGIC;
	sm->ssdi.ssd_version = SR_META_VERSION;
	sm->ssd_ondisk = 0;
	sm->ssdi.ssd_vol_flags = 0;
	bcopy(&uuid, &sm->ssdi.ssd_uuid, sizeof(struct sr_uuid));
	sm->ssdi.ssd_chunk_no = 1;
	sm->ssdi.ssd_volid = SR_HOTSPARE_VOLID;
	sm->ssdi.ssd_level = SR_HOTSPARE_LEVEL;
	sm->ssdi.ssd_size = size;
	strlcpy(sm->ssdi.ssd_vendor, "OPENBSD", sizeof(sm->ssdi.ssd_vendor));
	snprintf(sm->ssdi.ssd_product, sizeof(sm->ssdi.ssd_product),
	    "SR %s", "HOTSPARE");
	snprintf(sm->ssdi.ssd_revision, sizeof(sm->ssdi.ssd_revision),
	    "%03d", SR_META_VERSION);

	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK | M_ZERO);
	sd->sd_sc = sc;
	sd->sd_meta = sm;
	sd->sd_meta_type = SR_META_F_NATIVE;
	sd->sd_vol_status = BIOC_SVONLINE;
	strlcpy(sd->sd_name, "HOTSPARE", sizeof(sd->sd_name));
	SLIST_INIT(&sd->sd_meta_opt);

	/* Add chunk to volume. */
	sd->sd_vol.sv_chunks = malloc(sizeof(struct sr_chunk *), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	sd->sd_vol.sv_chunks[0] = hotspare;
	SLIST_INIT(&sd->sd_vol.sv_chunk_list);
	SLIST_INSERT_HEAD(&sd->sd_vol.sv_chunk_list, hotspare, src_link);

	/* Save metadata. */
	if (sr_meta_save(sd, SR_META_DIRTY)) {
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), devname);
		goto fail;
	}

	/*
	 * Add chunk to hotspare list.
	 */
	rw_enter_write(&sc->sc_hs_lock);
	cl = &sc->sc_hotspare_list;
	if (SLIST_EMPTY(cl))
		SLIST_INSERT_HEAD(cl, hotspare, src_link);
	else {
		SLIST_FOREACH(chunk, cl, src_link)
			last = chunk;
		SLIST_INSERT_AFTER(last, hotspare, src_link);
	}
	sc->sc_hotspare_no++;
	rw_exit_write(&sc->sc_hs_lock);

	rv = 0;
	goto done;

fail:
	if (hotspare)
		free(hotspare, M_DEVBUF);

done:
	if (sd && sd->sd_vol.sv_chunks)
		free(sd->sd_vol.sv_chunks, M_DEVBUF);
	if (sd)
		free(sd, M_DEVBUF);
	if (sm)
		free(sm, M_DEVBUF);
	if (open) {
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, curproc);
		vput(vn);
	}

	return (rv);
}

void
sr_hotspare_rebuild_callback(void *arg1, void *arg2)
{
	sr_hotspare_rebuild((struct sr_discipline *)arg1);
}

void
sr_hotspare_rebuild(struct sr_discipline *sd)
{
	struct sr_chunk_head	*cl;
	struct sr_chunk		*hotspare, *chunk = NULL;
	struct sr_workunit	*wu;
	struct sr_ccb		*ccb;
	int			i, s, chunk_no, busy;

	/*
	 * Attempt to locate a hotspare and initiate rebuild.
	 */

	for (i = 0; i < sd->sd_meta->ssdi.ssd_chunk_no; i++) {
		if (sd->sd_vol.sv_chunks[i]->src_meta.scm_status ==
		    BIOC_SDOFFLINE) {
			chunk_no = i;
			chunk = sd->sd_vol.sv_chunks[i];
			break;
		}
	}

	if (chunk == NULL) {
		printf("%s: no offline chunk found on %s!\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);
		return;
	}

	/* See if we have a suitable hotspare... */
	rw_enter_write(&sd->sd_sc->sc_hs_lock);
	cl = &sd->sd_sc->sc_hotspare_list;
	SLIST_FOREACH(hotspare, cl, src_link)
		if (hotspare->src_size >= chunk->src_size)
			break;

	if (hotspare != NULL) {

		printf("%s: %s volume degraded, will attempt to "
		    "rebuild on hotspare %s\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname, hotspare->src_devname);

		/*
		 * Ensure that all pending I/O completes on the failed chunk
		 * before trying to initiate a rebuild.
		 */
		i = 0;
		do {
			busy = 0;

			s = splbio();
			TAILQ_FOREACH(wu, &sd->sd_wu_pendq, swu_link) {
				TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
					if (ccb->ccb_target == chunk_no)
						busy = 1;
				}
			}
			TAILQ_FOREACH(wu, &sd->sd_wu_defq, swu_link) {
				TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
					if (ccb->ccb_target == chunk_no)
						busy = 1;
				}
			}
			splx(s);

			if (busy) {
				tsleep(sd, PRIBIO, "sr_hotspare", hz);
				i++;
			}

		} while (busy && i < 120);

		DNPRINTF(SR_D_META, "%s: waited %i seconds for I/O to "
		    "complete on failed chunk %s\n", DEVNAME(sd->sd_sc),
		    i, chunk->src_devname);

		if (busy) {
			printf("%s: pending I/O failed to complete on "
			    "failed chunk %s, hotspare rebuild aborted...\n",
			    DEVNAME(sd->sd_sc), chunk->src_devname);
			goto done;
		}

		s = splbio();
		rw_enter_write(&sd->sd_sc->sc_lock);
		if (sr_rebuild_init(sd, hotspare->src_dev_mm, 1) == 0) {

			/* Remove hotspare from available list. */
			sd->sd_sc->sc_hotspare_no--;
			SLIST_REMOVE(cl, hotspare, sr_chunk, src_link);
			free(hotspare, M_DEVBUF);

		}
		rw_exit_write(&sd->sd_sc->sc_lock);
		splx(s);
	}
done:
	rw_exit_write(&sd->sd_sc->sc_hs_lock);
}

int
sr_rebuild_init(struct sr_discipline *sd, dev_t dev, int hotspare)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk		*chunk = NULL;
	struct sr_meta_chunk	*meta;
	struct disklabel	label;
	struct vnode		*vn;
	daddr64_t		size, csize;
	char			devname[32];
	int			rv = EINVAL, open = 0;
	int			cid, i, part, status;

	/*
	 * Attempt to initiate a rebuild onto the specified device.
	 */

	if (!(sd->sd_capabilities & SR_CAP_REBUILD)) {
		printf("%s: discipline does not support rebuild\n",
		    DEVNAME(sc));
		goto done;
	}

	/* make sure volume is in the right state */
	if (sd->sd_vol_status == BIOC_SVREBUILD) {
		printf("%s: rebuild already in progress\n", DEVNAME(sc));
		goto done;
	}
	if (sd->sd_vol_status != BIOC_SVDEGRADED) {
		printf("%s: %s not degraded\n", DEVNAME(sc),
		    sd->sd_meta->ssd_devname);
		goto done;
	}

	/* Find first offline chunk. */
	for (cid = 0; cid < sd->sd_meta->ssdi.ssd_chunk_no; cid++) {
		if (sd->sd_vol.sv_chunks[cid]->src_meta.scm_status ==
		    BIOC_SDOFFLINE) {
			chunk = sd->sd_vol.sv_chunks[cid];
			break;
		}
	}
	if (chunk == NULL) {
		printf("%s: no offline chunks available for rebuild\n",
		    DEVNAME(sc));
		goto done;
	}

	/* Get coerced size from another online chunk. */
	for (i = 0; i < sd->sd_meta->ssdi.ssd_chunk_no; i++) {
		if (sd->sd_vol.sv_chunks[i]->src_meta.scm_status ==
		    BIOC_SDONLINE) {
			meta = &sd->sd_vol.sv_chunks[i]->src_meta;
			csize = meta->scmi.scm_coerced_size;
			break;
		}
	}

	sr_meta_getdevname(sc, dev, devname, sizeof(devname));
	if (bdevvp(dev, &vn)) {
		printf("%s: sr_rebuild_init: can't allocate vnode\n",
		    DEVNAME(sc));
		goto done;
	}
	if (VOP_OPEN(vn, FREAD | FWRITE, NOCRED, curproc)) {
		DNPRINTF(SR_D_META,"%s: sr_ioctl_setstate can't "
		    "open %s\n", DEVNAME(sc), devname);
		vput(vn);
		goto done;
	}
	open = 1; /* close dev on error */

	/* Get disklabel and check partition. */
	part = DISKPART(dev);
	if (VOP_IOCTL(vn, DIOCGDINFO, (caddr_t)&label, FREAD,
	    NOCRED, curproc)) {
		DNPRINTF(SR_D_META, "%s: sr_ioctl_setstate ioctl failed\n",
		    DEVNAME(sc));
		goto done;
	}
	if (label.d_partitions[part].p_fstype != FS_RAID) {
		printf("%s: %s partition not of type RAID (%d)\n",
		    DEVNAME(sc), devname,
		    label.d_partitions[part].p_fstype);
		goto done;
	}

	/* Is the partition large enough? */
	size = DL_GETPSIZE(&label.d_partitions[part]) - SR_DATA_OFFSET;
	if (size < csize) {
		printf("%s: partition too small, at least %llu B required\n",
		    DEVNAME(sc), csize << DEV_BSHIFT);
		goto done;
	} else if (size > csize)
		printf("%s: partition too large, wasting %llu B\n",
		    DEVNAME(sc), (size - csize) << DEV_BSHIFT);

	/* Ensure that this chunk is not already in use. */
	status = sr_chunk_in_use(sc, dev);
	if (status != BIOC_SDINVALID && status != BIOC_SDOFFLINE &&
	    !(hotspare && status == BIOC_SDHOTSPARE)) {
		printf("%s: %s is already in use\n", DEVNAME(sc), devname);
		goto done;
	}

	/* Reset rebuild counter since we rebuilding onto a new chunk. */
	sd->sd_meta->ssd_rebuild = 0;

	open = 0; /* leave dev open from here on out */

	/* Fix up chunk. */
	bcopy(label.d_uid, chunk->src_duid, sizeof(chunk->src_duid));
	chunk->src_dev_mm = dev;
	chunk->src_vn = vn;

	/* Reconstruct metadata. */
	meta = &chunk->src_meta;
	meta->scmi.scm_volid = sd->sd_meta->ssdi.ssd_volid;
	meta->scmi.scm_chunk_id = cid;
	strlcpy(meta->scmi.scm_devname, devname,
	    sizeof(meta->scmi.scm_devname));
	meta->scmi.scm_size = size;
	meta->scmi.scm_coerced_size = csize;
	bcopy(&sd->sd_meta->ssdi.ssd_uuid, &meta->scmi.scm_uuid,
	    sizeof(meta->scmi.scm_uuid));
	sr_checksum(sc, meta, &meta->scm_checksum,
	    sizeof(struct sr_meta_chunk_invariant));

	sd->sd_set_chunk_state(sd, cid, BIOC_SDREBUILD);

	if (sr_meta_save(sd, SR_META_DIRTY)) {
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), devname);
		open = 1;
		goto done;
	}

	printf("%s: rebuild of %s started on %s\n", DEVNAME(sc),
	    sd->sd_meta->ssd_devname, devname);

	sd->sd_reb_abort = 0;
	kthread_create_deferred(sr_rebuild, sd);

	rv = 0;
done:
	if (open) {
		VOP_CLOSE(vn, FREAD | FWRITE, NOCRED, curproc);
		vput(vn);
	}

	return (rv);
}

void
sr_roam_chunks(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_chunk		*chunk;
	struct sr_meta_chunk	*meta;
	int			roamed = 0;

	/* Have any chunks roamed? */
	SLIST_FOREACH(chunk, &sd->sd_vol.sv_chunk_list, src_link) {
		meta = &chunk->src_meta;
		if (strncmp(meta->scmi.scm_devname, chunk->src_devname,
		    sizeof(meta->scmi.scm_devname))) {

			printf("%s: roaming device %s -> %s\n", DEVNAME(sc),
			    meta->scmi.scm_devname, chunk->src_devname);

			strlcpy(meta->scmi.scm_devname, chunk->src_devname,
			    sizeof(meta->scmi.scm_devname));

			roamed++;
		}
	}

	if (roamed)
		sr_meta_save(sd, SR_META_DIRTY);
}

int
sr_ioctl_createraid(struct sr_softc *sc, struct bioc_createraid *bc, int user)
{
	struct sr_meta_opt_item *omi;
	struct sr_chunk_head	*cl;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*ch_entry;
	struct scsi_link	*link;
	struct device		*dev;
	char			devname[32];
	dev_t			*dt;
	int			i, no_chunk, rv = EINVAL, target, vol;
	int			no_meta;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_createraid(%d)\n",
	    DEVNAME(sc), user);

	/* user input */
	if (bc->bc_dev_list_len > BIOC_CRMAXLEN)
		goto unwind;

	dt = malloc(bc->bc_dev_list_len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (user) {
		if (copyin(bc->bc_dev_list, dt, bc->bc_dev_list_len) != 0)
			goto unwind;
	} else
		bcopy(bc->bc_dev_list, dt, bc->bc_dev_list_len);

	/* Initialise discipline. */
	sd = malloc(sizeof(struct sr_discipline), M_DEVBUF, M_WAITOK | M_ZERO);
	sd->sd_sc = sc;
	SLIST_INIT(&sd->sd_meta_opt);
	sd->sd_workq = workq_create("srdis", 1, IPL_BIO);
	if (sd->sd_workq == NULL) {
		printf("%s: could not create workq\n", DEVNAME(sc));
		goto unwind;
	}
	if (sr_discipline_init(sd, bc->bc_level)) {
		printf("%s: could not initialize discipline\n", DEVNAME(sc));
		goto unwind;
	}

	no_chunk = bc->bc_dev_list_len / sizeof(dev_t);
	cl = &sd->sd_vol.sv_chunk_list;
	SLIST_INIT(cl);

	/* Ensure that chunks are not already in use. */
	for (i = 0; i < no_chunk; i++) {
		if (sr_chunk_in_use(sc, dt[i]) != BIOC_SDINVALID) {
			sr_meta_getdevname(sc, dt[i], devname, sizeof(devname));
			printf("%s: chunk %s already in use\n",
			    DEVNAME(sc), devname);
			goto unwind;
		}
	}

	sd->sd_meta_type = sr_meta_probe(sd, dt, no_chunk);
	if (sd->sd_meta_type == SR_META_F_INVALID) {
		printf("%s: invalid metadata format\n", DEVNAME(sc));
		goto unwind;
	}

	if (sr_meta_attach(sd, no_chunk, bc->bc_flags & BIOC_SCFORCE)) {
		printf("%s: can't attach metadata type %d\n", DEVNAME(sc),
		    sd->sd_meta_type);
		goto unwind;
	}

	/* force the raid volume by clearing metadata region */
	if (bc->bc_flags & BIOC_SCFORCE) {
		/* make sure disk isn't up and running */
		if (sr_meta_read(sd))
			if (sr_already_assembled(sd)) {
				printf("%s: disk ", DEVNAME(sc));
				sr_uuid_print(&sd->sd_meta->ssdi.ssd_uuid, 0);
				printf(" is currently in use; can't force "
				    "create\n");
				goto unwind;
			}

		if (sr_meta_clear(sd)) {
			printf("%s: failed to clear metadata\n", DEVNAME(sc));
			goto unwind;
		}
	}

	no_meta = sr_meta_read(sd);
	if (no_meta == -1) {

		/* Corrupt metadata on one or more chunks. */
		printf("%s: one of the chunks has corrupt metadata; aborting "
		    "assembly\n", DEVNAME(sc));
		goto unwind;

	} else if (no_meta == 0) {

		/* Initialise volume and chunk metadata. */
		sr_meta_init(sd, bc->bc_level, no_chunk);
		sd->sd_vol_status = BIOC_SVONLINE;
		sd->sd_meta_flags = bc->bc_flags & BIOC_SCNOAUTOASSEMBLE;
		if (sd->sd_create) {
			if ((i = sd->sd_create(sd, bc, no_chunk,
			    sd->sd_vol.sv_chunk_minsz))) {
				rv = i;
				goto unwind;
			}
		}
		sr_meta_init_complete(sd);

		DNPRINTF(SR_D_IOCTL,
		    "%s: sr_ioctl_createraid: vol_size: %lld\n",
		    DEVNAME(sc), sd->sd_meta->ssdi.ssd_size);

		/* Warn if we've wasted chunk space due to coercing. */
		if ((sd->sd_capabilities & SR_CAP_NON_COERCED) == 0 &&
		    sd->sd_vol.sv_chunk_minsz != sd->sd_vol.sv_chunk_maxsz)
			printf("%s: chunk sizes are not equal; up to %llu "
			    "blocks wasted per chunk\n", DEVNAME(sc),
			    sd->sd_vol.sv_chunk_maxsz -
			    sd->sd_vol.sv_chunk_minsz);

	} else {

		if (no_meta != no_chunk)
			printf("%s: trying to bring up %s degraded\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);

		if (sd->sd_meta->ssd_meta_flags & SR_META_DIRTY)
			printf("%s: %s was not shutdown properly\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);

		if (user == 0 && sd->sd_meta_flags & BIOC_SCNOAUTOASSEMBLE) {
			DNPRINTF(SR_D_META, "%s: disk not auto assembled from "
			    "metadata\n", DEVNAME(sc));
			goto unwind;
		}

		if (sr_already_assembled(sd)) {
			printf("%s: disk ", DEVNAME(sc));
			sr_uuid_print(&sd->sd_meta->ssdi.ssd_uuid, 0);
			printf(" already assembled\n");
			goto unwind;
		}

		SLIST_FOREACH(omi, &sd->sd_meta_opt, omi_link)
			if (sd->sd_meta_opt_handler == NULL ||
			    sd->sd_meta_opt_handler(sd, omi->omi_som) != 0)
				sr_meta_opt_handler(sd, omi->omi_som);

		if (sd->sd_assemble) {
			if ((i = sd->sd_assemble(sd, bc, no_chunk))) {
				rv = i;
				goto unwind;
			}
		}

		DNPRINTF(SR_D_META, "%s: disk assembled from metadata\n",
		    DEVNAME(sc));

	}

	/* Metadata MUST be fully populated by this point. */

	/* Make sure that metadata level matches requested assembly level. */
	if (sd->sd_meta->ssdi.ssd_level != bc->bc_level) {
		printf("%s: volume level does not match metadata level!\n",
		    DEVNAME(sc));
		goto unwind;
	}

	/* Allocate all resources. */
	if ((rv = sd->sd_alloc_resources(sd)))
		goto unwind;

	/* Adjust flags if necessary. */
	if ((sd->sd_capabilities & SR_CAP_AUTO_ASSEMBLE) &&
	    (bc->bc_flags & BIOC_SCNOAUTOASSEMBLE) !=
	    (sd->sd_meta->ssdi.ssd_vol_flags & BIOC_SCNOAUTOASSEMBLE)) {
		sd->sd_meta->ssdi.ssd_vol_flags &= ~BIOC_SCNOAUTOASSEMBLE;
		sd->sd_meta->ssdi.ssd_vol_flags |=
		    bc->bc_flags & BIOC_SCNOAUTOASSEMBLE;
	}

	if (sd->sd_capabilities & SR_CAP_SYSTEM_DISK) {
		
		/* Initialise volume state. */
		sd->sd_set_vol_state(sd);
		if (sd->sd_vol_status == BIOC_SVOFFLINE) {
			printf("%s: %s offline, will not be brought online\n",
			    DEVNAME(sc), sd->sd_meta->ssd_devname);
			goto unwind;
		}

		/* Setup SCSI iopool. */
		mtx_init(&sd->sd_wu_mtx, IPL_BIO);
		scsi_iopool_init(&sd->sd_iopool, sd, sr_wu_get, sr_wu_put);

		/*
		 * All checks passed - return ENXIO if volume cannot be created.
		 */
		rv = ENXIO;

		/*
		 * Find a free target.
		 *
		 * XXX: We reserve sd_target == 0 to indicate the
		 * discipline is not linked into sc->sc_dis, so begin
		 * the search with target = 1.
		 */
		for (target = 1; target < SR_MAX_LD; target++)
			if (sc->sc_dis[target] == NULL)
				break;
		if (target == SR_MAX_LD) {
			printf("%s: no free target for %s\n", DEVNAME(sc),
			    sd->sd_meta->ssd_devname);
			goto unwind;
		}

		/* Clear sense data. */
		bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));

		/* Attach discipline and get midlayer to probe it. */
		sd->sd_target = target;
		sc->sc_dis[target] = sd;
		if (scsi_probe_lun(sc->sc_scsibus, target, 0) != 0) {
			printf("%s: scsi_probe_lun failed\n", DEVNAME(sc));
			sc->sc_dis[target] = NULL;
			sd->sd_target = 0;
			goto unwind;
		}

		link = scsi_get_link(sc->sc_scsibus, target, 0);
		dev = link->device_softc;
		DNPRINTF(SR_D_IOCTL, "%s: sr device added: %s at target %d\n",
		    DEVNAME(sc), dev->dv_xname, sd->sd_target);

		for (i = 0, vol = -1; i <= sd->sd_target; i++)
			if (sc->sc_dis[i])
				vol++;

		rv = 0;

		if (sd->sd_meta->ssd_devname[0] != '\0' &&
		    strncmp(sd->sd_meta->ssd_devname, dev->dv_xname,
		    sizeof(dev->dv_xname)))
			printf("%s: volume %s is roaming, it used to be %s, "
			    "updating metadata\n", DEVNAME(sc), dev->dv_xname,
			    sd->sd_meta->ssd_devname);

		/* Populate remaining volume metadata. */
		sd->sd_meta->ssdi.ssd_volid = vol;
		strlcpy(sd->sd_meta->ssd_devname, dev->dv_xname,
		    sizeof(sd->sd_meta->ssd_devname));

		/* Update device name on any roaming chunks. */
		sr_roam_chunks(sd);

#ifndef SMALL_KERNEL
		if (sr_sensors_create(sd))
			printf("%s: unable to create sensor for %s\n",
			    DEVNAME(sc), dev->dv_xname);
#endif /* SMALL_KERNEL */
	} else {
		/* This volume does not attach as a system disk. */
		ch_entry = SLIST_FIRST(cl); /* XXX */
		strlcpy(sd->sd_meta->ssd_devname, ch_entry->src_devname,
		    sizeof(sd->sd_meta->ssd_devname));

		if (sd->sd_start_discipline(sd))
			goto unwind;
	}

	/* Save current metadata to disk. */
	rv = sr_meta_save(sd, SR_META_DIRTY);

	if (sd->sd_vol_status == BIOC_SVREBUILD)
		kthread_create_deferred(sr_rebuild, sd);

	sd->sd_ready = 1;

	return (rv);
unwind:
	sr_discipline_shutdown(sd, 0);

	/* XXX - use internal status values! */
	if (rv == EAGAIN)
		rv = 0;

	return (rv);
}

int
sr_ioctl_deleteraid(struct sr_softc *sc, struct bioc_deleteraid *dr)
{
	struct sr_discipline	*sd = NULL;
	int			rv = 1;
	int			i;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_deleteraid %s\n", DEVNAME(sc),
	    dr->bd_dev);

	for (i = 0; i < SR_MAX_LD; i++)
		if (sc->sc_dis[i]) {
			if (!strncmp(sc->sc_dis[i]->sd_meta->ssd_devname,
			    dr->bd_dev,
			    sizeof(sc->sc_dis[i]->sd_meta->ssd_devname))) {
				sd = sc->sc_dis[i];
				break;
			}
		}

	if (sd == NULL)
		goto bad;

	sd->sd_deleted = 1;
	sd->sd_meta->ssdi.ssd_vol_flags = BIOC_SCNOAUTOASSEMBLE;
	sr_discipline_shutdown(sd, 1);

	rv = 0;
bad:
	return (rv);
}

int
sr_ioctl_discipline(struct sr_softc *sc, struct bioc_discipline *bd)
{
	struct sr_discipline	*sd = NULL;
	int			i, rv = 1;

	/* Dispatch a discipline specific ioctl. */

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_discipline %s\n", DEVNAME(sc),
	    bd->bd_dev);

	for (i = 0; i < SR_MAX_LD; i++)
		if (sc->sc_dis[i]) {
			if (!strncmp(sc->sc_dis[i]->sd_meta->ssd_devname,
			    bd->bd_dev,
			    sizeof(sc->sc_dis[i]->sd_meta->ssd_devname))) {
				sd = sc->sc_dis[i];
				break;
			}
		}

	if (sd && sd->sd_ioctl_handler)
		rv = sd->sd_ioctl_handler(sd, bd);

	return (rv);
}

int
sr_ioctl_installboot(struct sr_softc *sc, struct bioc_installboot *bb)
{
	void			*bootblk = NULL, *bootldr = NULL;
	struct sr_discipline	*sd = NULL;
	struct sr_chunk		*chunk;
	struct sr_meta_opt_item *omi;
	struct sr_meta_boot	*sbm;
	struct disk		*dk;
	u_int32_t		bbs, bls;
	u_char			duid[8];
	int			rv = EINVAL;
	int			i;

	DNPRINTF(SR_D_IOCTL, "%s: sr_ioctl_installboot %s\n", DEVNAME(sc),
	    bb->bb_dev);

	for (i = 0; i < SR_MAX_LD; i++)
		if (sc->sc_dis[i]) {
			if (!strncmp(sc->sc_dis[i]->sd_meta->ssd_devname,
			    bb->bb_dev,
			    sizeof(sc->sc_dis[i]->sd_meta->ssd_devname))) {
				sd = sc->sc_dis[i];
				break;
			}
		}

	if (sd == NULL)
		goto done;

	bzero(duid, sizeof(duid));
	TAILQ_FOREACH(dk, &disklist,  dk_link)
		if (!strncmp(dk->dk_name, bb->bb_dev, sizeof(bb->bb_dev)))
			break;
	if (dk == NULL || dk->dk_label == NULL ||
	    (dk->dk_flags & DKF_LABELVALID) == 0 ||
	    bcmp(dk->dk_label->d_uid, &duid, sizeof(duid)) == 0) {
		printf("%s: failed to get DUID for softraid volume!\n",
		    DEVNAME(sd->sd_sc));
		goto done;
	}
	bcopy(dk->dk_label->d_uid, duid, sizeof(duid));

	/* Ensure that boot storage area is large enough. */
	if (sd->sd_meta->ssd_data_offset < (SR_BOOT_OFFSET + SR_BOOT_SIZE)) {
		printf("%s: insufficient boot storage!\n", DEVNAME(sd->sd_sc));
		goto done;
	}

	if (bb->bb_bootblk_size > SR_BOOT_BLOCKS_SIZE * 512)
		goto done;

	if (bb->bb_bootldr_size > SR_BOOT_LOADER_SIZE * 512)
		goto done;

	/* Copy in boot block. */
	bbs = howmany(bb->bb_bootblk_size, DEV_BSIZE) * DEV_BSIZE;
	bootblk = malloc(bbs, M_DEVBUF, M_WAITOK | M_ZERO);
	if (copyin(bb->bb_bootblk, bootblk, bb->bb_bootblk_size) != 0)
		goto done;

	/* Copy in boot loader. */
	bls = howmany(bb->bb_bootldr_size, DEV_BSIZE) * DEV_BSIZE;
	bootldr = malloc(bls, M_DEVBUF, M_WAITOK | M_ZERO);
	if (copyin(bb->bb_bootldr, bootldr, bb->bb_bootldr_size) != 0)
		goto done;

	/* Create or update optional meta for bootable volumes. */
	SLIST_FOREACH(omi, &sd->sd_meta_opt, omi_link)
		if (omi->omi_som->som_type == SR_OPT_BOOT)
			break;
	if (omi == NULL) {
		omi = malloc(sizeof(struct sr_meta_opt_item), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		omi->omi_som = malloc(sizeof(struct sr_meta_crypto), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		omi->omi_som->som_type = SR_OPT_BOOT;
		omi->omi_som->som_length = sizeof(struct sr_meta_boot);
		SLIST_INSERT_HEAD(&sd->sd_meta_opt, omi, omi_link);
		sd->sd_meta->ssdi.ssd_opt_no++;
	}
	sbm = (struct sr_meta_boot *)omi->omi_som;

	bcopy(duid, sbm->sbm_root_duid, sizeof(sbm->sbm_root_duid));
	bzero(&sbm->sbm_boot_duid, sizeof(sbm->sbm_boot_duid));
	sbm->sbm_bootblk_size = bbs;
	sbm->sbm_bootldr_size = bls;

	DNPRINTF(SR_D_IOCTL, "sr_ioctl_installboot: root duid is "
	    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
	    sbm->sbm_root_duid[0], sbm->sbm_root_duid[1],
	    sbm->sbm_root_duid[2], sbm->sbm_root_duid[3],
	    sbm->sbm_root_duid[4], sbm->sbm_root_duid[5],
	    sbm->sbm_root_duid[6], sbm->sbm_root_duid[7]);

	/* Save boot block and boot loader to each chunk. */
	for (i = 0; i < sd->sd_meta->ssdi.ssd_chunk_no; i++) {

		chunk = sd->sd_vol.sv_chunks[i];
		if (chunk->src_meta.scm_status != BIOC_SDONLINE &&
		    chunk->src_meta.scm_status != BIOC_SDREBUILD)
			continue;

		if (i < SR_MAX_BOOT_DISKS)
			bcopy(chunk->src_duid, &sbm->sbm_boot_duid[i],
			    sizeof(sbm->sbm_boot_duid[i]));

		/* Save boot blocks. */
		DNPRINTF(SR_D_IOCTL,
		    "sr_ioctl_installboot: saving boot block to %s "
		    "(%u bytes)\n", chunk->src_devname, bbs);

		if (sr_rw(sc, chunk->src_dev_mm, bootblk, bbs,
		    SR_BOOT_BLOCKS_OFFSET, B_WRITE)) {
			printf("%s: failed to write boot block\n", DEVNAME(sc));
			goto done;
		}

		/* Save boot loader.*/
		DNPRINTF(SR_D_IOCTL,
		    "sr_ioctl_installboot: saving boot loader to %s "
		    "(%u bytes)\n", chunk->src_devname, bls);

		if (sr_rw(sc, chunk->src_dev_mm, bootldr, bls,
		    SR_BOOT_LOADER_OFFSET, B_WRITE)) {
			printf("%s: failed to write boot loader\n",
			   DEVNAME(sc));
			goto done;
		}

	}

	/* XXX - Install boot block on disk - MD code. */

	/* Mark volume as bootable and save metadata. */
	sd->sd_meta->ssdi.ssd_vol_flags |= BIOC_SCBOOTABLE;
	if (sr_meta_save(sd, SR_META_DIRTY)) {
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), chunk->src_devname);
		goto done;
	}

	rv = 0;

done:
	if (bootblk)
		free(bootblk, M_DEVBUF);
	if (bootldr)
		free(bootldr, M_DEVBUF);

	return (rv);
}

void
sr_chunks_unwind(struct sr_softc *sc, struct sr_chunk_head *cl)
{
	struct sr_chunk		*ch_entry, *ch_next;

	DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind\n", DEVNAME(sc));

	if (!cl)
		return;

	for (ch_entry = SLIST_FIRST(cl);
	    ch_entry != SLIST_END(cl); ch_entry = ch_next) {
		ch_next = SLIST_NEXT(ch_entry, src_link);

		DNPRINTF(SR_D_IOCTL, "%s: sr_chunks_unwind closing: %s\n",
		    DEVNAME(sc), ch_entry->src_devname);
		if (ch_entry->src_vn) {
			/*
			 * XXX - explicitly lock the vnode until we can resolve
			 * the problem introduced by vnode aliasing... specfs
			 * has no locking, whereas ufs/ffs does!
			 */
			vn_lock(ch_entry->src_vn, LK_EXCLUSIVE |
			    LK_RETRY, curproc);
			VOP_CLOSE(ch_entry->src_vn, FREAD | FWRITE, NOCRED,
			    curproc);
			vput(ch_entry->src_vn);
		}
		free(ch_entry, M_DEVBUF);
	}
	SLIST_INIT(cl);
}

void
sr_discipline_free(struct sr_discipline *sd)
{
	struct sr_softc		*sc;
	struct sr_meta_opt_head *som;
	struct sr_meta_opt_item	*omi, *omi_next;

	if (!sd)
		return;

	sc = sd->sd_sc;

	DNPRINTF(SR_D_DIS, "%s: sr_discipline_free %s\n",
	    DEVNAME(sc),
	    sd->sd_meta ? sd->sd_meta->ssd_devname : "nodev");
	if (sd->sd_free_resources)
		sd->sd_free_resources(sd);
	if (sd->sd_vol.sv_chunks)
		free(sd->sd_vol.sv_chunks, M_DEVBUF);
	if (sd->sd_meta)
		free(sd->sd_meta, M_DEVBUF);
	if (sd->sd_meta_foreign)
		free(sd->sd_meta_foreign, M_DEVBUF);

	som = &sd->sd_meta_opt;
	for (omi = SLIST_FIRST(som); omi != SLIST_END(som); omi = omi_next) {
		omi_next = SLIST_NEXT(omi, omi_link);
		if (omi->omi_som)
			free(omi->omi_som, M_DEVBUF);
		free(omi, M_DEVBUF);
	}

	if (sd->sd_target != 0) {
		KASSERT(sc->sc_dis[sd->sd_target] == sd);
		sc->sc_dis[sd->sd_target] = NULL;
	}

	explicit_bzero(sd, sizeof *sd);
	free(sd, M_DEVBUF);
}

void
sr_discipline_shutdown(struct sr_discipline *sd, int meta_save)
{
	struct sr_softc		*sc;
	int			s;

	if (!sd)
		return;
	sc = sd->sd_sc;

	DNPRINTF(SR_D_DIS, "%s: sr_discipline_shutdown %s\n", DEVNAME(sc),
	    sd->sd_meta ? sd->sd_meta->ssd_devname : "nodev");

	/* If rebuilding, abort rebuild and drain I/O. */
	if (sd->sd_reb_active) {
		sd->sd_reb_abort = 1;
		while (sd->sd_reb_active)
			tsleep(sd, PWAIT, "sr_shutdown", 1);
	}

	if (meta_save)
		sr_meta_save(sd, 0);

	s = splbio();

	sd->sd_ready = 0;

	/* make sure there isn't a sync pending and yield */
	wakeup(sd);
	while (sd->sd_sync || sd->sd_must_flush)
		if (tsleep(&sd->sd_sync, MAXPRI, "sr_down", 60 * hz) ==
		    EWOULDBLOCK)
			break;

#ifndef SMALL_KERNEL
	sr_sensors_delete(sd);
#endif /* SMALL_KERNEL */

	if (sd->sd_target != 0)
		scsi_detach_lun(sc->sc_scsibus, sd->sd_target, 0, DETACH_FORCE);

	sr_chunks_unwind(sc, &sd->sd_vol.sv_chunk_list);

	if (sd->sd_workq)
		workq_destroy(sd->sd_workq);

	if (sd)
		sr_discipline_free(sd);

	splx(s);
}

int
sr_discipline_init(struct sr_discipline *sd, int level)
{
	int			rv = 1;

	/* Initialise discipline function pointers with defaults. */
	sd->sd_alloc_resources = NULL;
	sd->sd_assemble = NULL;
	sd->sd_create = NULL;
	sd->sd_free_resources = NULL;
	sd->sd_ioctl_handler = NULL;
	sd->sd_openings = NULL;
	sd->sd_meta_opt_handler = NULL;
	sd->sd_scsi_inquiry = sr_raid_inquiry;
	sd->sd_scsi_read_cap = sr_raid_read_cap;
	sd->sd_scsi_tur = sr_raid_tur;
	sd->sd_scsi_req_sense = sr_raid_request_sense;
	sd->sd_scsi_start_stop = sr_raid_start_stop;
	sd->sd_scsi_sync = sr_raid_sync;
	sd->sd_scsi_rw = NULL;
	sd->sd_set_chunk_state = sr_set_chunk_state;
	sd->sd_set_vol_state = sr_set_vol_state;
	sd->sd_start_discipline = NULL;

	switch (level) {
	case 0:
		sr_raid0_discipline_init(sd);
		break;
	case 1:
		sr_raid1_discipline_init(sd);
		break;
	case 4:
		sr_raidp_discipline_init(sd, SR_MD_RAID4);
		break;
	case 5:
		sr_raidp_discipline_init(sd, SR_MD_RAID5);
		break;
	case 6:
		sr_raid6_discipline_init(sd);
		break;
#ifdef AOE
	/* AOE target. */
	case 'A':
		sr_aoe_server_discipline_init(sd);
		break;
	/* AOE initiator. */
	case 'a':
		sr_aoe_discipline_init(sd);
		break;
#endif
#ifdef CRYPTO
	case 'C':
		sr_crypto_discipline_init(sd);
		break;
#endif
	case 'c':
		sr_concat_discipline_init(sd);
		break;
	default:
		goto bad;
	}

	rv = 0;
bad:
	return (rv);
}

int
sr_raid_inquiry(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_inquiry	*cdb = (struct scsi_inquiry *)xs->cmd;
	struct scsi_inquiry_data inq;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_inquiry\n", DEVNAME(sd->sd_sc));

	if (xs->cmdlen != sizeof(*cdb))
		return (EINVAL);

	if (ISSET(cdb->flags, SI_EVPD))
		return (EOPNOTSUPP);

	bzero(&inq, sizeof(inq));
	inq.device = T_DIRECT;
	inq.dev_qual2 = 0;
	inq.version = 2;
	inq.response_format = 2;
	inq.additional_length = 32;
	inq.flags |= SID_CmdQue;
	strlcpy(inq.vendor, sd->sd_meta->ssdi.ssd_vendor,
	    sizeof(inq.vendor));
	strlcpy(inq.product, sd->sd_meta->ssdi.ssd_product,
	    sizeof(inq.product));
	strlcpy(inq.revision, sd->sd_meta->ssdi.ssd_revision,
	    sizeof(inq.revision));
	sr_copy_internal_data(xs, &inq, sizeof(inq));

	return (0);
}

int
sr_raid_read_cap(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_read_cap_data rcd;
	struct scsi_read_cap_data_16 rcd16;
	daddr64_t		addr;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_read_cap\n", DEVNAME(sd->sd_sc));

	addr = sd->sd_meta->ssdi.ssd_size - 1;
	if (xs->cmd->opcode == READ_CAPACITY) {
		bzero(&rcd, sizeof(rcd));
		if (addr > 0xffffffffllu)
			_lto4b(0xffffffff, rcd.addr);
		else
			_lto4b(addr, rcd.addr);
		_lto4b(512, rcd.length);
		sr_copy_internal_data(xs, &rcd, sizeof(rcd));
		rv = 0;
	} else if (xs->cmd->opcode == READ_CAPACITY_16) {
		bzero(&rcd16, sizeof(rcd16));
		_lto8b(addr, rcd16.addr);
		_lto4b(512, rcd16.length);
		sr_copy_internal_data(xs, &rcd16, sizeof(rcd16));
		rv = 0;
	}

	return (rv);
}

int
sr_raid_tur(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_tur\n", DEVNAME(sd->sd_sc));

	if (sd->sd_vol_status == BIOC_SVOFFLINE) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.flags = SKEY_NOT_READY;
		sd->sd_scsi_sense.add_sense_code = 0x04;
		sd->sd_scsi_sense.add_sense_code_qual = 0x11;
		sd->sd_scsi_sense.extra_len = 4;
		return (1);
	} else if (sd->sd_vol_status == BIOC_SVINVALID) {
		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT;
		sd->sd_scsi_sense.flags = SKEY_HARDWARE_ERROR;
		sd->sd_scsi_sense.add_sense_code = 0x05;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		return (1);
	}

	return (0);
}

int
sr_raid_request_sense(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_request_sense\n",
	    DEVNAME(sd->sd_sc));

	/* use latest sense data */
	bcopy(&sd->sd_scsi_sense, &xs->sense, sizeof(xs->sense));

	/* clear sense data */
	bzero(&sd->sd_scsi_sense, sizeof(sd->sd_scsi_sense));

	return (0);
}

int
sr_raid_start_stop(struct sr_workunit *wu)
{
	struct scsi_xfer	*xs = wu->swu_xs;
	struct scsi_start_stop	*ss = (struct scsi_start_stop *)xs->cmd;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_start_stop\n",
	    DEVNAME(wu->swu_dis->sd_sc));

	if (!ss)
		return (1);

	/*
	 * do nothing!
	 * a softraid discipline should always reflect correct status
	 */
	return (0);
}

int
sr_raid_sync(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	int			s, rv = 0, ios;

	DNPRINTF(SR_D_DIS, "%s: sr_raid_sync\n", DEVNAME(sd->sd_sc));

	/* when doing a fake sync don't count the wu */
	ios = wu->swu_fake ? 0 : 1;

	s = splbio();
	sd->sd_sync = 1;

	while (sd->sd_wu_pending > ios)
		if (tsleep(sd, PRIBIO, "sr_sync", 15 * hz) == EWOULDBLOCK) {
			DNPRINTF(SR_D_DIS, "%s: sr_raid_sync timeout\n",
			    DEVNAME(sd->sd_sc));
			rv = 1;
			break;
		}

	sd->sd_sync = 0;
	splx(s);

	wakeup(&sd->sd_sync);

	return (rv);
}

void
sr_startwu_callback(void *arg1, void *arg2)
{
	struct sr_discipline	*sd = arg1;
	struct sr_workunit	*wu = arg2;
	struct sr_ccb		*ccb;
	int			s;

	s = splbio();
	if (wu->swu_cb_active == 1)
		panic("%s: sr_startwu_callback", DEVNAME(sd->sd_sc));
	wu->swu_cb_active = 1;

	TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link)
		VOP_STRATEGY(&ccb->ccb_buf);

	wu->swu_cb_active = 0;
	splx(s);
}

void
sr_raid_startwu(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;

	splassert(IPL_BIO);

	if (wu->swu_state == SR_WU_RESTART)
		/*
		 * no need to put the wu on the pending queue since we
		 * are restarting the io
		 */
		 ;
	else
		/* move wu to pending queue */
		TAILQ_INSERT_TAIL(&sd->sd_wu_pendq, wu, swu_link);

	/* start all individual ios */
	workq_queue_task(sd->sd_workq, &wu->swu_wqt, 0, sr_startwu_callback,
	    sd, wu);
}

void
sr_set_chunk_state(struct sr_discipline *sd, int c, int new_state)
{
	int			old_state, s;

	DNPRINTF(SR_D_STATE, "%s: %s: %s: sr_set_chunk_state %d -> %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    sd->sd_vol.sv_chunks[c]->src_meta.scmi.scm_devname, c, new_state);

	/* ok to go to splbio since this only happens in error path */
	s = splbio();
	old_state = sd->sd_vol.sv_chunks[c]->src_meta.scm_status;

	/* multiple IOs to the same chunk that fail will come through here */
	if (old_state == new_state)
		goto done;

	switch (old_state) {
	case BIOC_SDONLINE:
		if (new_state == BIOC_SDOFFLINE)
			break;
		else
			goto die;
		break;

	case BIOC_SDOFFLINE:
		goto die;

	default:
die:
		splx(s); /* XXX */
		panic("%s: %s: %s: invalid chunk state transition "
		    "%d -> %d\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname,
		    sd->sd_vol.sv_chunks[c]->src_meta.scmi.scm_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol.sv_chunks[c]->src_meta.scm_status = new_state;
	sd->sd_set_vol_state(sd);

	sd->sd_must_flush = 1;
	workq_add_task(NULL, 0, sr_meta_save_callback, sd, NULL);
done:
	splx(s);
}

void
sr_set_vol_state(struct sr_discipline *sd)
{
	int			states[SR_MAX_STATES];
	int			new_state, i, s, nd;
	int			old_state = sd->sd_vol_status;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_set_vol_state\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);

	nd = sd->sd_meta->ssdi.ssd_chunk_no;

	for (i = 0; i < SR_MAX_STATES; i++)
		states[i] = 0;

	for (i = 0; i < nd; i++) {
		s = sd->sd_vol.sv_chunks[i]->src_meta.scm_status;
		if (s >= SR_MAX_STATES)
			panic("%s: %s: %s: invalid chunk state",
			    DEVNAME(sd->sd_sc),
			    sd->sd_meta->ssd_devname,
			    sd->sd_vol.sv_chunks[i]->src_meta.scmi.scm_devname);
		states[s]++;
	}

	if (states[BIOC_SDONLINE] == nd)
		new_state = BIOC_SVONLINE;
	else 
		new_state = BIOC_SVOFFLINE;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_set_vol_state %d -> %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    old_state, new_state);

	switch (old_state) {
	case BIOC_SVONLINE:
		if (new_state == BIOC_SVOFFLINE || new_state == BIOC_SVONLINE)
			break;
		else
			goto die;
		break;

	case BIOC_SVOFFLINE:
		/* XXX this might be a little too much */
		goto die;

	default:
die:
		panic("%s: %s: invalid volume state transition "
		    "%d -> %d\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol_status = new_state;
}

void
sr_checksum_print(u_int8_t *md5)
{
	int			i;

	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
		printf("%02x", md5[i]);
}

void
sr_checksum(struct sr_softc *sc, void *src, void *md5, u_int32_t len)
{
	MD5_CTX			ctx;

	DNPRINTF(SR_D_MISC, "%s: sr_checksum(%p %p %d)\n", DEVNAME(sc), src,
	    md5, len);

	MD5Init(&ctx);
	MD5Update(&ctx, src, len);
	MD5Final(md5, &ctx);
}

void
sr_uuid_get(struct sr_uuid *uuid)
{
	arc4random_buf(uuid->sui_id, sizeof(uuid->sui_id));
	/* UUID version 4: random */
	uuid->sui_id[6] &= 0x0f;
	uuid->sui_id[6] |= 0x40;
	/* RFC4122 variant */
	uuid->sui_id[8] &= 0x3f;
	uuid->sui_id[8] |= 0x80;
}

void
sr_uuid_print(struct sr_uuid *uuid, int cr)
{
	printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	    "%02x%02x%02x%02x%02x%02x",
	    uuid->sui_id[0], uuid->sui_id[1],
	    uuid->sui_id[2], uuid->sui_id[3],
	    uuid->sui_id[4], uuid->sui_id[5],
	    uuid->sui_id[6], uuid->sui_id[7],
	    uuid->sui_id[8], uuid->sui_id[9],
	    uuid->sui_id[10], uuid->sui_id[11],
	    uuid->sui_id[12], uuid->sui_id[13],
	    uuid->sui_id[14], uuid->sui_id[15]);

	if (cr)
		printf("\n");
}

int
sr_already_assembled(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			i;

	for (i = 0; i < SR_MAX_LD; i++)
		if (sc->sc_dis[i])
			if (!bcmp(&sd->sd_meta->ssdi.ssd_uuid,
			    &sc->sc_dis[i]->sd_meta->ssdi.ssd_uuid,
			    sizeof(sd->sd_meta->ssdi.ssd_uuid)))
				return (1);

	return (0);
}

int32_t
sr_validate_stripsize(u_int32_t b)
{
	int			s = 0;

	if (b % 512)
		return (-1);

	while ((b & 1) == 0) {
		b >>= 1;
		s++;
	}

	/* only multiple of twos */
	b >>= 1;
	if (b)
		return(-1);

	return (s);
}

void
sr_shutdownhook(void *arg)
{
	sr_shutdown((struct sr_softc *)arg);
}

void
sr_shutdown(struct sr_softc *sc)
{
	int			i;

	DNPRINTF(SR_D_MISC, "%s: sr_shutdown\n", DEVNAME(sc));

	/* XXX this will not work when we stagger disciplines */
	for (i = 0; i < SR_MAX_LD; i++)
		if (sc->sc_dis[i])
			sr_discipline_shutdown(sc->sc_dis[i], 1);
}

int
sr_validate_io(struct sr_workunit *wu, daddr64_t *blk, char *func)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: %s 0x%02x\n", DEVNAME(sd->sd_sc), func,
	    xs->cmd->opcode);

	if (sd->sd_meta->ssd_data_offset == 0)
		panic("invalid data offset");

	if (sd->sd_vol_status == BIOC_SVOFFLINE) {
		DNPRINTF(SR_D_DIS, "%s: %s device offline\n",
		    DEVNAME(sd->sd_sc), func);
		goto bad;
	}

	if (xs->datalen == 0) {
		printf("%s: %s: illegal block count for %s\n",
		    DEVNAME(sd->sd_sc), func, sd->sd_meta->ssd_devname);
		goto bad;
	}

	if (xs->cmdlen == 10)
		*blk = _4btol(((struct scsi_rw_big *)xs->cmd)->addr);
	else if (xs->cmdlen == 16)
		*blk = _8btol(((struct scsi_rw_16 *)xs->cmd)->addr);
	else if (xs->cmdlen == 6)
		*blk = _3btol(((struct scsi_rw *)xs->cmd)->addr);
	else {
		printf("%s: %s: illegal cmdlen for %s\n",
		    DEVNAME(sd->sd_sc), func, sd->sd_meta->ssd_devname);
		goto bad;
	}

	wu->swu_blk_start = *blk;
	wu->swu_blk_end = *blk + (xs->datalen >> DEV_BSHIFT) - 1;

	if (wu->swu_blk_end > sd->sd_meta->ssdi.ssd_size) {
		DNPRINTF(SR_D_DIS, "%s: %s out of bounds start: %lld "
		    "end: %lld length: %d\n",
		    DEVNAME(sd->sd_sc), func, wu->swu_blk_start,
		    wu->swu_blk_end, xs->datalen);

		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT |
		    SSD_ERRCODE_VALID;
		sd->sd_scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		sd->sd_scsi_sense.add_sense_code = 0x21;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		goto bad;
	}

	rv = 0;
bad:
	return (rv);
}

int
sr_check_io_collision(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_workunit	*wup;

	splassert(IPL_BIO);

	/* walk queue backwards and fill in collider if we have one */
	TAILQ_FOREACH_REVERSE(wup, &sd->sd_wu_pendq, sr_wu_list, swu_link) {
		if (wu->swu_blk_end < wup->swu_blk_start ||
		    wup->swu_blk_end < wu->swu_blk_start)
			continue;

		/* we have an LBA collision, defer wu */
		wu->swu_state = SR_WU_DEFERRED;
		if (wup->swu_collider)
			/* wu is on deferred queue, append to last wu */
			while (wup->swu_collider)
				wup = wup->swu_collider;

		wup->swu_collider = wu;
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu, swu_link);
		sd->sd_wu_collisions++;
		goto queued;
	}

	return (0);
queued:
	return (1);
}

void
sr_rebuild(void *arg)
{
	struct sr_discipline	*sd = arg;
	struct sr_softc		*sc = sd->sd_sc;

	if (kthread_create(sr_rebuild_thread, sd, &sd->sd_background_proc,
	    DEVNAME(sc)) != 0)
		printf("%s: unable to start backgound operation\n",
		    DEVNAME(sc));
}

void
sr_rebuild_thread(void *arg)
{
	struct sr_discipline	*sd = arg;
	struct sr_softc		*sc = sd->sd_sc;
	daddr64_t		whole_blk, partial_blk, blk, sz, lba;
	daddr64_t		psz, rb, restart;
	struct sr_workunit	*wu_r, *wu_w;
	struct scsi_xfer	xs_r, xs_w;
	struct scsi_rw_16	*cr, *cw;
	int			c, s, slept, percent = 0, old_percent = -1;
	u_int8_t		*buf;

	whole_blk = sd->sd_meta->ssdi.ssd_size / SR_REBUILD_IO_SIZE;
	partial_blk = sd->sd_meta->ssdi.ssd_size % SR_REBUILD_IO_SIZE;

	restart = sd->sd_meta->ssd_rebuild / SR_REBUILD_IO_SIZE;
	if (restart > whole_blk) {
		printf("%s: bogus rebuild restart offset, starting from 0\n",
		    DEVNAME(sc));
		restart = 0;
	}
	if (restart) {
		/*
		 * XXX there is a hole here; there is a posibility that we
		 * had a restart however the chunk that was supposed to
		 * be rebuilt is no longer valid; we can reach this situation
		 * when a rebuild is in progress and the box crashes and
		 * on reboot the rebuild chunk is different (like zero'd or
		 * replaced).  We need to check the uuid of the chunk that is
		 * being rebuilt to assert this.
		 */
		psz = sd->sd_meta->ssdi.ssd_size;
		rb = sd->sd_meta->ssd_rebuild;
		if (rb > 0)
			percent = 100 - ((psz * 100 - rb * 100) / psz) - 1;
		else
			percent = 0;
		printf("%s: resuming rebuild on %s at %d%%\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname, percent);
	}

	sd->sd_reb_active = 1;

	/* currently this is 64k therefore we can use dma_alloc */
	buf = dma_alloc(SR_REBUILD_IO_SIZE << DEV_BSHIFT, PR_WAITOK);
	for (blk = restart; blk <= whole_blk; blk++) {
		lba = blk * SR_REBUILD_IO_SIZE;
		sz = SR_REBUILD_IO_SIZE;
		if (blk == whole_blk) {
			if (partial_blk == 0)
				break;
			sz = partial_blk;
		}

		/* get some wu */
		if ((wu_r = scsi_io_get(&sd->sd_iopool, 0)) == NULL)
			panic("%s: rebuild exhausted wu_r", DEVNAME(sc));
		if ((wu_w = scsi_io_get(&sd->sd_iopool, 0)) == NULL)
			panic("%s: rebuild exhausted wu_w", DEVNAME(sc));

		/* setup read io */
		bzero(&xs_r, sizeof xs_r);
		xs_r.error = XS_NOERROR;
		xs_r.flags = SCSI_DATA_IN;
		xs_r.datalen = sz << DEV_BSHIFT;
		xs_r.data = buf;
		xs_r.cmdlen = sizeof(*cr);
		xs_r.cmd = &xs_r.cmdstore;
		cr = (struct scsi_rw_16 *)xs_r.cmd;
		cr->opcode = READ_16;
		_lto4b(sz, cr->length);
		_lto8b(lba, cr->addr);
		wu_r->swu_flags |= SR_WUF_REBUILD;
		wu_r->swu_xs = &xs_r;
		if (sd->sd_scsi_rw(wu_r)) {
			printf("%s: could not create read io\n",
			    DEVNAME(sc));
			goto fail;
		}

		/* setup write io */
		bzero(&xs_w, sizeof xs_w);
		xs_w.error = XS_NOERROR;
		xs_w.flags = SCSI_DATA_OUT;
		xs_w.datalen = sz << DEV_BSHIFT;
		xs_w.data = buf;
		xs_w.cmdlen = sizeof(*cw);
		xs_w.cmd = &xs_w.cmdstore;
		cw = (struct scsi_rw_16 *)xs_w.cmd;
		cw->opcode = WRITE_16;
		_lto4b(sz, cw->length);
		_lto8b(lba, cw->addr);
		wu_w->swu_flags |= SR_WUF_REBUILD;
		wu_w->swu_xs = &xs_w;
		if (sd->sd_scsi_rw(wu_w)) {
			printf("%s: could not create write io\n",
			    DEVNAME(sc));
			goto fail;
		}

		/*
		 * collide with the read io so that we get automatically
		 * started when the read is done
		 */
		wu_w->swu_state = SR_WU_DEFERRED;
		wu_r->swu_collider = wu_w;
		s = splbio();
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu_w, swu_link);

		/* schedule io */
		if (sr_check_io_collision(wu_r))
			goto queued;

		sr_raid_startwu(wu_r);
queued:
		splx(s);

		/* wait for read completion */
		slept = 0;
		while ((wu_w->swu_flags & SR_WUF_REBUILDIOCOMP) == 0) {
			tsleep(wu_w, PRIBIO, "sr_rebuild", 0);
			slept = 1;
		}
		/* yield if we didn't sleep */
		if (slept == 0)
			tsleep(sc, PWAIT, "sr_yield", 1);

		scsi_io_put(&sd->sd_iopool, wu_r);
		scsi_io_put(&sd->sd_iopool, wu_w);

		sd->sd_meta->ssd_rebuild = lba;

		/* save metadata every percent */
		psz = sd->sd_meta->ssdi.ssd_size;
		rb = sd->sd_meta->ssd_rebuild;
		if (rb > 0)
			percent = 100 - ((psz * 100 - rb * 100) / psz) - 1;
		else
			percent = 0;
		if (percent != old_percent && blk != whole_blk) {
			if (sr_meta_save(sd, SR_META_DIRTY))
				printf("%s: could not save metadata to %s\n",
				    DEVNAME(sc), sd->sd_meta->ssd_devname);
			old_percent = percent;
		}

		if (sd->sd_reb_abort)
			goto abort;
	}

	/* all done */
	sd->sd_meta->ssd_rebuild = 0;
	for (c = 0; c < sd->sd_meta->ssdi.ssd_chunk_no; c++)
		if (sd->sd_vol.sv_chunks[c]->src_meta.scm_status ==
		    BIOC_SDREBUILD) {
			sd->sd_set_chunk_state(sd, c, BIOC_SDONLINE);
			break;
		}

abort:
	if (sr_meta_save(sd, SR_META_DIRTY))
		printf("%s: could not save metadata to %s\n",
		    DEVNAME(sc), sd->sd_meta->ssd_devname);
fail:
	dma_free(buf, SR_REBUILD_IO_SIZE << DEV_BSHIFT);
	sd->sd_reb_active = 0;
	kthread_exit(0);
}

#ifndef SMALL_KERNEL
int
sr_sensors_create(struct sr_discipline *sd)
{
	struct sr_softc		*sc = sd->sd_sc;
	int			rv = 1;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_sensors_create\n",
	    DEVNAME(sc), sd->sd_meta->ssd_devname);

	sd->sd_vol.sv_sensor.type = SENSOR_DRIVE;
	sd->sd_vol.sv_sensor.status = SENSOR_S_UNKNOWN;
	strlcpy(sd->sd_vol.sv_sensor.desc, sd->sd_meta->ssd_devname,
	    sizeof(sd->sd_vol.sv_sensor.desc));

	sensor_attach(&sc->sc_sensordev, &sd->sd_vol.sv_sensor);
	sd->sd_vol.sv_sensor_attached = 1;

	if (sc->sc_sensor_task == NULL) {
		sc->sc_sensor_task = sensor_task_register(sc,
		    sr_sensors_refresh, 10);
		if (sc->sc_sensor_task == NULL)
			goto bad;
	}

	rv = 0;
bad:
	return (rv);
}

void
sr_sensors_delete(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_STATE, "%s: sr_sensors_delete\n", DEVNAME(sd->sd_sc));

	if (sd->sd_vol.sv_sensor_attached)
		sensor_detach(&sd->sd_sc->sc_sensordev, &sd->sd_vol.sv_sensor);
}

void
sr_sensors_refresh(void *arg)
{
	struct sr_softc		*sc = arg;
	struct sr_volume	*sv;
	struct sr_discipline	*sd;
	int			i, vol;

	DNPRINTF(SR_D_STATE, "%s: sr_sensors_refresh\n", DEVNAME(sc));

	for (i = 0, vol = -1; i < SR_MAX_LD; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (!sc->sc_dis[i])
			continue;

		sd = sc->sc_dis[i];
		sv = &sd->sd_vol;

		switch(sd->sd_vol_status) {
		case BIOC_SVOFFLINE:
			sv->sv_sensor.value = SENSOR_DRIVE_FAIL;
			sv->sv_sensor.status = SENSOR_S_CRIT;
			break;

		case BIOC_SVDEGRADED:
			sv->sv_sensor.value = SENSOR_DRIVE_PFAIL;
			sv->sv_sensor.status = SENSOR_S_WARN;
			break;

		case BIOC_SVSCRUB:
		case BIOC_SVONLINE:
			sv->sv_sensor.value = SENSOR_DRIVE_ONLINE;
			sv->sv_sensor.status = SENSOR_S_OK;
			break;

		default:
			sv->sv_sensor.value = 0; /* unknown */
			sv->sv_sensor.status = SENSOR_S_UNKNOWN;
		}
	}
}
#endif /* SMALL_KERNEL */

#ifdef SR_FANCY_STATS
void				sr_print_stats(void);

void
sr_print_stats(void)
{
	struct sr_softc		*sc;
	struct sr_discipline	*sd;
	int			i, vol;

	for (i = 0; i < softraid_cd.cd_ndevs; i++)
		if (softraid_cd.cd_devs[i]) {
			sc = softraid_cd.cd_devs[i];
			/* we'll only have one softc */
			break;
		}

	if (!sc) {
		printf("no softraid softc found\n");
		return;
	}

	for (i = 0, vol = -1; i < SR_MAX_LD; i++) {
		/* XXX this will not work when we stagger disciplines */
		if (!sc->sc_dis[i])
			continue;

		sd = sc->sc_dis[i];
		printf("%s: ios pending: %d  collisions %llu\n",
		    sd->sd_meta->ssd_devname,
		    sd->sd_wu_pending,
		    sd->sd_wu_collisions);
	}
}
#endif /* SR_FANCY_STATS */

#ifdef SR_DEBUG
void
sr_meta_print(struct sr_metadata *m)
{
	int			i;
	struct sr_meta_chunk	*mc;
	struct sr_meta_opt_hdr	*omh;

	if (!(sr_debug & SR_D_META))
		return;

	printf("\tssd_magic 0x%llx\n", m->ssdi.ssd_magic);
	printf("\tssd_version %d\n", m->ssdi.ssd_version);
	printf("\tssd_vol_flags 0x%x\n", m->ssdi.ssd_vol_flags);
	printf("\tssd_uuid ");
	sr_uuid_print(&m->ssdi.ssd_uuid, 1);
	printf("\tssd_chunk_no %d\n", m->ssdi.ssd_chunk_no);
	printf("\tssd_chunk_id %d\n", m->ssdi.ssd_chunk_id);
	printf("\tssd_opt_no %d\n", m->ssdi.ssd_opt_no);
	printf("\tssd_volid %d\n", m->ssdi.ssd_volid);
	printf("\tssd_level %d\n", m->ssdi.ssd_level);
	printf("\tssd_size %lld\n", m->ssdi.ssd_size);
	printf("\tssd_devname %s\n", m->ssd_devname);
	printf("\tssd_vendor %s\n", m->ssdi.ssd_vendor);
	printf("\tssd_product %s\n", m->ssdi.ssd_product);
	printf("\tssd_revision %s\n", m->ssdi.ssd_revision);
	printf("\tssd_strip_size %d\n", m->ssdi.ssd_strip_size);
	printf("\tssd_checksum ");
	sr_checksum_print(m->ssd_checksum);
	printf("\n");
	printf("\tssd_meta_flags 0x%x\n", m->ssd_meta_flags);
	printf("\tssd_ondisk %llu\n", m->ssd_ondisk);

	mc = (struct sr_meta_chunk *)(m + 1);
	for (i = 0; i < m->ssdi.ssd_chunk_no; i++, mc++) {
		printf("\t\tscm_volid %d\n", mc->scmi.scm_volid);
		printf("\t\tscm_chunk_id %d\n", mc->scmi.scm_chunk_id);
		printf("\t\tscm_devname %s\n", mc->scmi.scm_devname);
		printf("\t\tscm_size %lld\n", mc->scmi.scm_size);
		printf("\t\tscm_coerced_size %lld\n",mc->scmi.scm_coerced_size);
		printf("\t\tscm_uuid ");
		sr_uuid_print(&mc->scmi.scm_uuid, 1);
		printf("\t\tscm_checksum ");
		sr_checksum_print(mc->scm_checksum);
		printf("\n");
		printf("\t\tscm_status %d\n", mc->scm_status);
	}

	omh = (struct sr_meta_opt_hdr *)((u_int8_t *)(m + 1) +
	    sizeof(struct sr_meta_chunk) * m->ssdi.ssd_chunk_no);
	for (i = 0; i < m->ssdi.ssd_opt_no; i++) {
		printf("\t\t\tsom_type %d\n", omh->som_type);
		printf("\t\t\tsom_checksum ");
		sr_checksum_print(omh->som_checksum);
		printf("\n");
		omh = (struct sr_meta_opt_hdr *)((void *)omh +
		    omh->som_length);
	}
}

void
sr_dump_mem(u_int8_t *p, int len)
{
	int			i;

	for (i = 0; i < len; i++)
		printf("%02x ", *p++);
	printf("\n");
}

#endif /* SR_DEBUG */
