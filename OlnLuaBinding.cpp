/*
* Copyright 2008-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the CrossLine outliner Oln2 library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "LuaBinding.h"
#include "OutlineItem.h"
#include <Udb/Idx.h>
using namespace Oln;
using namespace Udb;

struct _OutlineItem
{
	static int isExpanded(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		lua_pushboolean( L, obj->isExpanded() );
		return 1;
	}
	static int setExpanded(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		obj->setExpanded( lua_toboolean( L, 2 ) );
		return 0;
	}
	static int isTitle(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		lua_pushboolean( L, obj->isTitle() );
		return 1;
	}
	static int setTitle(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		obj->setTitle( lua_toboolean( L, 2 ) );
		obj->setModifiedOn();
		return 0;
	}
	static int isReadOnly(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		lua_pushboolean( L, obj->isReadOnly() );
		return 1;
	}
	static int setReadOnly(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		obj->setReadOnly( lua_toboolean( L, 2 ) );
		return 0;
	}
	static int getHome(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getHome() );
		return 1;
	}
	static int getAlias(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getAlias() );
		return 1;
	}
	static int setAlias(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		OutlineItem* alias = CoBin<OutlineItem>::cast( L, 2 );
		if( alias )
			obj->setAlias( *alias );
		else
			obj->setAlias( Obj() );
		obj->setModifiedOn();
		return 0;
	}
	static int createItem(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		Obj before;
		if( OutlineItem* obj2 = CoBin<OutlineItem>::check( L, 2 ) )
			before = *obj2;
		Udb::LuaBinding::pushObject( L, OutlineItem::createItem( *obj, before ) );
		return 1;
	}
	static int getItems(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		QList<Obj> items;
		Obj sub = obj->getFirstObj();
		if( !sub.isNull() ) do
		{
			if( sub.getType() == OutlineItem::TID )
				items.append(sub);
		}while( sub.next() );
		lua_createtable(L, items.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < items.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, items[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
	static int getNextItem(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getNextItem() );
		return 1;
	}
	static int getPrevItem(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::LuaBinding::pushObject( L, obj->getPrevItem() );
		return 1;
	}
	static int moveUp(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::Obj before = obj->getPrevItem();
		obj->aggregateTo( obj->getParent(), before );
		return 0;
	}
	static int moveDown(lua_State *L)
	{
		OutlineItem* obj = CoBin<OutlineItem>::check( L, 1 );
		Udb::Obj before = obj->getNextItem();
		obj->aggregateTo( obj->getParent(), before );
		return 0;
	}
	static int getReferencingItems(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		bool includeAlias = false;
		if( lua_gettop(L) > 1 )
			includeAlias = lua_toboolean(L, 2);
		QList<OutlineItem> refs = OutlineItem::getReferences( *obj );
		if( includeAlias )
		{
			Udb::Idx idx( obj->getTxn(), OutlineItem::AliasIndex );
			if( !idx.isNull() && idx.seek( *obj ) ) do
			{
				OutlineItem item = obj->getObject( idx.getOid() );
				if( !refs.contains(item) )
					refs.append(item);
			}while( idx.nextKey() );
		}
		lua_createtable(L, refs.size(), 0 );
		const int table = lua_gettop(L);
		for( int i = 0; i < refs.size(); i++ )
		{
			Udb::LuaBinding::pushObject( L, refs[i] );
			lua_rawseti( L, table, i + 1 );
		}
		return 1;
	}
};

static const luaL_reg _OutlineItem_reg[] =
{
	{ "setExpanded", _OutlineItem::setExpanded },
	{ "isExpanded", _OutlineItem::isExpanded },
	{ "isTitle", _OutlineItem::isTitle },
	{ "setTitle", _OutlineItem::setTitle },
	{ "isReadOnly", _OutlineItem::isReadOnly },
	{ "setReadOnly", _OutlineItem::setReadOnly },
	{ "getHome", _OutlineItem::getHome },
	{ "getAlias", _OutlineItem::getAlias },
	{ "setAlias", _OutlineItem::setAlias },
	{ "getNextItem", _OutlineItem::getNextItem },
	{ "getPrevItem", _OutlineItem::getPrevItem },
	{ "moveUp", _OutlineItem::moveUp },
	{ "moveDown", _OutlineItem::moveDown },
	{ 0, 0 }
};

static const luaL_reg _ContentObject_reg[] =
{
	{ "createOutlineItem", _OutlineItem::createItem },
	{ "getOutlineItems", _OutlineItem::getItems },
	{ "getReferencingItems", _OutlineItem::getReferencingItems },
	{ 0, 0 }
};


void Oln::LuaBinding::install(lua_State *L)
{
	CoBin<OutlineItem,ContentObject>::install( L, "OutlineItem", _OutlineItem_reg );
	CoBin<Outline,ContentObject>::install( L, "Outline" );
	CoBin<ContentObject>::addMethods( L, _ContentObject_reg );
}

