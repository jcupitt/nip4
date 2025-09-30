/* Links to VipsObject.
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

// the old API with req / opt swapped
void vo_object_new(Reduce *rc, PElement *out, const char *name,
	PElement *required, PElement *optional);
void vo_call(Reduce *rc, PElement *out, const char *name,
	PElement *required, PElement *optional);

void vo_call9(Reduce *rc, PElement *out, const char *name,
	PElement *optional, PElement *required);
void vo_call9_args(Reduce *rc, PElement *out, const char *name,
	HeapNode **args);
void vo_callva(Reduce *rc, PElement *out, const char *name, ...);
