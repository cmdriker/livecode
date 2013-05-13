/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"
#include "sysdefs.h"

#include "globals.h"
#include "object.h"
#include "stack.h"
#include "cdata.h"
#include "objptr.h"
#include "field.h"
#include "object.h"
#include "button.h"
#include "card.h"
#include "exec.h"
#include "util.h"
#include "group.h"
#include "image.h"
#include "tilecache.h"

#include "dispatch.h"
#include "parentscript.h"
#include "stacklst.h"
#include "cardlst.h"
#include "redraw.h"
#include "external.h"

#include "exec-interface.h"

//////////

static MCExecEnumTypeElementInfo _kMCInterfaceStackStyleElementInfo[] =
{	
	{ MCmodelessstring, WM_MODELESS, false },
	{ MCpalettestring, WM_PALETTE, false },
	{ MCshadowstring, WM_PALETTE, false },
	{ MCroundrectstring, WM_PALETTE, false },
	{ MCmodalstring, WM_MODAL, false },
	{ MCdialogstring, WM_MODAL, false },
	{ MCmovablestring, WM_MODAL, false },
	{ MCsheetstring, WM_SHEET, false },
	{ MCdrawerstring, WM_DRAWER, true },
	{ MCtoplevelstring, WM_TOP_LEVEL, true },
};

static MCExecEnumTypeInfo _kMCInterfaceStackStyleTypeInfo =
{
	"Interface.StackStyle",
	sizeof(_kMCInterfaceStackStyleElementInfo) / sizeof(MCExecEnumTypeElementInfo),
	_kMCInterfaceStackStyleElementInfo
};

//////////

static MCExecEnumTypeElementInfo _kMCInterfaceCharsetElementInfo[] =
{	
	{ "ISO", 0, true },
	{ "MacOS", 1, true },
};

static MCExecEnumTypeInfo _kMCInterfaceCharsetTypeInfo =
{
	"Interface.Charset",
	sizeof(_kMCInterfaceCharsetElementInfo) / sizeof(MCExecEnumTypeElementInfo),
	_kMCInterfaceCharsetElementInfo
};

//////////

static MCExecEnumTypeElementInfo _kMCInterfaceCompositorTypeElementInfo[] =
{	
	{ "none", kMCTileCacheCompositorNone, false },
	{ "Software", kMCTileCacheCompositorSoftware, false },
	{ "CoreGraphics", kMCTileCacheCompositorCoreGraphics, false },
	{ "opengl", kMCTileCacheCompositorOpenGL, false },
	{ "Static OpenGL", kMCTileCacheCompositorStaticOpenGL, false },
	{ "Dynamic OpenGL", kMCTileCacheCompositorDynamicOpenGL, false },
};

static MCExecEnumTypeInfo _kMCInterfaceCompositorTypeTypeInfo =
{
	"Interface.CompositorType",
	sizeof(_kMCInterfaceCompositorTypeElementInfo) / sizeof(MCExecEnumTypeElementInfo),
	_kMCInterfaceCompositorTypeElementInfo
};

//////////

struct MCInterfaceDecoration
{
	uint2 decoration;
};

static void MCInterfaceDecorationParse(MCExecContext& ctxt, MCStringRef p_input, MCInterfaceDecoration& r_output)
{
}

static void MCInterfaceDecorationFormat(MCExecContext& ctxt, const MCInterfaceDecoration& p_input, MCStringRef& r_output)
{
}

static void MCInterfaceDecorationFree(MCExecContext& ctxt, MCInterfaceDecoration& p_input)
{
}

static MCExecCustomTypeInfo _kMCInterfaceDecorationTypeInfo =
{
	"Interface.TextStyle",
	sizeof(MCInterfaceTextStyle),
	(void *)MCInterfaceDecorationParse,
	(void *)MCInterfaceDecorationFormat,
	(void *)MCInterfaceDecorationFree
};

////////////////////////////////////////////////////////////////////////////////

MCExecEnumTypeInfo *kMCInterfaceStackStyleTypeInfo = &_kMCInterfaceStackStyleTypeInfo;
MCExecEnumTypeInfo *kMCInterfaceCharsetTypeInfo = &_kMCInterfaceCharsetTypeInfo;
MCExecEnumTypeInfo *kMCInterfaceCompositorTypeTypeInfo = &_kMCInterfaceCompositorTypeTypeInfo;
MCExecCustomTypeInfo *kMCInterfaceDecorationTypeInfo = &_kMCInterfaceDecorationTypeInfo;

////////////////////////////////////////////////////////////////////////////////

void MCStack::GetFullscreen(MCExecContext& ctxt, bool &r_setting)
{
	r_setting = getextendedstate(ECS_FULLSCREEN);
}

void MCStack::SetFullscreen(MCExecContext& ctxt, bool setting)
{
	if (getextendedstate(ECS_FULLSCREEN) != setting)
	{
		setextendedstate(setting, ECS_FULLSCREEN);
		
		// MW-2012-10-04: [[ Bug 10436 ]] Use 'setrect' to change the rect
		//   field.
		if (setting)
			old_rect = rect ;
		else if ((old_rect . width > 0) && (old_rect . height > 0))
			setrect(old_rect);
		
		if (opened > 0) 
			reopenwindow();
	}
}

void MCStack::SetName(MCExecContext& ctxt, MCStringRef p_name)
{
	bool t_success;
	t_success = true;

	// MW-2008-10-28: [[ ParentScripts ]] If this stack has its 'has parentscripts'
	//   flag set, temporarily store a copy of the current name.
	MCNewAutoNameRef t_old_name;
	if (getextendedstate(ECS_HAS_PARENTSCRIPTS))
	{
		if (t_success)
			t_success = MCNameClone(getname(), &t_old_name);
	}

	// We don't allow ',' in stack names - so coerce to '_'.
	MCStringFindAndReplaceChar(p_name, ',', '_', kMCCompareExact);

	if (t_success)
	{
		// If the name is going to be empty, coerce to 'Untitled'.
		if (MCStringGetLength(p_name) == 0)
		{
			MCAutoStringRef t_untitled;
			t_success = MCStringCreateWithCString(MCuntitledstring, &t_untitled);
			if (t_success)
				MCObject::SetName(ctxt, *t_untitled);
		}
		else
			MCObject::SetName(ctxt, p_name);
	}

	if (t_success && !ctxt . HasError())
	{
		dirtywindowname();

		// MW-2008-10-28: [[ ParentScripts ]] If there is a copy of the old name, then
		//   it means this stack potentially has parent scripts...
		if (*t_old_name != NULL)
		{
			// If the name has changed process...
			if (!hasname(*t_old_name))
			{
				bool t_is_mainstack;
				t_is_mainstack = MCdispatcher -> ismainstack(this) == True;

				// First flush any references to parentScripts on this stack
				MCParentScript::FlushStack(this);
				setextendedstate(false, ECS_HAS_PARENTSCRIPTS);
			}
		}
	}
}

void MCStack::SetId(MCExecContext& ctxt, uinteger_t p_new_id)
{
	if (p_new_id < obj_id)
	{
		ctxt . LegacyThrow(EE_STACK_BADID);
		return;
	}
	if (obj_id != p_new_id)
	{
		uint4 oldid = obj_id;
		obj_id = (uint4)p_new_id;
		message_with_args(MCM_id_changed, oldid, obj_id);
	}
}

void MCStack::SetVisible(MCExecContext& ctxt, uint32_t part, bool setting)
{
	MCObject::SetVisible(ctxt, part, setting);
	if (opened && (!(state & CS_IGNORE_CLOSE)) )
	{
		if (flags & F_VISIBLE)
		{
			dirtywindowname();
			openwindow(mode >= WM_PULLDOWN);
		}
		else
		{
			MCscreen->closewindow(window);
#ifdef X11
			//x11 will send propertynotify event which call ::close
			state |= CS_ISOPENING;
#endif

		}
		MCscreen->sync(getw());
	}
}

void MCStack::GetNumber(MCExecContext& ctxt, uinteger_t& r_number)
{
	uint2 num;

	if (parent != nil && !MCdispatcher -> ismainstack(this))
	{
		MCStack *sptr;
		sptr = (MCStack *)parent;
		sptr -> count(CT_STACK, CT_UNDEFINED, this, num);
	}
	else
		num = 0;

	r_number = num;
}

void MCStack::GetLayer(MCExecContext& ctxt, integer_t& r_layer)
{
	r_layer = 0;
}

void MCStack::GetFileName(MCExecContext& ctxt, MCStringRef& r_file_name)
{
	if (filename == NULL)
		return;

	if (MCStringCreateWithCString(filename, r_file_name))
		return;

	ctxt . Throw();
}

void MCStack::SetFileName(MCExecContext& ctxt, MCStringRef p_file_name)
{
	delete filename;
	// MW-2007-03-15: [[ Bug 616 ]] Throw an error if you try and set the filename of a substack
	if (!MCdispatcher->ismainstack(this))
	{
		ctxt . LegacyThrow(EE_STACK_NOTMAINSTACK);
		return;
	}
	
	if (p_file_name == nil)
		filename = NULL;
	else
		filename = strclone(MCStringGetCString(p_file_name));
}

void MCStack::GetEffectiveFileName(MCExecContext& ctxt, MCStringRef& r_file_name)
{
	if (!MCdispatcher -> ismainstack(this))
	{
		MCStack *sptr;
		sptr = (MCStack *)parent;
		sptr -> GetEffectiveFileName(ctxt, r_file_name);
		return;
	}

	if (MCStringCreateWithCString(filename, r_file_name))
		return;

	ctxt . Throw();
}

void MCStack::GetSaveCompressed(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = true;
}

void MCStack::SetSaveCompressed(MCExecContext& ctxt, bool setting)
{
	//	NO OP
}

void MCStack::GetCantAbort(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_CANT_ABORT);
}

void MCStack::SetCantAbort(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_CANT_ABORT);
}

void MCStack::GetCantDelete(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_S_CANT_DELETE);
}

void MCStack::SetCantDelete(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_S_CANT_DELETE);
}

void MCStack::GetStyle(MCExecContext& ctxt, intenum_t& r_style)
{
	int style = getstyleint(flags) + WM_TOP_LEVEL_LOCKED;
	
	switch (style)
	{
	case WM_MODELESS:
	case WM_PALETTE:
	case WM_MODAL:
	case WM_SHEET:
	case WM_DRAWER:
		r_style = (intenum_t)style;
		break;
	default:
		r_style = WM_TOP_LEVEL;
		break;
	}
}

void MCStack::SetStyle(MCExecContext& ctxt, intenum_t p_style)
{
	flags &= ~F_STYLE;

	switch (p_style)
	{
	case WM_PALETTE:
		flags |= WM_PALETTE - WM_TOP_LEVEL_LOCKED;
		break;
	case WM_MODELESS:
		flags |= WM_MODELESS - WM_TOP_LEVEL_LOCKED;
		break;
	default:
		flags |= WM_MODAL - WM_TOP_LEVEL_LOCKED;
		break;
	}

	if (opened)
	{
		mode = WM_TOP_LEVEL;
		reopenwindow();
	}
}

void MCStack::GetCantModify(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_CANT_MODIFY);
}

void MCStack::SetCantModify(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_CANT_MODIFY) && opened)
	{
		if (!iskeyed())
		{
			flags ^= F_CANT_MODIFY;
			ctxt . LegacyThrow(EE_STACK_NOKEY);
			return;
		}
		if (mode == WM_TOP_LEVEL || mode == WM_TOP_LEVEL_LOCKED)
			if (flags & F_CANT_MODIFY || !MCdispatcher->cut(True))
				mode = WM_TOP_LEVEL_LOCKED;
			else
				mode = WM_TOP_LEVEL;
		stopedit();
		dirtywindowname();
		resetcursor(True);
		MCstacks->top(this);
	}
}

void MCStack::GetCantPeek(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = true;
}

void MCStack::SetCantPeek(MCExecContext& ctxt, bool setting)
{
	// NO OP
}

void MCStack::GetDynamicPaths(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DYNAMIC_PATHS);
}

void MCStack::SetDynamicPaths(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_DYNAMIC_PATHS);
}

void MCStack::GetDestroyStack(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DESTROY_STACK);
}

void MCStack::SetDestroyStack(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_DESTROY_STACK);
}

void MCStack::GetDestroyWindow(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DESTROY_WINDOW);
}

void MCStack::SetDestroyWindow(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_DESTROY_WINDOW);
}

void MCStack::GetAlwaysBuffer(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_ALWAYS_BUFFER);
}

void MCStack::SetAlwaysBuffer(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_ALWAYS_BUFFER);
}

void MCStack::GetLabel(MCExecContext& ctxt, MCStringRef& r_label)
{
	if (title == nil)
		return;

	MCAutoStringRef t_title;
	if (MCStringCreateWithCString(title, &t_title) && MCU_utf8tonative(*t_title, r_label))
		return;

	ctxt . Throw();
}

void MCStack::SetLabel(MCExecContext& ctxt, MCStringRef p_label)
{
	// MW-2007-07-06: [[ Bug 3226 ]] Updated to take into account 'title' being
	//   stored as a UTF-8 string.
	delete title;
	title = NULL;

	bool t_success;
	t_success = true;

	if (p_label != nil)
	{
		MCAutoStringRef t_title;
		if (t_success)
			t_success = MCU_nativetoutf8(p_label, &t_title);
		if (t_success)
		{
			title = strclone(MCStringGetCString(*t_title));
			flags |= F_TITLE;
		}
	}
	else
		flags &= ~F_TITLE;

	if (t_success)
	{
		dirtywindowname();
		return;
	}

	ctxt . Throw();
}

void MCStack::GetUnicodeLabel(MCExecContext& ctxt, MCStringRef& r_label)
{
	if (title == nil)
		return;

	MCAutoStringRef t_title;
	if (MCStringCreateWithCString(title, &t_title) && MCU_multibytetounicode(*t_title, LCH_UTF8, r_label))
		return;

	ctxt . Throw();
}

void MCStack::SetUnicodeLabel(MCExecContext& ctxt, MCStringRef p_label)
{
	// MW-2007-07-06: [[ Bug 3226 ]] Updated to take into account 'title' being
	//   stored as a UTF-8 string.
	delete title;
	title = NULL;
	
	bool t_success;
	t_success = true;

	if (p_label != nil)
	{
		MCAutoStringRef t_title;
		if (t_success)
			t_success = MCU_unicodetomultibyte(p_label, LCH_UTF8, &t_title);
		if (t_success)
		{
			title = strclone(MCStringGetCString(*t_title));
			flags |= F_TITLE;
		}
	}
	else
		flags &= ~F_TITLE;

	if (t_success)
	{
		dirtywindowname();
		return;
	}

	ctxt . Throw();
}

void MCStack::SetDecoration(Properties which, bool setting)
{
	if (!(flags & F_DECORATIONS))
		decorations = WD_MENU | WD_TITLE | WD_MINIMIZE | WD_MAXIMIZE | WD_CLOSE;
	flags |= F_DECORATIONS;

	uint4 bflags;

	switch (which)
	{
	case P_CLOSE_BOX:
		bflags = WD_CLOSE;
		break;
	case P_COLLAPSE_BOX:
	case P_MINIMIZE_BOX:
		bflags = WD_MINIMIZE;
		break;
	case P_ZOOM_BOX:
	case P_MAXIMIZE_BOX:
		bflags = WD_MAXIMIZE;
		break;
	case P_LIVE_RESIZING:
		bflags = WD_LIVERESIZING;
		break;
	case P_SYSTEM_WINDOW:
		bflags = WD_UTILITY;
		break;
	case P_METAL:
		bflags = WD_METAL;
		break;
	case P_SHADOW:
		setting = !setting;
		bflags = WD_NOSHADOW;
		break;
	default:
		bflags = 0;
		break;
	}
	if (setting)
		decorations |= bflags;
	else
		decorations &= ~bflags;
	if (opened)
		reopenwindow();
}

void MCStack::GetCloseBox(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_CLOSE;
}

void MCStack::SetCloseBox(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_CLOSE_BOX, setting);
}

void MCStack::GetZoomBox(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_MAXIMIZE;
}

void MCStack::SetZoomBox(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_ZOOM_BOX, setting);
}

void MCStack::GetDraggable(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_TITLE;
}

void MCStack::SetDraggable(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_DRAGGABLE, setting);
}

void MCStack::GetCollapseBox(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_TITLE;
}

void MCStack::SetCollapseBox(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_COLLAPSE_BOX, setting);
}

void MCStack::GetLiveResizing(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_LIVERESIZING;
}

void MCStack::SetLiveResizing(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_LIVE_RESIZING, setting);
}

void MCStack::GetSystemWindow(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_UTILITY;
}

void MCStack::SetSystemWindow(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_SYSTEM_WINDOW, setting);
}

void MCStack::GetMetal(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_DECORATIONS) && decorations & WD_METAL;
}

void MCStack::SetMetal(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_METAL, setting);
}

void MCStack::GetShadow(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = !(flags & F_DECORATIONS) && decorations & WD_NOSHADOW;
}

void MCStack::SetShadow(MCExecContext& ctxt, bool setting)
{
	SetDecoration(P_SHADOW, setting);
}

void MCStack::GetResizable(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_RESIZABLE);
}

void MCStack::SetResizable(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_RESIZABLE) && opened)
		reopenwindow();
}

void MCStack::GetMinWidth(MCExecContext& ctxt, uinteger_t& r_width)
{
	r_width = minwidth;
}

void MCStack::SetMinWidth(MCExecContext& ctxt, uinteger_t p_width)
{
	minwidth = p_width;
	if (minwidth > maxwidth)
		maxwidth = minwidth;
	if (opened)
	{
		sethints();
		setgeom();
	}
}

void MCStack::GetMaxWidth(MCExecContext& ctxt, uinteger_t& r_width)
{
	r_width = maxwidth;
}

void MCStack::SetMaxWidth(MCExecContext& ctxt, uinteger_t p_width)
{
	maxwidth = p_width;
	if (minwidth > maxwidth)
		minwidth = maxwidth;
	if (opened)
	{
		sethints();
		setgeom();
	}
}

void MCStack::GetMinHeight(MCExecContext& ctxt, uinteger_t& r_height)
{
	r_height = minheight;
}

void MCStack::SetMinHeight(MCExecContext& ctxt, uinteger_t p_height)
{
	minheight = p_height;
	if (minheight > maxheight)
		maxheight = minheight;
	if (opened)
	{
		sethints();
		setgeom();
	}
}

void MCStack::GetMaxHeight(MCExecContext& ctxt, uinteger_t& r_height)
{
	r_height = maxheight;
}

void MCStack::SetMaxHeight(MCExecContext& ctxt, uinteger_t p_height)
{
	maxheight = p_height;
	if (minheight > maxheight)
		minheight = maxheight;
	if (opened)
	{
		sethints();
		setgeom();
	}
}

void MCStack::GetRecentNames(MCExecContext& ctxt, MCStringRef& r_names)
{
	if (MCrecent -> GetRecent(ctxt, this, P_SHORT_NAME, r_names))
		return;

	ctxt . Throw();
}

void MCStack::GetRecentCards(MCExecContext& ctxt, MCStringRef& r_cards)
{
	if (MCrecent -> GetRecent(ctxt, this, P_LONG_ID, r_cards))
		return;

	ctxt . Throw();
}

void MCStack::GetIconic(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getstate(CS_ICONIC) == True;
}

void MCStack::SetIconic(MCExecContext& ctxt, bool setting)
{
	uint4 newstate = state;
	
	if (setting != ((newstate & CS_ICONIC) == True) && opened)
	{
		if (setting)
			newstate |= CS_ICONIC;
		else
			newstate &= ~CS_ICONIC;

		//SMR 1261 don't set state to allow iconify() to take care of housekeeping
		// need to check X11 to make sure MCStack::iconify() (in stack2.cpp) is called when this prop is set
		sethints();
		if (newstate & CS_ICONIC)
			MCscreen->iconifywindow(window);
		else
			MCscreen->uniconifywindow(window);
	}
}

void MCStack::GetStartUpIconic(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_START_UP_ICONIC) == True;
}

void MCStack::SetStartUpIconic(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_START_UP_ICONIC) && opened)
		sethints();
}

void MCStack::GetIcon(MCExecContext& ctxt, uinteger_t& r_id)
{
	r_id = iconid;
}

void MCStack::SetIcon(MCExecContext& ctxt, uinteger_t p_id)
{
	if (opened && iconid != p_id)
	{
		iconid = p_id;
		if (state & CS_ICONIC)
			redrawicon();
	}
}

void MCStack::GetOwner(MCExecContext& ctxt, MCStringRef& r_owner)
{
	if (parent != nil && !MCdispatcher -> ismainstack(this))
		parent -> GetLongId(ctxt, r_owner);
}

void MCStack::GetMainStack(MCExecContext& ctxt, MCStringRef& r_main_stack)
{
	MCStack *sptr = this;

	if (parent != nil && !MCdispatcher->ismainstack(sptr))
		sptr = (MCStack *)parent;

	r_main_stack = MCValueRetain(MCNameGetString(sptr->getname()));
}

void MCStack::SetMainStack(MCExecContext& ctxt, MCStringRef p_main_stack)
{
	MCStack *stackptr = nil;

	if ((stackptr = MCdispatcher -> findstackname(MCStringGetOldString(p_main_stack))) == nil)
	{
		ctxt . LegacyThrow(EE_STACK_NOMAINSTACK);
		return;
	}

	if (stackptr != this && !MCdispatcher -> ismainstack(stackptr))
	{
		ctxt . LegacyThrow(EE_STACK_NOTMAINSTACK);
		return;
	}
	
	if (parent != nil && this != MCdispatcher -> gethome() && (substacks == nil || stackptr == this))
	{
		bool t_this_is_mainstack;
		t_this_is_mainstack = MCdispatcher -> ismainstack(this) == True;

		// OK-2008-04-10 : Added parameters to mainstackChanged message to specify the new
		// and old mainstack names.
		MCObject *t_old_stackptr;
		if (t_this_is_mainstack)
			t_old_stackptr = this;
		else
			t_old_stackptr = parent;

		//   If this was previously a mainstack, then it will be referenced by (name, NULL).
		//   If this was previously a substack, it will have been referenced by (name, old_mainstack).

		if (t_this_is_mainstack)
			MCdispatcher -> removestack(this);
		else
		{
			MCStack *pstack = (MCStack *)parent;
			remove(pstack -> substacks);
			// MW-2012-09-07: [[ Bug 10372 ]] If the stack no longer has substacks, then 
			//   make sure we undo the extraopen.
			if (pstack -> substacks == nil)
				pstack -> extraclose(true);
		}

		if (stackptr == this)
		{
			MCdispatcher -> appendstack(this);
			parent = MCdispatcher -> gethome();
		}
		else
		{
			// MW-2012-09-07: [[ Bug 10372 ]] If the stack doesn't have substacks, then
			//   make sure we apply the extraopen (as it's about to have some!).
			if (stackptr -> substacks == nil)
				stackptr -> extraopen(true);
			appendto(stackptr -> substacks);
			parent = stackptr;
		}

		// OK-2008-04-10 : Added parameters to mainstackChanged message to specify the new
		// and old mainstack names.
		message_with_valueref_args(MCM_main_stack_changed, t_old_stackptr -> getname(), stackptr -> getname());
	}
	else
	{
		ctxt . LegacyThrow(EE_STACK_CANTSETMAINSTACK);
		return;
	}
}

void MCStack::GetSubstacks(MCExecContext& ctxt, MCStringRef& r_substacks)
{
	if (substacks == nil)
		return;

	bool t_success;
	t_success = true;

	MCAutoListRef t_substacks_list;

	if (t_success)
		t_success = MCListCreateMutable('\n', &t_substacks_list);

	MCStack *sptr = substacks;
	do
	{
		if (t_success)
			t_success = MCListAppend(*t_substacks_list, sptr -> getname());

		sptr = sptr -> next();
	}
	while (t_success && sptr != substacks);

	if (t_success)
		t_success = MCListCopyAsString(*t_substacks_list, r_substacks);

	if (t_success)
		return;
	
	ctxt . Throw();
}

void MCStack::SetSubstacks(MCExecContext& ctxt, MCStringRef p_substacks)
{
	if (p_substacks == nil)
		return;

	if (!MCdispatcher->ismainstack(this))
	{
		ctxt . LegacyThrow(EE_STACK_NOTMAINSTACK);
		return;
	}
	
	bool t_success;
	t_success = true;

	// MW-2012-09-07: [[ Bug 10372 ]] Record the old stack of substackedness so we
	//   can work out later whether we need to extraopen/close.
	bool t_had_substacks;
	t_had_substacks = substacks != nil;

	MCStack *oldsubs = substacks;
	substacks = nil;

	uindex_t t_old_offset;
	t_old_offset = 0;
	uindex_t t_new_offset;
	t_new_offset = 0;

	uindex_t t_length;
	t_length = MCStringGetLength(p_substacks);

	while (t_success && t_old_offset <= t_length)
	{
		MCAutoStringRef t_name_string;
		MCNewAutoNameRef t_name;
		
		if (!MCStringFirstIndexOfChar(p_substacks, '\n', t_old_offset, kMCCompareCaseless, t_new_offset))
			t_new_offset = t_length;

		t_success = MCStringCopySubstring(p_substacks, MCRangeMake(t_old_offset, t_new_offset - t_old_offset), &t_name_string);
		if (t_success && t_new_offset > t_old_offset)
		{
			// If tsub is one of the existing substacks of the stack, it is set to
			// non-null, as it needs to be removed.
			MCStack *tsub = oldsubs;
			if (tsub != nil)
			{
				// Lookup 't_name_string' as a name, if it doesn't exist it can't exist as a substack
				// name.
				&t_name = MCValueRetain(MCNameLookup(*t_name_string));
				if (*t_name != nil)
				{
					while (tsub -> hasname(*t_name))
					{
						tsub = (MCStack *)tsub->nptr;
						if (tsub == oldsubs)
						{
							tsub = nil;
							break;
						}
					}
				}
			}
			// OK-2008-04-10 : Added parameters to mainstackChanged message
			bool t_was_mainstack;
			if (tsub == nil)
			{
				MCStack *toclone = MCdispatcher -> findstackname(MCStringGetOldString(*t_name_string));
				t_was_mainstack = MCdispatcher -> ismainstack(toclone) == True;	

				if (toclone != nil)
					tsub = new MCStack(*toclone);
			}
			else
			{
				// If we are here then it means tsub was found in the current list of
				// substacks of this stack.
				t_was_mainstack = false;
				tsub -> remove(oldsubs);
			}

			if (tsub != nil)
			{
				MCObject *t_old_mainstack;
				if (t_was_mainstack)
					t_old_mainstack = tsub;
				else
					t_old_mainstack = tsub -> getparent();

				tsub -> appendto(substacks);
				tsub -> parent = this;
				tsub -> message_with_valueref_args(MCM_main_stack_changed, t_old_mainstack -> getname(), getname());
			}
			else
				ctxt . LegacyThrow(EE_STACK_BADSUBSTACK);
		}
		t_old_offset = t_new_offset + 1;
	}
	
	if (t_success)
	{
		while (oldsubs != nil)
		{
			MCStack *dsub = (MCStack *)oldsubs->remove(oldsubs);
			delete dsub;
		}

		// MW-2012-09-07: [[ Bug 10372 ]] Make sure we sync the appropriate extraopen/close with
		//   the updated substackness of this stack.
		if (t_had_substacks && substacks == nil)
			extraclose(true);
		else if (!t_had_substacks && substacks != nil)
			extraopen(true);

		// MW-2011-08-17: [[ Redraw ]] This seems a little extreme, but leave as is
		//   for now.
		MCRedrawDirtyScreen();
		return;
	}

	ctxt . Throw();
}

void MCStack::GetGroupProps(MCExecContext& ctxt, Properties which, MCStringRef& r_props)
{
	MCControl *startptr = editing == nil ? controls : savecontrols;
	MCControl *optr = startptr;

	MCAutoListRef t_prop_list;

	bool t_success;
	t_success = true;

	if (t_success)
		t_success = MCListCreateMutable('\n', &t_prop_list);

	if (t_success && optr != nil)
	{
		bool t_want_background;
		t_want_background = which == P_BACKGROUND_NAMES || which == P_BACKGROUND_IDS;
		
		bool t_want_shared;
		t_want_shared = which == P_SHARED_GROUP_NAMES || which == P_SHARED_GROUP_IDS;

		do
		{
			// MW-2011-08-08: [[ Groups ]] Use 'isbackground()' rather than !F_GROUP_ONLY.
			MCGroup *t_group;
			t_group = nil;
			if (optr->gettype() == CT_GROUP)
				t_group = static_cast<MCGroup *>(optr);

			optr = optr -> next();

			if (t_group == nil)
				continue;

			if (t_want_background && !t_group -> isbackground())
				continue;

			if (t_want_shared && !t_group -> isshared())
				continue;

			MCAutoStringRef t_property;

			if (which == P_BACKGROUND_NAMES || which == P_SHARED_GROUP_NAMES)
			{
				t_group -> GetShortName(ctxt, &t_property);
				t_success = !ctxt . HasError();
			}
			else
			{
				uint32_t t_id;
				t_group -> GetId(ctxt, t_id);
				t_success = MCStringFormat(&t_property, "%d", t_id);
			}

			if (t_success)
				t_success = MCListAppend(*t_prop_list, *t_property);
		}
		while (t_success && optr != startptr);
	}

	if (t_success)
		t_success = MCListCopyAsString(*t_prop_list, r_props);

	if (t_success)
		return;

	ctxt . Throw();
}

void MCStack::GetBackgroundNames(MCExecContext& ctxt, MCStringRef& r_names)
{
	GetGroupProps(ctxt, P_BACKGROUND_NAMES, r_names);
}

void MCStack::GetBackgroundIds(MCExecContext& ctxt, MCStringRef& r_ids)
{
	GetGroupProps(ctxt, P_BACKGROUND_IDS, r_ids);
}

void MCStack::GetSharedGroupNames(MCExecContext& ctxt, MCStringRef& r_names)
{
	GetGroupProps(ctxt, P_SHARED_GROUP_NAMES, r_names);
}

void MCStack::GetSharedGroupIds(MCExecContext& ctxt, MCStringRef& r_ids)
{
	GetGroupProps(ctxt, P_SHARED_GROUP_IDS, r_ids);
}

void MCStack::GetCardProps(MCExecContext& ctxt, Properties which, MCStringRef& r_props)
{
	MCAutoListRef t_prop_list;

	bool t_success;
	t_success = true; 

	if (t_success)
		t_success = MCListCreateMutable('\n', &t_prop_list);
	
	if (t_success && cards != nil)
	{
		MCCard *cptr = cards;
		do
		{
			MCAutoStringRef t_property;
			if (which == P_CARD_NAMES)
			{
				cptr -> GetShortName(ctxt, &t_property);
				t_success = !ctxt . HasError();
			}
			else
			{
				uint32_t t_id;
				cptr -> GetId(ctxt, t_id);
				t_success = MCStringFormat(&t_property, "%d", t_id);
			}
			if (t_success)
				t_success = MCListAppend(*t_prop_list, *t_property);

			cptr = cptr -> next();
		}
		while (cptr != cards && t_success);
	}

	if (t_success)
		t_success = MCListCopyAsString(*t_prop_list, r_props);

	if (t_success)
		return;

	ctxt . Throw();
}

void MCStack::GetCardIds(MCExecContext& ctxt, MCStringRef& r_ids)
{
	GetCardProps(ctxt, P_CARD_IDS, r_ids);
}

void MCStack::GetCardNames(MCExecContext& ctxt, MCStringRef& r_names)
{
	GetCardProps(ctxt, P_CARD_NAMES, r_names);
}

void MCStack::GetEditBackground(MCExecContext& ctxt, bool& r_value)
{
	r_value = editing != nil;
}

void MCStack::SetEditBackground(MCExecContext& ctxt, bool p_value)
{
	if (opened)
	{
		if (p_value)
		{
			MCGroup *gptr = (MCGroup *)curcard->getchild(CT_FIRST, kMCEmptyString, CT_GROUP, CT_UNDEFINED);
			if (gptr == nil)
				gptr = getbackground(CT_FIRST, MCnullmcstring, CT_GROUP);
			if (gptr != nil)
				startedit(gptr);
		}
		else
			stopedit();
		dirtywindowname();
		return;
	}

	ctxt . Throw();
}

void MCStack::GetExternals(MCExecContext& ctxt, MCStringRef& r_externals)
{
	if (externalfiles == nil)
		return;

	if (MCStringCreateWithCString(externalfiles, r_externals))
		return;

	ctxt . Throw();
}

void MCStack::SetExternals(MCExecContext& ctxt, MCStringRef p_externals)
{
	delete externalfiles;

	if (p_externals != nil)
		externalfiles = strclone(MCStringGetCString(p_externals));
	else
		externalfiles = NULL;
}

void MCStack::GetExternalCommands(MCExecContext& ctxt, MCStringRef& r_commands)
{
	if (m_externals != nil)
		m_externals -> ListHandlers(HT_MESSAGE, r_commands);
}

void MCStack::GetExternalFunctions(MCExecContext& ctxt, MCStringRef& r_functions)
{
	if (m_externals != nil)
		m_externals -> ListHandlers(HT_FUNCTION, r_functions);
}

void MCStack::GetExternalPackages(MCExecContext& ctxt, MCStringRef& r_externals)
{
	if (m_externals != nil)
		m_externals -> ListExternals(r_externals);
}

void MCStack::GetMode(MCExecContext& ctxt, integer_t& r_mode)
{
	r_mode = getmode();
}

void MCStack::GetWmPlace(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_WM_PLACE) == True;
}

void MCStack::SetWmPlace(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_WM_PLACE);
}

void MCStack::GetWindowId(MCExecContext& ctxt, uinteger_t& r_id)
{
	r_id = MCscreen -> dtouint4(window);
}

void MCStack::GetPixmapId(MCExecContext& ctxt, uinteger_t& r_id)
{
	r_id = 0;
}

void MCStack::GetHcAddressing(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_HC_ADDRESSING) == True;
}

void MCStack::SetHcAddressing(MCExecContext& ctxt, bool setting)
{
	changeflag(setting, F_HC_ADDRESSING);
}

void MCStack::GetHcStack(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_HC_STACK) == True;
}

void MCStack::GetSize(MCExecContext& ctxt, uinteger_t& r_size)
{
	uint4 t_size;
	/* UNCHECKED */ MCU_stoui4(STACK_SIZE, t_size);
	r_size = t_size;
}

void MCStack::GetFreeSize(MCExecContext& ctxt, uinteger_t& r_size)
{
	uint4 t_size;
	/* UNCHECKED */ MCU_stoui4(FREE_SIZE, t_size);
	r_size = t_size;
}

void MCStack::GetLockScreen(MCExecContext& ctxt, bool& r_locked)
{
	// MW-2011-08-18: [[ Redraw ]] Update to use redraw.
	r_locked = MCRedrawIsScreenLocked();
}

void MCStack::SetLockScreen(MCExecContext& ctxt, bool lock)
{
	// MW-2011-08-18: [[ Redraw ]] Update to use redraw.
	if (lock)
		MCRedrawLockScreen();
	else
		MCRedrawUnlockScreenWithEffects();
}

void MCStack::GetStackFiles(MCExecContext& ctxt, MCStringRef& r_files)
{
	bool t_success;
	t_success = true;
	
	MCAutoListRef t_file_list;

	if (t_success)
		t_success = MCListCreateMutable('\n', &t_file_list);

	for (uint2 i = 0; i < nstackfiles; i++)
	{
		MCAutoStringRef t_filename;

		if (t_success)
			t_success = MCStringFormat(&t_filename, "%s,%s", stackfiles[i].stackname, stackfiles[i].filename);

		if (t_success)
			t_success = MCListAppend(*t_file_list, *t_filename);
	}

	if (t_success)
		t_success = MCListCopyAsString(*t_file_list, r_files);

	if (t_success)
		return;

	ctxt . Throw();
}

void MCStack::SetStackFiles(MCExecContext& ctxt, MCStringRef p_files)
{
	while (nstackfiles--)
	{
		delete stackfiles[nstackfiles].stackname;
		delete stackfiles[nstackfiles].filename;
	}
	delete stackfiles;

	MCStackfile *newsf = NULL;
	uint2 nnewsf = 0;

	bool t_success;
	t_success = true;

	uindex_t t_old_offset;
	t_old_offset = 0;
	uindex_t t_new_offset;
	t_new_offset = 0;

	uindex_t t_length;
	t_length = MCStringGetLength(p_files);

	while (t_success && t_old_offset <= t_length)
	{
		MCAutoStringRef t_line;
		
		if (!MCStringFirstIndexOfChar(p_files, '\n', t_old_offset, kMCCompareCaseless, t_new_offset))
			t_new_offset = t_length;

		t_success = MCStringCopySubstring(p_files, MCRangeMake(t_old_offset, t_new_offset - t_old_offset), &t_line);
		if (t_success && t_new_offset > t_old_offset)
		{
			MCAutoStringRef t_stack_name;
			MCAutoStringRef t_file_name;

			t_success = MCStringDivideAtChar(*t_line, ',', kMCCompareExact, &t_stack_name, &t_file_name);

			if (t_success && MCStringGetLength(*t_file_name) != 0)
			{
				MCU_realloc((char **)&newsf, nnewsf, nnewsf + 1, sizeof(MCStackfile));
				newsf[nnewsf].stackname = strclone(MCStringGetCString(*t_stack_name));
				newsf[nnewsf].filename = strclone(MCStringGetCString(*t_file_name));
				nnewsf++;
			}
		}
		t_old_offset = t_new_offset + 1;
	}

	if (t_success)
	{
		stackfiles = newsf;
		nstackfiles = nnewsf;

		if (nstackfiles != 0)
			flags |= F_STACK_FILES;
		else
			flags &= ~F_STACK_FILES;

		return;
	}

	ctxt . Throw();
}

void MCStack::GetMenuBar(MCExecContext& ctxt, MCStringRef& r_menubar)
{
	r_menubar = (MCStringRef)MCValueRetain(getmenubar());
}

void MCStack::SetMenuBar(MCExecContext& ctxt, MCStringRef p_menubar)
{
	bool t_success;
	t_success = true;

	MCNewAutoNameRef t_new_menubar;

	if (t_success)
		t_success = MCNameCreate(p_menubar, &t_new_menubar);

	if (t_success && !MCNameIsEqualTo(getmenubar(), *t_new_menubar, kMCCompareCaseless))
	{
		MCNameDelete(_menubar);
		t_success = MCNameClone(*t_new_menubar, _menubar);
		if (t_success)
		{
			if (!hasmenubar())
				flags &= ~F_MENU_BAR;
			else
				flags |= F_MENU_BAR;
			if (opened)
			{
				setgeom();
				updatemenubar();

				// MW-2011-08-17: [[ Redraw ]] Tell the stack to dirty all of itself.
				dirtyall();
			}
		}
	}

	if (t_success)
		return;

	ctxt . Throw();
}

void MCStack::GetEditMenus(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getstate(CS_EDIT_MENUS) == True;
}

void MCStack::SetEditMenus(MCExecContext& ctxt, bool setting)
{
	if (changestate(setting, CS_EDIT_MENUS) && opened)
	{
		setgeom();
		updatemenubar();
	}
}

void MCStack::GetVScroll(MCExecContext& ctxt, integer_t& r_scroll)
{
	r_scroll = getscroll();
}

void MCStack::GetCharset(MCExecContext& ctxt, intenum_t& r_charset)
{
#ifdef _MACOSX
		r_charset = (state & CS_TRANSLATED) != 0 ? 0 : 1;
#else
		r_charset = (state & CS_TRANSLATED) != 0 ? 1 : 0;
#endif
}

void MCStack::GetFormatForPrinting(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getflag(F_FORMAT_FOR_PRINTING);
}

void MCStack::SetFormatForPrinting(MCExecContext& ctxt, bool setting)
{
	if (changeflag(setting, F_FORMAT_FOR_PRINTING) && opened)
		purgefonts();
}

void MCStack::SetLinkAtt(MCExecContext& ctxt, Properties which, MCInterfaceNamedColor p_color)
{
	if (p_color . name != nil && MCStringGetLength(p_color . name) == 0)
	{
		if (linkatts != nil)
		{
			MCValueRelease(linkatts->colorname);
			MCValueRelease(linkatts->hilitecolorname);
			MCValueRelease(linkatts->visitedcolorname);
			delete linkatts;
			linkatts = nil;
		}
	}
	else
	{
		if (linkatts == nil)
		{
			/* UNCHECKED */ linkatts = new Linkatts;
			MCMemoryCopy(linkatts, &MClinkatts, sizeof(Linkatts));
			linkatts->colorname = MClinkatts.colorname == nil ? nil : MCValueRetain(MClinkatts.colorname);
			linkatts->hilitecolorname = MClinkatts.hilitecolorname == nil ? nil : MCValueRetain(MClinkatts.hilitecolorname);
			linkatts->visitedcolorname = MClinkatts.visitedcolorname == nil ? nil : MCValueRetain(MClinkatts.visitedcolorname);
			MCscreen->alloccolor(linkatts->color);
			MCscreen->alloccolor(linkatts->hilitecolor);
			MCscreen->alloccolor(linkatts->visitedcolor);
		}
		switch (which)
		{
		case P_LINK_COLOR:
			set_interface_color(linkatts->color, linkatts->colorname, p_color);
			break;
		case P_LINK_HILITE_COLOR:
			set_interface_color(linkatts->hilitecolor, linkatts->hilitecolorname, p_color);
			break;
		case P_LINK_VISITED_COLOR:
			set_interface_color(linkatts->visitedcolor, linkatts->visitedcolorname, p_color);
			break;
		default:
			break;
		}
	}
	
	// MW-2011-08-17: [[ Redraw ]] Tell the stack to dirty all of itself.
	dirtyall();
}


void MCStack::GetLinkColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	if (linkatts == nil)
		r_color . name = MCValueRetain(kMCEmptyString);
	else
	{
		Linkatts *la = getlinkatts();
		get_interface_color(la->color, la->colorname, r_color);
	}
}

void MCStack::SetLinkColor(MCExecContext& ctxt, const MCInterfaceNamedColor& p_color)
{
	SetLinkAtt(ctxt, P_LINK_COLOR, p_color);
}

void MCStack::GetEffectiveLinkColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	Linkatts *la = getlinkatts();
	get_interface_color(la->color, la->colorname, r_color);
}

void MCStack::GetLinkHiliteColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	if (linkatts == nil)
		r_color . name = MCValueRetain(kMCEmptyString);
	else
	{
		Linkatts *la = getlinkatts();
		get_interface_color(la->color, la->colorname, r_color);
	}
}

void MCStack::SetLinkHiliteColor(MCExecContext& ctxt, const MCInterfaceNamedColor& p_color)
{
	SetLinkAtt(ctxt, P_LINK_HILITE_COLOR, p_color);
}

void MCStack::GetEffectiveLinkHiliteColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	Linkatts *la = getlinkatts();
	get_interface_color(la->color, la->colorname, r_color);
}

void MCStack::GetLinkVisitedColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	if (linkatts == nil)
		r_color . name = MCValueRetain(kMCEmptyString);
	else
	{
		Linkatts *la = getlinkatts();
		get_interface_color(la->color, la->colorname, r_color);
	}
}

void MCStack::SetLinkVisitedColor(MCExecContext& ctxt, const MCInterfaceNamedColor& p_color)
{
	SetLinkAtt(ctxt, P_LINK_HILITE_COLOR, p_color);
}

void MCStack::GetEffectiveLinkVisitedColor(MCExecContext& ctxt, MCInterfaceNamedColor& r_color)
{
	Linkatts *la = getlinkatts();
	get_interface_color(la->color, la->colorname, r_color);
}

void MCStack::GetUnderlineLinks(MCExecContext& ctxt, bool& r_value)
{
	if (linkatts == nil)
		r_value = false;
	else
	{
		Linkatts *la = getlinkatts();
		r_value = la->underline == True;
	}
}

void MCStack::SetUnderlineLinks(MCExecContext& ctxt, bool p_value)
{
	if (linkatts == NULL)
	{
		linkatts = new Linkatts;
		memcpy(linkatts, &MClinkatts, sizeof(Linkatts));
		linkatts->colorname = (MCStringRef)MCValueRetain(MClinkatts.colorname);
		linkatts->hilitecolorname = (MCStringRef)MCValueRetain(MClinkatts.hilitecolorname);
		linkatts->visitedcolorname = (MCStringRef)MCValueRetain(MClinkatts.visitedcolorname);
		MCscreen->alloccolor(linkatts->color);
		MCscreen->alloccolor(linkatts->hilitecolor);
		MCscreen->alloccolor(linkatts->visitedcolor);
	}

	linkatts->underline = p_value;

	// MW-2011-08-17: [[ Redraw ]] Tell the stack to dirty all of itself.
	dirtyall();
}

void MCStack::GetEffectiveUnderlineLinks(MCExecContext& ctxt, bool& r_value)
{
	Linkatts *la = getlinkatts();
	r_value = la->underline == True;
}

void MCStack::GetWindowShape(MCExecContext& ctxt, uinteger_t& r_shape)
{
	r_shape = windowshapeid;
}

void MCStack::SetWindowShape(MCExecContext& ctxt, uinteger_t p_shape)
{
	// unless we opened the window ourselves, we can't change the window shape
	windowshapeid = p_shape;
	if (windowshapeid)
	{
		// MW-2011-10-08: [[ Bug 4198 ]] Make sure we preserve the shadow status of the stack.
		decorations = WD_SHAPE | (decorations & WD_NOSHADOW);
		flags |= F_DECORATIONS;
		
#if defined(_DESKTOP)
		// MW-2004-04-27: [[Deep Masks]] If a window already has a mask, replace it now to avoid flicker
		if (m_window_shape != NULL)
		{
			MCImage *t_image;
			// MW-2009-02-02: [[ Improved image search ]] Search for the appropriate image object using the standard method.
			t_image = resolveimageid(windowshapeid);
			if (t_image != NULL)
			{
				MCWindowShape *t_new_mask;
				setextendedstate(True, ECS_MASK_CHANGED);
				t_image -> setflag(True, F_I_ALWAYS_BUFFER);
				t_image -> open();
				t_new_mask = t_image -> makewindowshape();
				t_image -> close();
				if (t_new_mask != NULL)
				{
					destroywindowshape();
					m_window_shape = t_new_mask;
					// MW-2011-08-17: [[ Redraw ]] Tell the stack to dirty all of itself.
					dirtyall();
					return;
				}
			}
		}
#endif
	}
	else
	{
		decorations &= ~WD_SHAPE;
		flags &= ~F_DECORATIONS;
	}
	
	if (opened)
	{
		reopenwindow();
		
#if defined(_DESKTOP)
		// MW-2011-08-17: [[ Redraw ]] Tell the stack to dirty all of itself.
		if (m_window_shape != NULL)
			dirtyall();
#endif
	}
}

void MCStack::SetBlendLevel(MCExecContext& ctxt, uinteger_t p_level)
{
	old_blendlevel = blendlevel;
	MCObject::SetBlendLevel(ctxt, p_level);

	// MW-2011-11-03: [[ Bug 9852 ]] Make sure an update is scheduled to sync the
	//   opacity.
	MCRedrawScheduleUpdateForStack(this);
}

void MCStack::GetScreen(MCExecContext& ctxt, integer_t& r_screen)
{
	const MCDisplay *t_display;
	t_display = MCscreen -> getnearestdisplay(rect);
	r_screen = t_display -> index + 1;
}

void MCStack::GetCurrentCard(MCExecContext& ctxt, MCStringRef& r_card)
{
	if (curcard != nil)
		curcard -> GetShortName(ctxt, r_card);
}

void MCStack::SetCurrentCard(MCExecContext& ctxt, MCStringRef p_card)
{
	MCCard *t_card;
	t_card = getchild(CT_EXPRESSION, p_card, CT_CARD);
	if (t_card != NULL)
		setcard(t_card, False, False);
}


void MCStack::GetModifiedMark(MCExecContext& ctxt, bool& r_setting)
{
	r_setting = getextendedstate(ECS_MODIFIED_MARK);
}

void MCStack::SetModifiedMark(MCExecContext& ctxt, bool setting)
{
	if (changeextendedstate(setting, ECS_MODIFIED_MARK) && opened)
		updatemodifiedmark();
}

void MCStack::GetAcceleratedRendering(MCExecContext& ctxt, bool& r_value)
{
	r_value = getacceleratedrendering();
}

void MCStack::SetAcceleratedRendering(MCExecContext& ctxt, bool p_value)
{
	setacceleratedrendering(p_value);
}

void MCStack::GetCompositorType(MCExecContext& ctxt, intenum_t*& r_type)
{
	if (m_tilecache == nil)
	{
		r_type = nil;
		return;
	}

	intenum_t t_type;
	t_type = (intenum_t)MCTileCacheGetCompositor(m_tilecache);
	r_type = &t_type;
}

void MCStack::SetCompositorType(MCExecContext& ctxt, intenum_t* p_type)
{
	MCTileCacheCompositorType t_type;

	if (p_type == nil)
		t_type = kMCTileCacheCompositorNone;
	else if (*p_type == kMCTileCacheCompositorOpenGL)
	{
		if (MCTileCacheSupportsCompositor(kMCTileCacheCompositorDynamicOpenGL))
			t_type = kMCTileCacheCompositorDynamicOpenGL;
		else
			t_type = kMCTileCacheCompositorStaticOpenGL;
	}
	else
		t_type = (MCTileCacheCompositorType)*p_type;

	if (!MCTileCacheSupportsCompositor(t_type))
	{
		ctxt . LegacyThrow(EE_COMPOSITOR_NOTSUPPORTED);
		return;
	}

	if (t_type == kMCTileCacheCompositorNone)
	{
		MCTileCacheDestroy(m_tilecache);
		m_tilecache = nil;
	}
	else
	{
		if (m_tilecache == nil)
		{
			MCTileCacheCreate(32, 4096 * 1024, m_tilecache);
			MCTileCacheSetViewport(m_tilecache, curcard -> getrect());
		}
	
		MCTileCacheSetCompositor(m_tilecache, t_type);
	}
	
	dirtyall();
}

void MCStack::GetDeferScreenUpdates(MCExecContext& ctxt, bool& r_value)
{
	r_value = m_defer_updates;
}

void MCStack::SetDeferScreenUpdates(MCExecContext& ctxt, bool p_value)
{
	m_defer_updates = p_value;
}

void MCStack::GetEffectiveDeferScreenUpdates(MCExecContext& ctxt, bool& r_value)
{
	r_value = m_defer_updates && m_tilecache != nil;
}

/*
void MCStack::Set: uint/GetCompositorTileSize: optional-uint
void MCStack::Set: uint/GetCompositorCacheLimit: optional-uint
*/