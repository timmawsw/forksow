/*
Copyright (C) 2017 Victor Luchits
Original copyright (C) 2010-2017 Forest Hale

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cm_bih.c
// 

#include "qcommon.h"
#include "cm_local.h"
#include "bih.h"

bih_t *CM_MakeCollisionBIH(cmodel_state_t *cms, cmodel_t *model, bih_t *out)
{
	int j;
	int bihnumleafs;
	int bihmaxnodes;
	int brushindex;
	int triangleindex;
	int bihleafindex;
	int nummodelbrushes = model->nummarkbrushes;
	int nummodelsurfaces = model->nummarkfaces;
	const int *e;
	const int *collisionelement3i;
	const float *collisionvertex3f;
	const int *renderelement3i;
	const float *rendervertex3f;
	bih_leaf_t *bihleafs;
	bih_node_t *bihnodes;
	int *temp_leafsort;
	int *temp_leafsortscratch;
	const cface_t *surface;
	const cbrush_t *brush;

	// find out how many BIH leaf nodes we need
	bihnumleafs = 0;

	for (brushindex = 0, brush = model->markbrushes; brushindex < nummodelbrushes; brushindex++, brush++)
		bihnumleafs++;
	for (j = 0, surface = model->markfaces; j < nummodelsurfaces; j++, surface++)
	{
		if (surface->numtriangles)
			bihnumleafs += surface->numtriangles;
	}

	if (!bihnumleafs)
		return NULL;

	// allocate the memory for the BIH leaf nodes
	bihleafs = (bih_leaf_t *)Mem_Alloc(cms->mempool, sizeof(bih_leaf_t) * bihnumleafs);

	// now populate the BIH leaf nodes
	bihleafindex = 0;

	// add collision brushes
	for (brushindex = 0, brush = model->markbrushes; brushindex < nummodelbrushes; brushindex++, brush++)
	{
		if (!brush->numsides)
			continue;
		bihleafs[bihleafindex].type = BIH_BRUSH;
		bihleafs[bihleafindex].textureindex = brush->texture - model->data_textures;
		bihleafs[bihleafindex].surfaceindex = -1;
		bihleafs[bihleafindex].itemindex = brushindex + model->firstmodelbrush;
		VectorCopy(brush->colbrushf->mins, bihleafs[bihleafindex].mins);
		VectorCopy(brush->colbrushf->maxs, bihleafs[bihleafindex].maxs);
		bihleafindex++;
	}

	// add collision surfaces
	collisionelement3i = model->brush.data_collisionelement3i;
	collisionvertex3f = model->brush.data_collisionvertex3f;
	for (j = 0, surface = model->data_surfaces + model->firstmodelsurface; j < nummodelsurfaces; j++, surface++)
	{
		for (triangleindex = 0, e = collisionelement3i + 3 * surface->num_firstcollisiontriangle; triangleindex < surface->num_collisiontriangles; triangleindex++, e += 3)
		{
			bihleafs[bihleafindex].type = BIH_COLLISIONTRIANGLE;
			bihleafs[bihleafindex].textureindex = surface->texture - model->data_textures;
			bihleafs[bihleafindex].surfaceindex = surface - model->data_surfaces;
			bihleafs[bihleafindex].itemindex = triangleindex + surface->num_firstcollisiontriangle;
			bihleafs[bihleafindex].mins[0] = min(collisionvertex3f[3 * e[0] + 0], min(collisionvertex3f[3 * e[1] + 0], collisionvertex3f[3 * e[2] + 0])) - 1;
			bihleafs[bihleafindex].mins[1] = min(collisionvertex3f[3 * e[0] + 1], min(collisionvertex3f[3 * e[1] + 1], collisionvertex3f[3 * e[2] + 1])) - 1;
			bihleafs[bihleafindex].mins[2] = min(collisionvertex3f[3 * e[0] + 2], min(collisionvertex3f[3 * e[1] + 2], collisionvertex3f[3 * e[2] + 2])) - 1;
			bihleafs[bihleafindex].maxs[0] = max(collisionvertex3f[3 * e[0] + 0], max(collisionvertex3f[3 * e[1] + 0], collisionvertex3f[3 * e[2] + 0])) + 1;
			bihleafs[bihleafindex].maxs[1] = max(collisionvertex3f[3 * e[0] + 1], max(collisionvertex3f[3 * e[1] + 1], collisionvertex3f[3 * e[2] + 1])) + 1;
			bihleafs[bihleafindex].maxs[2] = max(collisionvertex3f[3 * e[0] + 2], max(collisionvertex3f[3 * e[1] + 2], collisionvertex3f[3 * e[2] + 2])) + 1;
			bihleafindex++;
		}
	}

	// allocate buffers for the produced and temporary data
	bihmaxnodes = bihnumleafs + 1;
	bihnodes = (bih_node_t *)Mem_Alloc(cms->mempool, sizeof(bih_node_t) * bihmaxnodes);
	temp_leafsort = (int *)Mem_Alloc(cms->mempool, sizeof(int) * bihnumleafs * 2);
	temp_leafsortscratch = temp_leafsort + bihnumleafs;

	// now build it
	BIH_Build(out, bihnumleafs, bihleafs, bihmaxnodes, bihnodes, temp_leafsort, temp_leafsortscratch);

	// we're done with the temporary data
	Mem_Free(temp_leafsort);

	// resize the BIH nodes array if it over-allocated
	if (out->maxnodes > out->numnodes)
	{
		out->maxnodes = out->numnodes;
		out->nodes = (bih_node_t *)Mem_Realloc(cms->mempool, out->nodes, out->numnodes * sizeof(bih_node_t));
	}

	return out;
}
