/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/**
 * Where curiosity (attractions) stats are calculated for list dialog
 * @author Hj. Malthaner
 */

#include "curiositylist_stats_t.h"

#include "../display/simgraph.h"
#include "../display/viewport.h"
#include "../simtypes.h"
#include "../simcolor.h"
#include "../simworld.h"
#include "../simhalt.h"
#include "../simskin.h"

#include "../obj/gebaeude.h"

#include "../descriptor/building_desc.h"
#include "../descriptor/skin_desc.h"

#include "../dataobj/translator.h"

#include "../utils/simstring.h"
#include "../utils/cbuffer_t.h"

#include "gui_frame.h"
#include "simwin.h"


curiositylist_stats_t::curiositylist_stats_t(curiositylist::sort_mode_t sortby, bool sortreverse)
{
	get_unique_attractions(sortby,sortreverse);
	recalc_size();
	line_selected = 0xFFFFFFFFu;
}


class compare_curiosities
{
	public:
		compare_curiosities(curiositylist::sort_mode_t sortby_, bool reverse_) :
			sortby(sortby_),
			reverse(reverse_)
		{}

		bool operator ()(const gebaeude_t* a, const gebaeude_t* b)
		{
			int cmp;
			switch (sortby) {
				default: NOT_REACHED
				case curiositylist::by_name:
				{
					const char* a_name = translator::translate(a->get_tile()->get_desc()->get_name());
					const char* b_name = translator::translate(b->get_tile()->get_desc()->get_name());
					cmp = STRICMP(a_name, b_name);
					break;
				}

				case curiositylist::by_paxlevel:
					cmp = a->get_passagier_level() - b->get_passagier_level();
					break;
			}
			return reverse ? cmp > 0 : cmp < 0;
		}

	private:
		curiositylist::sort_mode_t sortby;
		bool reverse;
};


void curiositylist_stats_t::get_unique_attractions(curiositylist::sort_mode_t sb, bool sr)
{
	const weighted_vector_tpl<gebaeude_t*>& world_attractions = welt->get_attractions();

	sortby = sb;
	sortreverse = sr;

	attractions.clear();
	last_world_curiosities = world_attractions.get_count();
	attractions.resize(last_world_curiosities);

	FOR(weighted_vector_tpl<gebaeude_t*>, const geb, world_attractions) {
		if (geb != NULL &&
				geb->get_first_tile() == geb &&
				geb->get_passagier_level() != 0) {
			attractions.insert_ordered( geb, compare_curiosities(sortby, sortreverse) );
		}
	}
}


/**
 * Events werden hiermit an die GUI-Komponenten
 * gemeldet
 * @author Hj. Malthaner
 */
bool curiositylist_stats_t::infowin_event(const event_t * ev)
{
	const unsigned int line = (ev->cy) / (LINESPACE+1);

	line_selected = 0xFFFFFFFFu;
	if (line>=attractions.get_count()) {
		return false;
	}

	gebaeude_t* geb = attractions[line];
	if (geb==NULL) {
		return false;
	}

	// un-press goto button
	if(  ev->button_state>0  &&  ev->cx>0  &&  ev->cx<15  ) {
		line_selected = line;
	}

	if (IS_LEFTRELEASE(ev)) {
		if(  ev->cx>0  &&  ev->cx<15  ) {
			welt->get_viewport()->change_world_position(geb->get_pos());
		}
		else {
			geb->show_info();
		}
	}
	else if (IS_RIGHTRELEASE(ev)) {
		welt->get_viewport()->change_world_position(geb->get_pos());
	}
	return false;
} // end of function curiositylist_stats_t::infowin_event(const event_t * ev)


void curiositylist_stats_t::recalc_size()
{
	// show_scroll_x==false ->> size.w not important ->> no need to calc text pixel length
	set_size( scr_size(210, attractions.get_count() * (LINESPACE+1) ) );
}


/**
 * Draw the component
 * @author Hj. Malthaner
 */
void curiositylist_stats_t::draw(scr_coord offset)
{
	clip_dimension const cd = display_get_clip_wh();
	const int start = cd.y-LINESPACE+1;
	const int end = cd.yy;

	static cbuffer_t buf;
	int yoff = offset.y;

	if(  last_world_curiosities != welt->get_attractions().get_count()  ) {
		// some deleted/ added => resort
		get_unique_attractions( sortby, sortreverse );
		recalc_size();
	}

	uint32 sel = line_selected;
	FORX(vector_tpl<gebaeude_t*>, const geb, attractions, yoff += LINESPACE + 1) {
		if (yoff >= end) {
			break;
		}

		int xoff = offset.x+10;

		// skip invisible lines
		if (yoff < start) {
			continue;
		}

		// goto button
		bool selected = sel==0  ||  welt->get_viewport()->is_on_center( geb->get_pos() );
		display_img_aligned( gui_theme_t::pos_button_img[ selected ], scr_rect( xoff-8, yoff, 14, LINESPACE ), ALIGN_CENTER_V | ALIGN_CENTER_H, true );
		sel --;

		buf.clear();

		// is connected? => decide on indicatorfarbe (indicator color)
		PIXVAL indicatorfarbe;
		bool mail=false;
		bool pax=false;
		bool all_crowded=true;
		bool some_crowded=false;
		const planquadrat_t *plan = welt->access(geb->get_pos().get_2d());
		const halthandle_t *halt_list = plan->get_haltlist();
		for(  unsigned h=0;  (mail&pax)==0  &&  h<plan->get_haltlist_count();  h++ ) {
			halthandle_t halt = halt_list[h];
			if (halt->get_pax_enabled()) {
				pax = true;
				if (halt->get_pax_unhappy() > 40) {
					some_crowded |= true;
				}
				else {
					all_crowded = false;
				}
			}
			if (halt->get_mail_enabled()) {
				mail = true;
				if (halt->get_pax_unhappy() > 40) {
					some_crowded |= true;
				}
				else {
					all_crowded = false;
				}
			}
		}
		// now decide on color
		if(some_crowded) {
			indicatorfarbe = color_idx_to_rgb(all_crowded ? COL_RED : COL_ORANGE);
		}
		else if(pax) {
			indicatorfarbe = color_idx_to_rgb(mail ? COL_TURQUOISE : COL_DARK_GREEN);
		}
		else {
			indicatorfarbe = color_idx_to_rgb(mail ? COL_BLUE : COL_YELLOW);
		}

		display_fillbox_wh_clip_rgb(xoff+7, yoff+2, D_INDICATOR_WIDTH, D_INDICATOR_HEIGHT, indicatorfarbe, true);

		// the other infos
		const unsigned char *name = (const unsigned char *)ltrim( translator::translate(geb->get_tile()->get_desc()->get_name()) );
		char short_name[256];
		char* dst = short_name;
		int    cr = 0;
		for( int j=0;  j<255  &&  cr<10;  j++  ) {
			if(name[j]>0  &&  name[j]<=' ') {
				cr++;
				if(  name[j]<32  ) {
					break;
				}
				if (dst != short_name && dst[-1] != ' ') {
					*dst++ = ' ';
				}
			}
			else {
				*dst++ = name[j];
			}
		}
		*dst = '\0';
		// now we have a short name ...
		buf.printf("%s (%d)", short_name, geb->get_passagier_level());

		display_proportional_clip_rgb(xoff+D_INDICATOR_WIDTH+10+9,yoff,buf,ALIGN_LEFT,SYSCOL_TEXT,true);

		if (geb->get_tile()->get_desc()->get_extra() != 0) {
		    display_color_img(skinverwaltung_t::intown->get_image_id(0), xoff+D_INDICATOR_WIDTH+9, yoff, 0, false, false);
		}
		if(  win_get_magic( (ptrdiff_t)geb )  ) {
			display_blend_wh_rgb( offset.x+D_POS_BUTTON_WIDTH+D_H_SPACE, yoff, size.w, LINESPACE, color_idx_to_rgb(COL_BLACK), 25 );
		}
	}
}
