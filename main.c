/*-
 * Copyright (c) 2002-2011  Benedikt Meurer <benedikt.meurer@googlemail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/select.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/lkm.h>

#include "fibovar.h"


/* init routine */
int fibo_lkmentry(struct lkm_table *, int, int);

/* handle routine */
static int fibo_handle(struct lkm_table *, int);

/* device routines */
static int fibo_open(dev_t, int, int, struct proc *);
static int fibo_close(dev_t, int, int, struct proc *);
static int fibo_read(dev_t dev, struct uio *, int);

/* device struct */
static struct cdevsw fibo_dev = {
	fibo_open, fibo_close,
	fibo_read, (dev_type_write((*))) enodev,
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, 0,
	(dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev, 0
};

/* module reference counter */
static int fibo_refcnt = 0;

/* minor devices */
static struct fibo_softc fibo_scs[MAXFIBODEVS];

#if (__NetBSD_Version__ >= 106080000)
MOD_DEV("fibo", "fibo", NULL, -1, &fibo_dev, -1);
#else
MOD_DEV("fibo", LM_DT_CHAR, -1, &fibo_dev);
#endif

int
fibo_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, fibo_handle, fibo_handle, fibo_handle);
}

static int
fibo_handle(struct lkm_table *lkmtp, int cmd)
{
	switch (cmd) {
	case LKM_E_LOAD:
		if (lkmexists(lkmtp))
			return (EEXIST);

		bzero(fibo_scs, sizeof(fibo_scs));
		printf("fibo: FIBONACCI driver loaded successfully\n");
		break;

	case LKM_E_UNLOAD:
		if (fibo_refcnt > 0)
			return (EBUSY);
		break;

	case LKM_E_STAT:
		break;

	default:
		return (EIO);
	}

	return (0);
}

static int
fibo_open(dev_t dev, int flag, int mode, struct proc *p)
{
	struct fibo_softc *fibosc = (fibo_scs + minor(dev));

	if (minor(dev) >= MAXFIBODEVS)
		return (ENODEV);

	/* check if device already open */
	if (fibosc->sc_refcnt > 0)
		return (EBUSY);

	fibosc->sc_current = 1;
	fibosc->sc_previous = 0;

	/* increase device reference counter */
	fibosc->sc_refcnt++;

	/* increase module reference counter */
	fibo_refcnt++;

	return (0);
}

static int
fibo_close(dev_t dev, int flag, int mode, struct proc *p)
{
	struct fibo_softc *fibosc = (fibo_scs + minor(dev));

	/* decrease device reference counter */
	fibosc->sc_refcnt--;

	/* decrease module reference counter */
	fibo_refcnt--;

	return (0);
}

static int
fibo_read(dev_t dev, struct uio *uio, int flag)
{
	struct fibo_softc *fibosc = (fibo_scs + minor(dev));

	if (uio->uio_resid < sizeof(u_int32_t))
		return (EINVAL);

	while (uio->uio_resid >= sizeof(u_int32_t)) {
		int error;

		/* copy to user space */
		if ((error = uiomove(&(fibosc->sc_current),
		    sizeof(fibosc->sc_current), uio))) {
			return (error);
		}

		/* prevent overflow */
		if (fibosc->sc_current > (MAXFIBONUM - 1)) {
			fibosc->sc_current = 1;
			fibosc->sc_previous = 0;
			continue;
		}

		/* calculate */ {
			u_int32_t tmp;

			tmp = fibosc->sc_current;
			fibosc->sc_current += fibosc->sc_previous;
			fibosc->sc_previous = tmp;
		}
	}

	return (0);
}


