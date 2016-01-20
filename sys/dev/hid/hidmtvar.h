/* $OpenBSD: hidmtvar.h,v 1.1 2016/01/20 01:26:00 jcs Exp $ */
/*
 * Copyright (c) 2016 joshua stein <jcs@openbsd.org>
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

struct hidmt_input {
	int32_t			usage;
	struct hid_location	loc;
	SIMPLEQ_ENTRY(hidmt_input) entry;
};

struct hidmt_contact {
	int		x;
	int		y;
	int		width;
	int		height;
	int		tip;
	int		confidence;
	int		contactid;

	int		seen;
};

struct hidmt {
	int		sc_enabled;
	uint32_t	sc_flags;
#define HIDMT_REVY	0x0001	/* Y-axis is reversed ("natural" scrolling) */

	struct device	*sc_device;
	int		(*hidev_get_report)(struct device *, int, int, void *,
			    int);
	int		(*hidev_set_report)(struct device *, int, int, void *,
			    int);

	int		sc_rep_input;
	int		sc_rep_input_size;
	int		sc_rep_config;
	int		sc_rep_cap;

	SIMPLEQ_HEAD(, hidmt_input) sc_inputs;

	struct device	*sc_wsmousedev;
	int		sc_wsmode;

	int		sc_clickpad;
	int		sc_num_contacts;
#define HIDMT_MAX_CONTACTS	5
	int		sc_minx, sc_maxx;
	int		sc_miny, sc_maxy;

	struct hidmt_contact sc_contacts[HIDMT_MAX_CONTACTS];
	int		sc_button;

	int		last_x, last_y;
};

int	hidmt_set_input_mode(struct hidmt *, int);
#define HIDMT_INPUT_MODE_MT	0x3

void	hidmt_attach(struct hidmt *, const struct wsmouse_accessops *);
int	hidmt_detach(struct hidmt *, int);
void	hidmt_disable(struct hidmt *);
int	hidmt_enable(struct hidmt *);
void	hidmt_input(struct hidmt *, uint8_t *, u_int);
int	hidmt_ioctl(struct hidmt *, u_long, caddr_t, int, struct proc *);
int	hidmt_setup(struct device *, struct hidmt *, void *, int);
