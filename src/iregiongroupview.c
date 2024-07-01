/* coordinate the display of regionviews on imageviews
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
 */
#define DEBUG

#include "nip4.h"

G_DEFINE_TYPE(iRegiongroupview, iregiongroupview, VIEW_TYPE)

static iRegiongroup *
iregiongroupview_get_iregiongroup(iRegiongroupview *iregiongroupview)
{
	return IREGIONGROUP(VOBJECT(iregiongroupview)->iobject);
}

static Classmodel *
iregiongroupview_get_classmodel(iRegiongroupview *iregiongroupview)
{
	iRegiongroup *iregiongroup;

	if ((iregiongroup = iregiongroupview_get_iregiongroup(iregiongroupview)))
		return CLASSMODEL(ICONTAINER(iregiongroup)->parent);

	return NULL;
}

static void *
iregiongroupview_unref(Regionview *regionview)
{
	imageui_remove_regionview(regionview->imageui, regionview);

	return NULL;
}

static void
iregiongroupview_dispose(GObject *object)
{
	iRegiongroupview *iregiongroupview;

#ifdef DEBUG
	printf("iregiongroupview_destroy: %p\n", object);
#endif /*DEBUG*/

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_IREGIONGROUPVIEW(object));

	iregiongroupview = IREGIONGROUPVIEW(object);

	/* Destroy all regionviews we manage.
	 */
	slist_map(iregiongroupview->classmodel->views,
		(SListMapFn) iregiongroupview_unref, NULL);

	G_OBJECT_CLASS(iregiongroupview_parent_class)->dispose(object);
}

/* What we track during a refresh.
 */
typedef struct {
	GSList *notused;
	iRegiongroupview *iregiongroupview;
	Classmodel *classmodel;
	iImage *iimage;
	Imageui *imageui;
} iRegiongroupviewRefreshState;

static Regionview *
iregiongroupview_refresh_imageview_test(Regionview *regionview,
	iRegiongroupviewRefreshState *irs)
{
	if (regionview->classmodel == irs->classmodel &&
		regionview->imageui == irs->imageui)
		return regionview;

	return NULL;
}

static void *
iregiongroupview_refresh_imageview(Imageui *imageui,
	iRegiongroupviewRefreshState *irs)
{
	Regionview *regionview;

#ifdef DEBUG
	printf("iregiongroupview_refresh_imageview: imageui = %p\n", imageui);
#endif /*DEBUG*/

	irs->imageui = imageui;

	/* Do we have a Regionview for this imageui already?
	 */
	if ((regionview = slist_map(irs->notused,
			 (SListMapFn) iregiongroupview_refresh_imageview_test, irs))) {
		/* Yes ... reuse.
		 */
		printf("\treusing old regionview\n");
		irs->notused = g_slist_remove(irs->notused, regionview);
	}
	else {
        /* Nope ... make a new one.
         */
        iRegionInstance *instance = classmodel_get_instance(irs->classmodel);
        PElement *root = &HEAPMODEL(irs->classmodel)->row->expr->root;

		Regionview *regionview = regionview_new(irs->classmodel);

#ifdef DEBUG
		printf("\tcreating new regionview\n");
#endif /*DEBUG*/

		imageui_add_regionview(imageui, regionview);
	}

	return NULL;
}

static void *
iregiongroupview_refresh_iimage(iImage *iimage,
	iRegiongroupviewRefreshState *irs)
{
#ifdef DEBUG
	printf("iregiongroupview_refresh_iimage: iimage = %p\n", iimage);
	printf("\t%d views for this iimage\n", g_slist_length(iimage->views));
#endif /*DEBUG*/

	irs->iimage = iimage;
	slist_map(iimage->views,
		(SListMapFn) iregiongroupview_refresh_imageview, irs);

	return NULL;
}

static void
iregiongroupview_refresh(vObject *vobject)
{
	iRegiongroupview *iregiongroupview = IREGIONGROUPVIEW(vobject);

	iRegiongroupviewRefreshState irs;

#ifdef DEBUG
	printf("iregiongroupview_refresh\n");
	printf("watching model %s %s\n",
		G_OBJECT_TYPE_NAME(vobject->iobject),
		IOBJECT(vobject->iobject)->name);
#endif /*DEBUG*/

	iregiongroupview->classmodel =
		iregiongroupview_get_classmodel(iregiongroupview);

	if (iregiongroupview->classmodel) {
		/* Make a note of all the displays we have now, loop over the
		 * displays we should have, reusing when possible ... remove
		 * any unused displays at the end.
		 */
		irs.classmodel = iregiongroupview->classmodel;
		irs.notused = g_slist_copy(irs.classmodel->views);
		irs.iregiongroupview = iregiongroupview;

		printf("\ttesting %d regions\n", g_slist_length(irs.notused));

		slist_map(irs.classmodel->iimages,
			(SListMapFn) iregiongroupview_refresh_iimage, &irs);

		/* Remove all the regionviews we've not used.
		 */
		printf("\tremoving %d old regions\n", g_slist_length(irs.notused));
		slist_map(irs.notused, (SListMapFn) iregiongroupview_unref, NULL);
		VIPS_FREEF(g_slist_free, irs.notused);
	}

	VOBJECT_CLASS(iregiongroupview_parent_class)->refresh(vobject);
}

static void
iregiongroupview_class_init(iRegiongroupviewClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;
	vObjectClass *vobject_class = (vObjectClass *) class;

	object_class->dispose = iregiongroupview_dispose;

	/* Create signals.
	 */

	/* Init methods.
	 */
	vobject_class->refresh = iregiongroupview_refresh;
}

static void
iregiongroupview_init(iRegiongroupview *iregiongroupview)
{
#ifdef DEBUG
	printf("iregiongroupview_init\n");
#endif /*DEBUG*/
}

View *
iregiongroupview_new(void)
{
	iRegiongroupview *iregiongroupview =
		g_object_new(IREGIONGROUPVIEW_TYPE, NULL);

#ifdef DEBUG
	printf("iregiongroupview_new\n");
#endif /*DEBUG*/

	return VIEW(iregiongroupview);
}
